/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Which window manager is running, and which chord, enabled,
 * or session line applies to it.
 *
 * A line's first token is the value (the chord, or 0/1). The
 * remaining tokens are window-manager names. "default" is a
 * keyword, not a name. A known name (fvwm, bvwm, i3, kde, gnome,
 * xfce, and the others in the table) is matched case-insensitively
 * against the running manager. Any other token is a program name,
 * matched case-sensitively against a running command.
 *
 * The first line that names the running manager wins. If none do,
 * the first line with no names, or with the word default, wins.
 *
 * --wm name replaces detection with a known name, so files for one
 * manager can be mastered from another. Program-name tokens still
 * match the running commands.
 *
 * Detection runs once and is cached. A session file may start the
 * listener before the manager, so the daemon calls bh_wm_wait to
 * rescan for a bounded time until a known manager appears.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "bhotkeys.h"

#define NPROC	512
#define PNAME	64

static const struct {
	const char *key;
	const char *comm;
} known[] = {
	{ "bvwm", "bvwm" },
	{ "fvwm", "fvwm3" },
	{ "fvwm", "fvwm" },
	{ "fvwm", "fvwm2" },
	{ "i3", "i3" },
	{ "lxde", "lxsession" },
	{ "lxqt", "lxqt-session" },
	{ "kde", "kwin_x11" },
	{ "kde", "kwin_wayland" },
	{ "kde", "kwin" },
	{ "gnome", "gnome-shell" },
	{ "xfce", "xfwm4" },
	{ "openbox", "openbox" },
	{ "fluxbox", "fluxbox" },
	{ "awesome", "awesome" },
	{ "bspwm", "bspwm" },
	{ "herbstluftwm", "herbstluftwm" },
	{ "cinnamon", "cinnamon" },
	{ "cinnamon", "muffin" },
	{ "mate", "marco" },
	{ "enlightenment", "enlightenment" },
	{ "icewm", "icewm" },
	{ "jwm", "jwm" },
	{ "pekwm", "pekwm" },
	{ "cwm", "cwm" },
	{ "spectrwm", "spectrwm" },
	{ "xmonad", "xmonad" },
	{ "dwm", "dwm" },
	{ "windowmaker", "wmaker" },
	{ "afterstep", "afterstep" },
	{ "sway", "sway" },
	{ NULL, NULL }
};

static char procs[NPROC][PNAME];
static int nproc;
static int loaded;
static int forced;
static const char *canon;

static int
proc_has(const char *name)
{
	int i;

	for (i = 0; i < nproc; i++) {
		if (strcmp(procs[i], name) == 0)
			return (1);
	}
	return (0);
}

static void
load_procs(void)
{
	FILE *fp;
	char line[256];
	char *nl;

	if (loaded)
		return;
	loaded = 1;
	fp = popen("ps -ax -o comm=", "r");
	if (fp == NULL)
		return;
	while (nproc < NPROC && fgets(line, sizeof(line), fp) != NULL) {
		nl = strchr(line, '\n');
		if (nl == NULL)
			continue;
		*nl = '\0';
		if (line[0] == '\0' || strlen(line) >= PNAME)
			continue;
		strlcpy(procs[nproc], line, PNAME);
		nproc++;
	}
	pclose(fp);
}

static void
detect(void)
{
	int i;

	if (loaded)
		return;
	load_procs();
	if (forced)
		return;
	for (i = 0; known[i].key != NULL; i++) {
		if (proc_has(known[i].comm)) {
			canon = known[i].key;
			return;
		}
	}
}

void
bh_wm_wait(int secs)
{
	int i;

	for (i = 0; i < secs; i++) {
		detect();
		if (canon != NULL)
			return;
		sleep(1);
		loaded = 0;
		nproc = 0;
	}
	detect();
}

int
bh_wm_force(const char *name)
{
	int i;

	if (name == NULL)
		return (-1);
	for (i = 0; known[i].key != NULL; i++) {
		if (strcasecmp(name, known[i].key) == 0) {
			canon = known[i].key;
			forced = 1;
			return (0);
		}
	}
	return (-1);
}

/*
 * The next blank-separated token of s, or NULL at the end. *len is
 * its length. The string is not modified, so a line may be as long
 * and name as many managers as the author likes.
 */
static const char *
token(const char *s, size_t *len)
{
	const char *t;

	while (*s == ' ' || *s == '\t')
		s++;
	if (*s == '\0')
		return (NULL);
	for (t = s; *t != '\0' && *t != ' ' && *t != '\t'; t++)
		;
	*len = (size_t)(t - s);
	return (s);
}

static int
tok_eq(const char *tok, size_t len, const char *s, int fold)
{
	if (strlen(s) != len)
		return (0);
	return (fold ? strncasecmp(tok, s, len) == 0 :
	    strncmp(tok, s, len) == 0);
}

static int
tok_known(const char *tok, size_t len)
{
	int i;

	for (i = 0; known[i].key != NULL; i++) {
		if (tok_eq(tok, len, known[i].key, 1))
			return (1);
	}
	return (0);
}

static int
tok_proc(const char *tok, size_t len)
{
	int i;

	for (i = 0; i < nproc; i++) {
		if (tok_eq(tok, len, procs[i], 0))
			return (1);
	}
	return (0);
}

void
bh_wm_note(struct bh_wmpick *pick, const char *rest)
{
	const char *value, *tok;
	size_t vlen, len;
	int specific = 0, fallback;

	if (pick == NULL || rest == NULL)
		return;
	value = token(rest, &vlen);
	if (value == NULL || vlen >= BH_CHORD_MAX)
		return;
	detect();
	tok = token(value + vlen, &len);
	fallback = (tok == NULL);
	for (; tok != NULL; tok = token(tok + len, &len)) {
		if (tok_eq(tok, len, "default", 1))
			fallback = 1;
		else if (tok_known(tok, len)) {
			if (canon != NULL && tok_eq(tok, len, canon, 1))
				specific = 1;
		} else if (tok_proc(tok, len))
			specific = 1;
	}
	if (specific && pick->specific[0] == '\0') {
		memcpy(pick->specific, value, vlen);
		pick->specific[vlen] = '\0';
	}
	if (fallback && pick->fallback[0] == '\0') {
		memcpy(pick->fallback, value, vlen);
		pick->fallback[vlen] = '\0';
	}
}

const char *
bh_wm_pick(const struct bh_wmpick *pick)
{
	if (pick == NULL)
		return (NULL);
	if (pick->specific[0] != '\0')
		return (pick->specific);
	if (pick->fallback[0] != '\0')
		return (pick->fallback);
	return (NULL);
}

const char *
bh_wm_name(void)
{
	detect();
	return (canon);
}
