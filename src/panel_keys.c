/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Keys while the chord list is up. A modifier chord is not search
 * text: the listener still runs it, unless a new chord is being captured.
 */

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>

#include "panel_priv.h"

extern int	bh_passwd_ok(const char *pw);

void
panel_capture(int on)
{
	char path[64];
	FILE *fp;

	snprintf(path, sizeof(path), "/tmp/bhotkeys-capturing.%ld",
	    (long)getuid());
	if (!on) {
		unlink(path);
		return;
	}
	fp = fopen(path, "w");
	if (fp == NULL)
		return;
	fprintf(fp, "%ld\n", (long)getpid());
	fclose(fp);
}

static void
ui_path(char *path, size_t n)
{
	const char *home = getenv("HOME");

	if (home == NULL || home[0] == '\0')
		home = "/tmp";
	snprintf(path, n, "%s/.config/bhotkeys/panel-ui", home);
}

void
panel_ui_load(struct panel_ui *ui)
{
	FILE *fp;
	char path[512], line[64];
	int v;

	if (bh_greeter)
		return;
	ui_path(path, sizeof(path));
	fp = fopen(path, "r");
	if (fp == NULL)
		return;
	while (fgets(line, sizeof(line), fp) != NULL) {
		if (sscanf(line, "sort %d", &v) == 1) {
			if (v < 0)
				ui->sort_col = -1;
			else
				ui->sort_col = v ? 1 : 0;
		}
		else if (sscanf(line, "rev %d", &v) == 1)
			ui->sort_rev = v ? 1 : 0;
		else if (sscanf(line, "scroll %d", &v) == 1) {
			if (v < 0)
				v = 0;
			ui->scroll = v;
			ui->saved_scroll = v;
		}
	}
	fclose(fp);
}

void
panel_ui_save(const struct panel_ui *ui)
{
	FILE *fp;
	char path[512], dir[512];
	const char *home = getenv("HOME");

	if (bh_greeter)
		return;
	if (home == NULL || home[0] == '\0')
		home = "/tmp";
	snprintf(dir, sizeof(dir), "%s/.config", home);
	mkdir(dir, 0755);
	snprintf(dir, sizeof(dir), "%s/.config/bhotkeys", home);
	mkdir(dir, 0755);
	ui_path(path, sizeof(path));
	fp = fopen(path, "w");
	if (fp == NULL)
		return;
	fprintf(fp, "sort %d\nrev %d\nscroll %d\n",
	    ui->sort_col, ui->sort_rev ? 1 : 0, ui->saved_scroll);
	fclose(fp);
}

static const char *const apply_wm[] = {
	"bvwm", "fvwm", "kde", "xfce", "gnome", "mate", "i3",
	"cinnamon", "dwm", "fluxbox", "lxde", "lxqt", "openbox",
	"windowmaker", "xmonad",
	NULL
};

static void
fork_apply(const char *home_path, const char *local_path)
{
	const char *apply = NULL;
	pid_t pid;

	if (access(home_path, X_OK) == 0)
		apply = home_path;
	else if (access(local_path, X_OK) == 0)
		apply = local_path;
	if (apply == NULL)
		return;
	signal(SIGCHLD, SIG_IGN);
	pid = fork();
	if (pid < 0)
		return;
	if (pid == 0) {
		execl(apply, apply, (char *)NULL);
		_exit(127);
	}
}

static void
fork_named(const char *name)
{
	char home_path[128];
	char local_path[80];

	snprintf(home_path, sizeof(home_path),
	    "/home/dteske/bin/bhotkeys-%s-apply", name);
	snprintf(local_path, sizeof(local_path),
	    "/usr/local/libexec/bhotkeys/bhotkeys-%s-apply", name);
	fork_apply(home_path, local_path);
}

void
panel_save(struct panel_ui *ui)
{
	int i;

	if (ui->readonly)
		return;
	bh_save_session(ui->set);
	if (bh_greeter)
		return;
	/*
	 * The mark is already flipped. Applying every binding in this
	 * process is what made the X wait.
	 */
	for (i = 0; apply_wm[i] != NULL; i++)
		fork_named(apply_wm[i]);
}

static int
mod_key(KeySym ks)
{
	switch (ks) {
	case XK_Shift_L: case XK_Shift_R:
	case XK_Control_L: case XK_Control_R:
	case XK_Alt_L: case XK_Alt_R:
	case XK_Meta_L: case XK_Meta_R:
	case XK_Super_L: case XK_Super_R:
	case XK_Hyper_L: case XK_Hyper_R:
	case XK_ISO_Level3_Shift: case XK_Mode_switch:
	case XK_Caps_Lock: case XK_Shift_Lock:
		return (1);
	default:
		return (0);
	}
}

static int
scroll_key(KeySym ks, int *page)
{
	*page = 0;
	switch (ks) {
	case XK_Up: case XK_KP_Up: case XF86XK_ScrollUp:
		return (-1);
	case XK_Down: case XK_KP_Down: case XF86XK_ScrollDown:
		return (1);
	case XK_Prior: case XK_KP_Prior:
		*page = 1;
		return (-1);
	case XK_Next: case XK_KP_Next:
		*page = 1;
		return (1);
	default:
		return (0);
	}
}

static void
note_chord(KeySym ks, unsigned int state, char *dst, size_t len)
{
	char key[64], tmp[BH_CHORD_MAX];
	int super = (state & Mod4Mask) != 0;
	int alt = (state & Mod1Mask) != 0;
	int shift = (state & ShiftMask) != 0;
	int ctrl = (state & ControlMask) != 0;
	int letter = 0;

	dst[0] = '\0';
	if (ks >= XK_A && ks <= XK_Z)
		ks = (KeySym)(ks - XK_A + XK_a);
	if (ks == XK_slash)
		strlcpy(key, "/", sizeof(key));
	else if (ks == XK_question) {
		strlcpy(key, "?", sizeof(key));
		shift = 0;
	} else if (ks == XK_grave)
		strlcpy(key, "`", sizeof(key));
	else if (ks == XK_asciitilde) {
		strlcpy(key, "`", sizeof(key));
		shift = 1;
	} else if (ks == XK_space)
		strlcpy(key, "space", sizeof(key));
	else if (ks >= XK_a && ks <= XK_z) {
		key[0] = (char)toupper((unsigned char)ks);
		key[1] = '\0';
		letter = 1;
	} else {
		const char *name = XKeysymToString(ks);

		if (name == NULL)
			return;
		strlcpy(key, name, sizeof(key));
	}
	tmp[0] = '\0';
	if (super)
		strlcat(tmp, "Super+", sizeof(tmp));
	if (ctrl)
		strlcat(tmp, "Ctrl+", sizeof(tmp));
	if (alt)
		strlcat(tmp, "Alt+", sizeof(tmp));
	if (shift && !letter)
		strlcat(tmp, "Shift+", sizeof(tmp));
	strlcat(tmp, key, sizeof(tmp));
	if (strlen(tmp) >= len)
		return;
	strlcpy(dst, tmp, len);
}

static void
append_pw(struct panel_ui *ui, const char *buf, int n)
{
	size_t qn = strlen(ui->pw);

	if (n != 1 || (unsigned char)buf[0] < 32)
		return;
	if (qn + 1 >= sizeof(ui->pw))
		return;
	ui->pw[qn] = buf[0];
	ui->pw[qn + 1] = '\0';
}

static void
query_clamp(struct panel_ui *ui)
{
	int n = (int)strlen(ui->query);

	if (ui->qpos < 0)
		ui->qpos = 0;
	if (ui->qpos > n)
		ui->qpos = n;
}

static void
query_changed(struct panel_ui *ui)
{
	ui->scroll = 0;
	panel_refilter(ui);
}

static void
query_insert(struct panel_ui *ui, const char *buf, int n)
{
	size_t qn;

	query_clamp(ui);
	if (n <= 0 || (unsigned char)buf[0] < 32)
		return;
	qn = strlen(ui->query);
	if (qn + (size_t)n >= sizeof(ui->query))
		return;
	memmove(ui->query + ui->qpos + n, ui->query + ui->qpos,
	    qn - (size_t)ui->qpos + 1);
	memcpy(ui->query + ui->qpos, buf, (size_t)n);
	ui->qpos += n;
	query_changed(ui);
}

static int
edit_search(struct panel_ui *ui, KeySym ks, unsigned int st)
{
	int ctrl = (st & ControlMask) != 0;
	size_t nlen;

	query_clamp(ui);
	if (ks == XK_Left || ks == XK_KP_Left) {
		if (ui->qpos > 0)
			ui->qpos--;
		return (1);
	}
	if (ks == XK_Right || ks == XK_KP_Right) {
		if (ui->qpos < (int)strlen(ui->query))
			ui->qpos++;
		return (1);
	}
	if (ctrl && (ks == XK_a || ks == XK_A)) {
		ui->qpos = 0;
		return (1);
	}
	if (ctrl && (ks == XK_e || ks == XK_E)) {
		ui->qpos = (int)strlen(ui->query);
		return (1);
	}
	if (ctrl && (ks == XK_u || ks == XK_U)) {
		memmove(ui->query, ui->query + ui->qpos,
		    strlen(ui->query + ui->qpos) + 1);
		ui->qpos = 0;
		query_changed(ui);
		return (1);
	}
	if (ctrl && (ks == XK_k || ks == XK_K)) {
		ui->query[ui->qpos] = '\0';
		query_changed(ui);
		return (1);
	}
	if (ctrl && (ks == XK_w || ks == XK_W)) {
		int p = ui->qpos;

		while (p > 0 && ui->query[p - 1] == ' ')
			p--;
		while (p > 0 && ui->query[p - 1] != ' ')
			p--;
		memmove(ui->query + p, ui->query + ui->qpos,
		    strlen(ui->query + ui->qpos) + 1);
		ui->qpos = p;
		query_changed(ui);
		return (1);
	}
	if (ks == XK_BackSpace || (ctrl && (ks == XK_h || ks == XK_H))) {
		if (ui->qpos <= 0)
			return (1);
		memmove(ui->query + ui->qpos - 1, ui->query + ui->qpos,
		    strlen(ui->query + ui->qpos) + 1);
		ui->qpos--;
		query_changed(ui);
		return (1);
	}
	if (ks == XK_Delete) {
		nlen = strlen(ui->query);
		if (ui->qpos >= (int)nlen)
			return (1);
		memmove(ui->query + ui->qpos, ui->query + ui->qpos + 1,
		    nlen - (size_t)ui->qpos);
		query_changed(ui);
		return (1);
	}
	return (0);
}

int
panel_on_key(Display *dpy, XKeyEvent *ev, struct panel_ui *ui)
{
	KeySym ks = NoSymbol;
	unsigned int st = ev->state;
	int page = 0, dir;
	char buf[8];

	(void)XLookupString(ev, buf, (int)sizeof(buf) - 1, &ks, NULL);
	if (ks == NoSymbol)
		ks = XkbKeycodeToKeysym(dpy, ev->keycode, 0, 0);
	if (ks == NoSymbol)
		ks = XLookupKeysym(ev, 0);
	ui->pwbad = 0;
	ui->pwbad_until = 0;
	if (ks == XK_Escape ||
	    (XKeysymToKeycode(dpy, XK_Escape) != 0 &&
	    ev->keycode == XKeysymToKeycode(dpy, XK_Escape))) {
		if (ui->pwmode) {
			panel_pw_clear(ui);
			ui->release_keys = 1;
			return (0);
		}
		if (ui->capture != -2) {
			ui->capture = -2;
			ui->release_keys = 1;
			panel_capture(0);
			return (0);
		}
		if (ui->query[0] != '\0') {
			ui->query[0] = '\0';
			ui->qpos = 0;
			panel_refilter(ui);
			return (0);
		}
		return (1);
	}
	if (mod_key(ks))
		return (0);
	if (ui->pwmode) {
		int n;

		if (ks == XK_Return || ks == XK_KP_Enter ||
		    (XKeysymToKeycode(dpy, XK_Return) != 0 &&
		    ev->keycode == XKeysymToKeycode(dpy, XK_Return)) ||
		    (XKeysymToKeycode(dpy, XK_KP_Enter) != 0 &&
		    ev->keycode == XKeysymToKeycode(dpy, XK_KP_Enter))) {
			int rc = bh_passwd_ok(ui->pw);

			if (rc == 1) {
				ui->readonly = 0;
				panel_pw_clear(ui);
			} else
				panel_pw_fail(ui, 1);
			ui->release_keys = 1;
			return (0);
		}
		if (ks == XK_BackSpace || ks == XK_Delete) {
			size_t nlen = strlen(ui->pw);

			if (nlen > 0)
				ui->pw[nlen - 1] = '\0';
			return (0);
		}
		n = XLookupString(ev, buf, (int)sizeof(buf) - 1, &ks, NULL);
		append_pw(ui, buf, n);
		return (0);
	}
	if (ui->capture == ROW_PANEL || ui->capture == ROW_ALT ||
	    ui->capture >= 0) {
		char neu[BH_CHORD_MAX];

		note_chord(ks, st, neu, sizeof(neu));
		if (neu[0] == '\0')
			return (0);
		if (ui->capture == ROW_PANEL)
			strlcpy(ui->set->panel_chord, neu,
			    sizeof(ui->set->panel_chord));
		else if (ui->capture == ROW_ALT)
			strlcpy(ui->set->panel_alt_chord, neu,
			    sizeof(ui->set->panel_alt_chord));
		else
			strlcpy(ui->set->at[ui->capture]->chord, neu,
			    sizeof(ui->set->at[0]->chord));
		panel_save(ui);
		ui->capture = -2;
		ui->release_keys = 1;
		panel_capture(0);
		return (0);
	}
	if (!ui->pwmode &&
	    (st & (Mod4Mask | Mod1Mask | Mod3Mask | Mod5Mask)) == 0 &&
	    edit_search(ui, ks, st))
		return (0);
	if ((st & (Mod4Mask | ControlMask | Mod1Mask)) == 0 &&
	    (ks == XK_Home || ks == XK_KP_Home ||
	    ks == XK_End || ks == XK_KP_End)) {
		panel_scroll_set(ui, (ks == XK_Home || ks == XK_KP_Home) ?
		    0 : ui->content_h);
		panel_ui_save(ui);
		return (0);
	}
	dir = scroll_key(ks, &page);
	if (dir != 0 && (st & (Mod4Mask | ControlMask | Mod1Mask)) == 0) {
		panel_scroll(ui, dir, page);
		panel_ui_save(ui);
		return (0);
	}
	if (st & (Mod4Mask | ControlMask | Mod1Mask | Mod3Mask | Mod5Mask)) {
		ui->drop_grab = 1;
		return (0);
	}
	{
		int n = XLookupString(ev, buf, (int)sizeof(buf) - 1, &ks, NULL);

		query_insert(ui, buf, n);
	}
	return (0);
}
