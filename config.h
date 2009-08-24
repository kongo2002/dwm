/* See LICENSE file for copyright and license details. */

/* appearance */
static const char font[]            = "-*-proggycleanszcp-medium-r-normal-*-13-*-*-*-*-*-*-*";
static const char normbordercolor[] = "#000000";
static const char normbgcolor[]     = "#000000";
static const char normfgcolor[]     = "#eeeeee";
static const char selbordercolor[]  = "#333333";
static const char selbgcolor[]      = "#000000";
static const char selfgcolor[]      = "#ff6600";
static const unsigned int borderpx  = 5;        /* border pixel of windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const Bool showbar           = True;     /* False means no bar */
static const Bool topbar            = True;     /* False means bottom bar */
static const Bool usegrab           = False;    /* True means grabbing the X server
                                                   during mouse-based resizals */

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
    /* class      instance    title       tags mask     isfloating  monitor */
    { "Gimp",     NULL,       NULL,       0,            True,       -1 },
    { "jEdit",    NULL,       NULL,       0,            True,       -1 },
    { "Pidgin",   NULL,       NULL,       1 << 7,       True,       -1 },
    { "Firefox",  NULL,       NULL,       1 << 8,       False,      -1 },
    { "ut2004-bin",   NULL,   NULL,       1 << 6,       True,       -1 },
};

/* layout(s) */
static const float mfact      = 0.55; /* factor of master area size [0.05..0.95] */
static const Bool resizehints = True; /* False means respect size hints in tiled resizals */

/* bottom stack layout function */
void
bstack(Monitor *m) {
    int x, y, h, w, mh;
    unsigned int i, n;
    Client *c;

    for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);

    if (n == 0)
        return;

    c = nexttiled(m->clients);
    mh = m->mfact * m->wh;
    resize(c, m->wx, m->wy, m->ww - 2 * c->bw, (n == 1 ? m->wh : mh) - 2 * c->bw, False);

    if (--n == 0)
        return;

    x = m->wx;
    y = (m->wy + mh > c->y + c->h) ? c->y + c->h + 2 * c->bw : m->wy + mh;
    w = m->ww / n;
    h = (m->wy + mh > c->y + c->h) ? m->wy + m->wh - y : m->wh -  mh;

    if (h < bh)
        h = m->wh;

    for (i = 0, c = nexttiled(c->next); c; c = nexttiled(c->next), i++) {
        resize(c, x, y, ((i + 1 == n) ? m->wx + m->ww - x : w) - 2 * c->bw, 
                h - 2 * c->bw, False);

        if (w != m->ww)
            x = c->x + WIDTH(c);
    }
}

static const Layout layouts[] = {
    /* symbol     arrange function */
    { "[]=",      tile },    /* first entry is default */
    { "><>",      NULL },    /* no layout function means floating behavior */
    { "[M]",      monocle },
    { "TTT",      bstack },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
    { MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
    { MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
    { MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
    { MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* custom functions */
static Client *
prevtiled(Client *c) {
    Client *p, *r;

    for(p = selmon->clients, r = NULL; p && p != c; p = p->next)
        if(!p->isfloating && ISVISIBLE(p))
            r = p;
    return r;
}

static void
pushup(const Arg *arg) {
    Client *sel = selmon->sel;
    Client *c;

    if(!sel || sel->isfloating)
        return;
    if((c = prevtiled(sel))) {
        /* attach before c */
        detach(sel);
        sel->next = c;
        if(selmon->clients == c)
            selmon->clients = sel;
        else {
            for(c = selmon->clients; c->next != sel->next; c = c->next);
            c->next = sel;
        }
    } else {
        /* move to the end */
        for(c = sel; c->next; c = c->next);
        detach(sel);
        sel->next = NULL;
        c->next = sel;
    }
    focus(sel);
    arrange();
}

static void
pushdown(const Arg *arg) {
    Client *sel = selmon->sel;
    Client *c;

    if(!sel || sel->isfloating)
        return;
    if((c = nexttiled(sel->next))) {
        /* attach after c */
        detach(sel);
        sel->next = c->next;
        c->next = sel;
    } else {
        /* move to the front */
        detach(sel);
        attach(sel);
    }
    focus(sel);
    arrange();
}

static void
nexttag(const Arg *arg) {
    unsigned int seld = 0, i = 0;

    for (i=0; i<LENGTH(tags); ++i) {
        if (selmon->tagset[selmon->seltags] & (1 << i))
            ++seld;
    }
    if (seld != 1)
        return;
    if (selmon->tagset[selmon->seltags] & (1 << (LENGTH(tags)-1)))
        selmon->tagset[selmon->seltags] = 1;
    else
        selmon->tagset[selmon->seltags] = selmon->tagset[selmon->seltags] << 1;
    arrange();
}

static void
prevtag(const Arg *arg) {
    unsigned int seld = 0, i = 0;

    for (i=0; i<LENGTH(tags); ++i) {
        if (selmon->tagset[selmon->seltags] & (1 << i))
            ++seld;
    }
    if (seld != 1)
        return;
    if (selmon->tagset[selmon->seltags] & 1)
        selmon->tagset[selmon->seltags] = (1 << (LENGTH(tags)-1));
    else
        selmon->tagset[selmon->seltags] = selmon->tagset[selmon->seltags] >> 1;
    arrange();
}

/* commands */
static const char *termcmd[]  = { "urxvt", NULL };
static const char *browsercmd[] = { "firefox", NULL };
static const char *mailcmd[] = { "thunderbird", NULL };
static const char *launchercmd[] = { "gmrun", NULL };
static const char *mpdcmd[] = { "mpc", "next", NULL };
static const char *editorcmd[] = { "gvim", NULL };
static const char *uzblcmd[] = { "uzbl", NULL };

/* key definitions */
static Key keys[] = {
    /* modifier                     key        function        argument */
    { MODKEY,                       XK_p,      spawn,          {.v = launchercmd } },
    { MODKEY|ShiftMask,             XK_Return, spawn,          {.v = termcmd } },
    { MODKEY,                       XK_f,      spawn,          {.v = browsercmd } },
    { MODKEY,                       XK_v,      spawn,          {.v = editorcmd } },
    { MODKEY,                       XK_n,      spawn,          {.v = mpdcmd } },
    { MODKEY,                       XK_t,      spawn,          {.v = mailcmd } },
    { MODKEY,                       XK_u,      spawn,          {.v = uzblcmd } },
    { MODKEY,                       XK_b,      togglebar,      {0} },
    { MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
    { MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
    { MODKEY|ControlMask,           XK_j,      pushdown,       {0} },
    { MODKEY|ControlMask,           XK_k,      pushup,         {0} },
    { MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },
    { MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },
    { MODKEY,                       XK_Return, zoom,           {0} },
    { MODKEY,                       XK_Tab,    view,           {0} },
    { MODKEY,                       XK_Escape, killclient,     {0} },
    { MODKEY|ControlMask,           XK_t,      setlayout,      {.v = &layouts[0]} },
    { MODKEY|ControlMask,           XK_f,      setlayout,      {.v = &layouts[1]} },
    { MODKEY|ControlMask,           XK_m,      setlayout,      {.v = &layouts[2]} },
    { MODKEY|ControlMask,           XK_b,      setlayout,      {.v = &layouts[3]} },
    { MODKEY,                       XK_space,  setlayout,      {0} },
    { MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
    { MODKEY,                       XK_0,      view,           {.ui = ~0 } },
    { MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
    { MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
    { MODKEY,                       XK_period, focusmon,       {.i = +1 } },
    { MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
    { MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
    TAGKEYS(                        XK_1,                      0)
    TAGKEYS(                        XK_2,                      1)
    TAGKEYS(                        XK_3,                      2)
    TAGKEYS(                        XK_4,                      3)
    TAGKEYS(                        XK_5,                      4)
    TAGKEYS(                        XK_6,                      5)
    TAGKEYS(                        XK_7,                      6)
    TAGKEYS(                        XK_8,                      7)
    TAGKEYS(                        XK_9,                      8)
    { MODKEY|ShiftMask,             XK_q,      quit,           {0} },
    { MODKEY,                       XK_Right,  nexttag,        {0} },
    { MODKEY,                       XK_Left,   prevtag,        {0} },
};

/* button definitions */
/* click can be a tag number (starting at 0),
 * ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
    /* click                event mask      button          function        argument */
    { ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
    { ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
    { ClkWinTitle,          0,              Button2,        zoom,           {0} },
    { ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
    { ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
    { ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
    { ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
    { ClkTagBar,            0,              Button1,        view,           {0} },
    { ClkTagBar,            0,              Button3,        toggleview,     {0} },
    { ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
    { ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};

