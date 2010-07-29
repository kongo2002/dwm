/* status bar script as a replacement for xsetroot -name "`foo`"
 * by Gregor Uhlenheuer
 *
 * compile with:
 * gcc dwmstatus.c -o dwmstatus -lX11 `pkg-config --libs libmpdclient` -O2
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

#include <sys/sysinfo.h>
#include <sys/statfs.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <mpd/client.h>

static struct sysinfo info;
static struct statfs fs;

static unsigned long cpu0_total = 0, cpu0_active = 0, cpu1_total = 0, cpu1_active = 0;
static unsigned long net_transmit = 0, net_receive = 0;

static int mpd_error_exit(struct mpd_connection *conn)
{
#ifdef DEBUG
    assert(mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS);

    fprintf(stderr, "error: %s\n", mpd_connection_get_error_message(conn));
#endif

    mpd_connection_free(conn);
    return 0;
}

int get_mpd(char *status)
{
    const char *artist, *title;
    unsigned int duration;

    struct mpd_connection *conn;
    struct mpd_status *state;
    struct mpd_song *song;
    enum mpd_tag_type tag_type;

    conn = mpd_connection_new("127.0.0.1", 6600, 10);

    if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS)
        return mpd_error_exit(conn);

    if (!mpd_command_list_begin(conn, true) ||
            !mpd_send_status(conn) ||
            !mpd_send_current_song(conn) ||
            !mpd_command_list_end(conn))
        return mpd_error_exit(conn);

    state = mpd_recv_status(conn);

    if (state == NULL)
        return mpd_error_exit(conn);

    if (mpd_status_get_state(state) == MPD_STATE_PLAY ||
            mpd_status_get_state(state) == MPD_STATE_PAUSE)
    {
        if (!mpd_response_next(conn))
            return mpd_error_exit(conn);

        song = mpd_recv_song(conn);

        if (song != NULL)
        {
            tag_type = mpd_tag_name_iparse("artist");
            artist = mpd_song_get_tag(song, tag_type, 0);

            tag_type = mpd_tag_name_iparse("title");
            title = mpd_song_get_tag(song, tag_type, 0);

            duration = mpd_song_get_duration(song);

            sprintf(status, "%s | %s - %s [%d:%02d]",
                    status,
                    artist,
                    title,
                    duration / 60,
                    duration % 60);
#ifdef DEBUG
            printf("%s - %s [%d:%02d]\n",
                    artist,
                    title,
                    duration / 60,
                    duration % 60);
#endif

            mpd_song_free(song);
        }
    }

    mpd_status_free(state);

    if (!mpd_response_finish(conn))
        return mpd_error_exit(conn);

    mpd_connection_free(conn);

    return 1;
}

int get_fs(char *status)
{
    unsigned long total, used, free = 0;

    total = fs.f_blocks * fs.f_bsize / 1024 / 1024 / 1024;
    free = fs.f_bfree * fs.f_bsize / 1024 / 1024 / 1024;
    used = total - free;

    sprintf(status, "%s | %.0f%% (%dG/%dG)",
                    status,
                    (float) used / total * 100,
                    (int) used,
                    (int) total);

#ifdef DEBUG
    printf("FS: %.0f%% (%dG/%dG)\n",
            (float) used / total * 100,
            (int) used,
            (int) total);
#endif

    return 1;
}

int get_cpu(char *status)
{
    char buffer[256];
    FILE *cpu_fp;
    unsigned long total_new[4];
    unsigned long total, active = 0;
    unsigned long diff_total, diff_active = 0;
    float cpu0 = 0, cpu1 = 0;

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

#ifdef DEBUG
    printf("CPU: %.0f%% %.0f%%\n", cpu0, cpu1);
#endif

    return 1;
}

int get_net(char *status)
{
    char buffer[256];
    FILE *net_fp = NULL;
    unsigned long receive = 0, transmit = 0;
    unsigned long diff_receive = 0, diff_transmit = 0;

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

#ifdef DEBUG
    printf("NET: %.f %.f\n",
            (float)diff_receive/1024,
            (float)diff_transmit/1024);
#endif

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

#ifdef DEBUG
    printf("MEM: %.0f%% (%dM/%dM)\n",
            (float)meminuse/memtotal*100,
            (int)meminuse/1024,
            (int)memtotal/1024);
#endif

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

#ifdef DEBUG
    printf("TIME: %d:%.2d %.2d.%.2d.%.2d\n",
            now->tm_hour,
            now->tm_min,
            now->tm_mday,
            now->tm_mon+1,
            now->tm_year+1900-2000);
#endif

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

#ifdef DEBUG
    printf("PROC: %hu/%hu\n", procs_run, procs);
#endif

    return 1;
}

int get_uptime(char *status)
{
    unsigned short int hours, minutes;

    hours = info.uptime / 3600;
    minutes = (info.uptime % 3600) / 60;

    sprintf(status, "%s | %huh%.2hu", status, hours, minutes);

#ifdef DEBUG
    printf("UPTIME: %huh%.2hu\n", hours, minutes);
#endif

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
        get_fs(statusbar);
        get_mem(statusbar);
        get_net(statusbar);
        get_mpd(statusbar);
        get_uptime(statusbar);
        get_time(statusbar);

        XStoreName(disp, root, statusbar);
        XFlush(disp);

        sleep(1);
    }

    return 1;
}
