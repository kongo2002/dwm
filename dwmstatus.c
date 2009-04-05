/* status bar script as a replacement for xsetroot -name "`foo`"
 * by Gregor Uhlenheuer */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <sys/sysinfo.h>
#include <sys/statfs.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <libmpdclient.h>

char text[1024];

static struct sysinfo info;
static struct statfs fs;

static unsigned long cpu0_total, cpu0_active, cpu1_total, cpu1_active = 0;
static unsigned long net_transmit, net_receive = 0;

/* runs a (shell) command and returns the output in text[] */
/*
int run_cmd(const char *command)
{
    int pd[2], l=0, r=0, p=0;

    if (pipe(pd) == -1)
        return 0;

    if ((p = fork()) == 0)
    {
        close(pd[0]);
        close(2);
        close(0);
        dup2(pd[1], 1);
        execlp("/bin/sh", "/bin/sh", "-c", command, NULL);
        perror("exec failed");
        exit(1);
    }
    else if (p == 1)
        return 0;

    close(pd[1]);
    waitpid(p, NULL, 0);

    while ((r = read(pd[0], text+l, 1024-l)) > 0)
        l += r;

    close(pd[0]);

    text[l] = 0;
    
    return 1;
}
*/

/*
int get_mpd(char *status)
{
    char *tmp = text;
    int i = 0;

    if (!run_cmd("mpc status"))
        return 0;

    if (strncmp(text, "volume:", 7) == 0)
        return 0;

    while (*tmp != '\n')
    {
        if (i >= 127)
            break;
        i++;
        tmp++;
    }
    *tmp = '\0';

    int a, b, c, d;
    sscanf(++tmp, "[playing] #%*d/%*d %d:%d/%d:%d", &a, &b, &c, &d);

    sprintf(status, "%s | %s [%d:%.2d/%d:%.2d]", status, text, a, b, c, d);

    return 1;
}
*/

int get_mpd2(char *status)
{
    mpd_Connection *conn;
    conn = mpd_newConnection("127.0.0.1", 6600, 10);

    if (conn->error)
    {
        mpd_closeConnection(conn);
        return 0;
    }

    mpd_Status *state;
    mpd_InfoEntity *entity;

    mpd_sendCommandListOkBegin(conn);
    mpd_sendStatusCommand(conn);
    mpd_sendCurrentSongCommand(conn);
    mpd_sendCommandListEnd(conn);

    if ((state = mpd_getStatus(conn)) == NULL)
    {
        mpd_closeConnection(conn);
        return 0;
    }

    if (state->state != MPD_STATUS_STATE_PLAY && 
        state->state != MPD_STATUS_STATE_PAUSE)
    {
        mpd_closeConnection(conn);
        return 0;
    }

    mpd_nextListOkCommand(conn);

    entity = mpd_getNextInfoEntity(conn);

    mpd_Song *song = entity->info.song;

    if (entity->type == MPD_INFO_ENTITY_TYPE_SONG)
    {
        if (song->artist && song->title)
            sprintf(status, "%s | %s - %s [%d:%.2d/%d:%.2d]", 
                    status, 
                    song->artist, 
                    song->title,
                    state->elapsedTime / 60,
                    state->elapsedTime % 60,
                    state->totalTime / 60,
                    state->totalTime % 60);

        mpd_freeInfoEntity(entity);
    }

    if (conn->error)
    {
        mpd_closeConnection(conn);
        return 0;
    }

    mpd_finishCommand(conn);
    if (conn->error)
    {
        mpd_closeConnection(conn);
        return 0;
    }

    mpd_freeStatus(state);
    mpd_closeConnection(conn);

    return 1;
} 

/*
int get_fs(char *status)
{
    if (!run_cmd("df -h /dev/sda4"))
        return 0;

    char *tmp = text;
    int used, total, perc = 0;

    while (*tmp != '\n')
        ++tmp;

    sscanf(++tmp, "/dev/sda4 %dG %dG %*dG %d%%", &total, &used, &perc);

    sprintf(status, "%s | %d%% (%dG/%dG)", status, perc, used, total);

    return 1;
}
*/

int get_fs2(char *status)
{
    unsigned long avail, total, used, free = 0;

    total = fs.f_blocks * fs.f_bsize / 1024 / 1024 / 1024;
    free = fs.f_bfree * fs.f_bsize / 1024 / 1024 / 1024;
    used = total - free;

    sprintf(status, "%s | %.0f%% (%dG/%dG)", 
                    status,
                    (float) used / total * 100,
                    (int) used,
                    (int) total);

    return 1;
}

int get_cpu(char *status)
{
    char buffer[256];
    FILE *cpu_fp;
    unsigned long total_new[4];
    unsigned long total, active = 0;
    unsigned long diff_total, diff_active = 0;
    float cpu0, cpu1;

    cpu_fp = fopen("/proc/stat", "r");

    if (cpu_fp == NULL)
        return 0;

    while (!feof(cpu_fp))
    {
        if (fgets(buffer, 256, cpu_fp) == NULL)
            break;

        if (strncmp(buffer, "cpu0", 4) == 0)
        {
            sscanf(buffer, "%*s %lu %lu %lu %lu", 
                           &total_new[0], 
                           &total_new[1], 
                           &total_new[2], 
                           &total_new[3]);

            total = total_new[0] + total_new[1] + total_new[2] + total_new[3];
            active = total - total_new[3];

            diff_total = total - cpu0_total;
            diff_active = active - cpu0_active;

            cpu0 = (float) diff_active / diff_total * 100;

            cpu0_total = total;
            cpu0_active = active;
        }
        else if (strncmp(buffer, "cpu1", 4) == 0)
        {
            sscanf(buffer, "%*s %lu %lu %lu %lu", 
                           &total_new[0], 
                           &total_new[1], 
                           &total_new[2], 
                           &total_new[3]);

            total = total_new[0] + total_new[1] + total_new[2] + total_new[3];
            active = total - total_new[3];

            diff_total = total - cpu1_total;
            diff_active = active - cpu1_active;

            cpu1 = (float) diff_active / diff_total * 100;

            cpu1_total = total;
            cpu1_active = active;

            break;
        }
    }

    fclose(cpu_fp);

    sprintf(status, "%s | %.0f%% %.0f%%", status, cpu0, cpu1);

    return 1;
}

int get_net(char *status)
{
    char buffer[256];
    FILE *net_fp;
    unsigned long receive, transmit = 0;
    unsigned long diff_receive, diff_transmit = 0;

    net_fp = fopen("/proc/net/dev", "r");

    if (net_fp == NULL)
        return 0;

    while (!feof(net_fp))
    {
        if (fgets(buffer, 256, net_fp) == NULL)
            break;

        /* skip whitespace */
        char *tmp = buffer;
        while (*tmp == ' ')
            ++tmp;

        if (strncmp(tmp, "eth0:", 5) == 0)
        {
            sscanf(tmp, 
                   "eth0:%lu  %*d     %*d  %*d  %*d  %*d   %*d        %*d       %lu",
                   &receive, &transmit);

            /* check for overflow */
            if (net_receive > receive)
                net_receive = 0;
            if (net_transmit > transmit)
                net_transmit = 0;

            diff_receive = receive - net_receive;
            diff_transmit = transmit - net_transmit;

            net_receive = receive;
            net_transmit = transmit;

            break;
        }
    }

    fclose(net_fp);

    sprintf(status, "%s | %.f %.f", 
                    status, 
                    (float)diff_receive/1024, 
                    (float)diff_transmit/1024);

    return 1;
}

int get_mem(char *status)
{
    char buffer[256];
    FILE *mem_fp;
    unsigned long memtotal, memfree, buffers, cached, meminuse = 0;

    mem_fp = fopen("/proc/meminfo", "r");

    if (mem_fp == NULL)
        return 0;

    while (!feof(mem_fp))
    {
        if (fgets(buffer, 255, mem_fp) == NULL)
            break;

        if (strncmp(buffer, "MemTotal:", 9) == 0) 
            sscanf(buffer, "%*s %lu", &memtotal);
        else if (strncmp(buffer, "MemFree:", 8) == 0) 
            sscanf(buffer, "%*s %lu", &memfree);
        else if (strncmp(buffer, "Buffers:", 8) == 0) 
            sscanf(buffer, "%*s %lu", &buffers);
        else if (strncmp(buffer, "Cached:", 7) == 0) 
        {
            sscanf(buffer, "%*s %lu", &cached);
            break;
        }
    }

    fclose(mem_fp);

    memfree = memfree + buffers + cached;
    meminuse = memtotal - memfree;

    sprintf(status, "%s | %.0f%% (%dM/%dM)", 
                    status, 
                    (float)meminuse/memtotal*100, 
                    (int)meminuse/1024, 
                    (int)memtotal/1024);

    return 1;
}

int get_time(char *status)
{
    time_t timestamp;
    struct tm *now;

    timestamp = time(NULL);
    now = localtime(&timestamp);

    sprintf(status, "%s | %d:%.2d %.2d.%.2d.%.2d", 
                    status, 
                    now->tm_hour, 
                    now->tm_min, 
                    now->tm_mday, 
                    now->tm_mon+1, 
                    now->tm_year+1900-2000);

    return 1;
}

int get_procs(char *status)
{
    unsigned short int procs, procs_run = 0;
    char buffer[256];
    FILE *proc_fp;

    proc_fp = fopen("/proc/stat", "r");

    if (proc_fp == NULL)
        return 0;

    while (!feof(proc_fp))
    {
        if (fgets(buffer, 255, proc_fp) == 0)
            break;

        if (strncmp(buffer, "procs_running ", 14) == 0)
        {
            sscanf(buffer, "%*s %hu", &procs_run);
            break;
        }
    }

    fclose(proc_fp);

    procs = info.procs;

    sprintf(status, "%s | %hu/%hu", status, procs_run, procs);

    return 1;
}   

int get_uptime(char *status)
{
    unsigned short int hours, minutes;

    hours = info.uptime / 3600;
    minutes = (info.uptime % 3600) / 60;

    sprintf(status, "%s | %huh%hu", status, hours, minutes);

    return 1;
}

int main(int argc, char **argv)
{
    Display *disp;
    Window root;
    char statusbar[1024];

    if (!(disp = XOpenDisplay(0)))
    {
        fprintf(stderr, "%s: cannot open display\n", argv[0]);
        return 0;
    }

    root = DefaultRootWindow(disp);

    while (1)
    {
        strcpy(statusbar, " ");

        sysinfo(&info);
        statfs("/home", &fs);

        get_cpu(statusbar);
        get_procs(statusbar);
        get_fs2(statusbar);
        get_mem(statusbar);
        get_net(statusbar);
        get_mpd2(statusbar);
        get_uptime(statusbar);
        get_time(statusbar);

        //printf("%s\n", statusbar); 
        
        XStoreName(disp, root, statusbar);
        XFlush(disp);

        sleep(1);
    }

    return 1;
}
