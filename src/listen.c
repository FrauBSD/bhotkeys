/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * XRecord listener.  Sees KeyPress while another client holds
 * XGrabKeyboard.  Does not eat the event.  No functional hotkeys
 * live here: a match runs the plugin command.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/record.h>

#include "bhotkeys.h"

#define REPEAT_INITIAL_MS	280
#define REPEAT_RATE_MS		30

static volatile sig_atomic_t running = 1;
static Display *dpy_data;
static int lock_fd = -1;
static int super_down;
static KeySym held_ks = NoSymbol;
static KeyCode held_code = 0;
static int held_repeat;
static struct timeval held_since, last_fire;
static struct bh_set *live;
static int alt_down;
static KeyCode up_code;
static Time up_time;
static int up_pending;
static struct timeval up_when;
static int inflight = -1;

static void
on_signal(int sig __unused)
{
	running = 0;
	_exit(0);
}

static long
tv_diff_ms(const struct timeval *a, const struct timeval *b)
{
	return (a->tv_sec - b->tv_sec) * 1000L +
	    (a->tv_usec - b->tv_usec) / 1000L;
}

static int
singleton_lock(void)
{
	const char *dir;
	char path[256];
	int fd;

	if (bh_greeter) {
		strlcpy(path, "/tmp/bhotkeys-greeter.lock", sizeof(path));
	} else {
		dir = getenv("XDG_RUNTIME_DIR");
		if (dir == NULL || dir[0] == '\0')
			dir = "/tmp";
		if (snprintf(path, sizeof(path), "%s/bhotkeys.lock", dir) >=
		    (int)sizeof(path))
			return (-1);
	}
	fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	if (fd < 0)
		return (-1);
	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
		close(fd);
		if (!bh_greeter)
			return (-1);
		unlink(path);
		fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
		if (fd < 0)
			return (-1);
		if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
			close(fd);
			return (-1);
		}
	}
	lock_fd = fd;
	return (0);
}

static void
track_super(KeySym ks, int pressed)
{
	if (ks != XK_Super_L && ks != XK_Super_R)
		return;
	if (pressed) {
		if (super_down < 8)
			super_down++;
	} else if (super_down > 0) {
		super_down--;
	}
}

static void
track_alt(KeySym ks, int pressed)
{
	if (ks != XK_Alt_L && ks != XK_Alt_R && ks != XK_Meta_L &&
	    ks != XK_Meta_R)
		return;
	if (pressed) {
		if (alt_down < 8)
			alt_down++;
	} else if (alt_down > 0)
		alt_down--;
}

/*
 * SuperMenuArm opens Start on Super release. A chord has to mark
 * itself first, or the menu appears when Super comes up.
 */
static void
cancel_super_menu(void)
{
	const char *home;
	char dir[512], path[512], pidbuf[32];
	FILE *fp;
	pid_t pid;

	if (bh_greeter)
		return;
	home = getenv("HOME");
	if (home == NULL)
		return;
	if (snprintf(dir, sizeof(dir), "%s/.cache/super-menu-chord", home) >=
	    (int)sizeof(dir))
		return;
	(void)mkdir(dir, 0700);
	if (snprintf(path, sizeof(path),
	    "%s/chord", dir) < (int)sizeof(path)) {
		fp = fopen(path, "w");
		if (fp != NULL)
			fclose(fp);
	}
	if (snprintf(path, sizeof(path), "%s/pending.pid", dir) >=
	    (int)sizeof(path))
		return;
	fp = fopen(path, "r");
	if (fp == NULL)
		return;
	if (fgets(pidbuf, sizeof(pidbuf), fp) != NULL) {
		pid = (pid_t)atoi(pidbuf);
		if (pid > 0)
			kill(pid, SIGTERM);
	}
	fclose(fp);
	unlink(path);
}

static int
greeter_glyph(KeySym ks)
{
	if (ks == XK_Escape)
		return (1);
	if (ks >= XK_space && ks <= XK_asciitilde)
		return (1);
	if (ks >= XK_nobreakspace && ks <= XK_ydiaeresis)
		return (1);
	return (0);
}

static int
nudge_panel(void)
{
	char path[64], buf[32];
	FILE *fp;
	pid_t pid;

	snprintf(path, sizeof(path), "/tmp/bhotkeys-panel.%ld",
	    (long)getuid());
	fp = fopen(path, "r");
	if (fp == NULL)
		return (0);
	if (fgets(buf, sizeof(buf), fp) == NULL) {
		fclose(fp);
		return (0);
	}
	fclose(fp);
	pid = (pid_t)atoi(buf);
	if (pid <= 0 || kill(pid, SIGUSR1) != 0)
		return (0);
	return (1);
}

static int
capturing(void)
{
	char path[64], buf[32];
	FILE *fp;
	pid_t pid;

	snprintf(path, sizeof(path), "/tmp/bhotkeys-capturing.%ld",
	    (long)getuid());
	fp = fopen(path, "r");
	if (fp == NULL)
		return (0);
	if (fgets(buf, sizeof(buf), fp) == NULL) {
		fclose(fp);
		return (0);
	}
	fclose(fp);
	pid = (pid_t)atoi(buf);
	if (pid <= 0)
		return (0);
	if (kill(pid, 0) != 0)
		return (0);
	return (1);
}

static int
inflight_busy(void)
{
	int st;

	if (inflight <= 0)
		return (0);
	if (waitpid(inflight, &st, WNOHANG) == 0)
		return (1);
	inflight = -1;
	return (0);
}

static void
dispatch(KeySym ks, unsigned int state)
{
	const struct bh_plugin *p;
	int mod4 = (state & Mod4Mask) != 0;
	int pid;

	held_repeat = 0;
	if (bh_greeter && ks == XK_Escape && bh_panel_up())
		bh_backspace();
	if (capturing())
		return;
	if (bh_panel_open(live, ks, super_down, mod4)) {
		cancel_super_menu();
		if (bh_greeter && greeter_glyph(ks))
			bh_backspace();
		bh_run_panel();
		return;
	}
	if (!bh_match(live, ks, super_down, state, &p))
		return;
	if (p->need_super)
		cancel_super_menu();
	if (bh_greeter && p->need_super && greeter_glyph(ks))
		bh_backspace();
	/*
	 * Hardware autorepeat used to queue a child per tick. Those
	 * children kept running after the key came up. One child at
	 * a time: further repeats wait, and release stops the queue.
	 */
	if (p->repeat && inflight_busy()) {
		held_repeat = 1;
		return;
	}
	/*
	 * Lock: the overlay sits outside the window manager, so close
	 * it before xlock.  Otherwise wake an open list, and do not
	 * wait when none is open.
	 */
	if (strcmp(p->id, "lock") == 0)
		bh_dismiss_panel();
	else if (nudge_panel())
		usleep(30000);
	pid = bh_exec(p->command);
	if (p->repeat)
		inflight = pid;
	held_repeat = p->repeat;
}

static void
commit_up(void)
{
	if (!up_pending)
		return;
	up_pending = 0;
	if (held_code && up_code == held_code) {
		held_ks = NoSymbol;
		held_code = 0;
		held_repeat = 0;
	}
}

static void
on_key(int type, KeyCode code, unsigned int state, Time when)
{
	KeySym ks = XkbKeycodeToKeysym(bh_dpy, code, 0, 0);

	if (type == KeyRelease) {
		track_super(ks, 0);
		track_alt(ks, 0);
		up_code = code;
		up_time = when;
		up_pending = 1;
		gettimeofday(&up_when, NULL);
		return;
	}
	/* Autorepeat is a release then press; short gap is not a new chord */
	if (up_pending && code == up_code) {
		struct timeval now;

		gettimeofday(&now, NULL);
		if (tv_diff_ms(&now, &up_when) < 40) {
			up_pending = 0;
			return;
		}
	}
	(void)when;
	commit_up();
	track_super(ks, 1);
	track_alt(ks, 1);
	if (alt_down && (state & Mod1Mask) == 0)
		state |= Mod1Mask;
	/* Level 0 of this key is slash; Shift selects question */
	if ((state & ShiftMask) != 0 && ks == XK_slash) {
		KeySym up = XkbKeycodeToKeysym(bh_dpy, code, 0, 1);

		if (up != NoSymbol)
			ks = up;
	}
	dispatch(ks, state);
	gettimeofday(&last_fire, NULL);
	if (held_repeat) {
		held_ks = ks;
		held_code = code;
		held_since = last_fire;
	} else {
		held_ks = NoSymbol;
		held_code = 0;
	}
}

static void
record_callback(XPointer closure __unused, XRecordInterceptData *data)
{
	const xEvent *xev;

	if (data->category == XRecordFromServer && data->data != NULL &&
	    data->data_len >= 2) {
		xev = (const xEvent *)data->data;
		if (xev->u.u.type == KeyPress || xev->u.u.type == KeyRelease)
			on_key(xev->u.u.type, (KeyCode)xev->u.u.detail,
			    xev->u.keyButtonPointer.state,
			    xev->u.keyButtonPointer.time);
	}
	XRecordFreeData(data);
}

static void
repeat_tick(void)
{
	struct timeval now;
	long since_down, since_fire, need;

	if (held_ks == NoSymbol || !held_repeat)
		return;
	if (inflight_busy())
		return;
	gettimeofday(&now, NULL);
	since_down = tv_diff_ms(&now, &held_since);
	since_fire = tv_diff_ms(&now, &last_fire);
	need = (since_down < REPEAT_INITIAL_MS) ?
	    REPEAT_INITIAL_MS : REPEAT_RATE_MS;
	if (since_fire < need)
		return;
	dispatch(held_ks, 0);
	last_fire = now;
}

static void
maybe_reload(struct bh_set *set, struct timespec *stamp)
{
	char path[512];
	struct stat st;
	const char *xdg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (bh_greeter) {
		strlcpy(path, BH_GREETER_FILE, sizeof(path));
		if (stat(path, &st) != 0)
			return;
		if (st.st_mtim.tv_sec == stamp->tv_sec &&
		    st.st_mtim.tv_nsec == stamp->tv_nsec)
			return;
		*stamp = st.st_mtim;
		bh_load(set);
		return;
	}
	if (xdg != NULL && xdg[0] != '\0')
		snprintf(path, sizeof(path), "%s/bhotkeys/session", xdg);
	else {
		if (home == NULL)
			return;
		snprintf(path, sizeof(path), "%s/.config/bhotkeys/session",
		    home);
	}
	if (stat(path, &st) != 0)
		return;
	if (st.st_mtim.tv_sec == stamp->tv_sec &&
	    st.st_mtim.tv_nsec == stamp->tv_nsec)
		return;
	*stamp = st.st_mtim;
	bh_load(set);
}

int
bh_listen(struct bh_set *set)
{
	int major, minor, data_fd, n;
	XRecordRange *range;
	XRecordClientSpec client;
	XRecordContext ctx;
	fd_set rfds;
	struct timeval tv;
	struct timespec stamp = { 0, 0 };

	live = set;
	if (singleton_lock() < 0)
		return (bh_greeter ? 1 : 0);

	signal(SIGTERM, on_signal);
	signal(SIGINT, on_signal);
	signal(SIGHUP, on_signal);
	signal(SIGCHLD, SIG_DFL);

	bh_dpy = XOpenDisplay(NULL);
	dpy_data = XOpenDisplay(NULL);
	if (bh_dpy == NULL || dpy_data == NULL) {
		fprintf(stderr, "bhotkeys: cannot open display\n");
		return (1);
	}
	if (bh_greeter)
		setenv("BHOTKEYS_GREETER", "1", 1);
	if (!XRecordQueryVersion(bh_dpy, &major, &minor)) {
		fprintf(stderr, "bhotkeys: no RECORD extension\n");
		return (1);
	}
	range = XRecordAllocRange();
	if (range == NULL)
		return (1);
	range->device_events.first = KeyPress;
	range->device_events.last = KeyRelease;
	client = XRecordAllClients;
	ctx = XRecordCreateContext(bh_dpy, 0, &client, 1, &range, 1);
	XFree(range);
	if (ctx == 0)
		return (1);
	XSync(bh_dpy, False);
	if (!XRecordEnableContextAsync(dpy_data, ctx, record_callback, NULL))
		return (1);

	data_fd = ConnectionNumber(dpy_data);
	while (running) {
		while (waitpid(-1, NULL, WNOHANG) > 0)
			;
		if (up_pending) {
			struct timeval now;

			gettimeofday(&now, NULL);
			if (tv_diff_ms(&now, &up_when) > 30)
				commit_up();
		}
		maybe_reload(set, &stamp);
		repeat_tick();
		FD_ZERO(&rfds);
		FD_SET(data_fd, &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = 15000;
		n = select(data_fd + 1, &rfds, NULL, NULL, &tv);
		if (n > 0 && FD_ISSET(data_fd, &rfds))
			XRecordProcessReplies(dpy_data);
		else if (n == 0) {
			/* Focus selects no keys, so the press is not written
			 * to any client and the recording stays buffered.
			 */
			XSync(bh_dpy, False);
			XRecordProcessReplies(dpy_data);
		}
	}
	XRecordDisableContext(bh_dpy, ctx);
	XRecordFreeContext(bh_dpy, ctx);
	XCloseDisplay(dpy_data);
	XCloseDisplay(bh_dpy);
	if (lock_fd >= 0)
		close(lock_fd);
	return (0);
}
