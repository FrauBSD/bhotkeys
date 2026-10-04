/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Plugin descriptors are text files, not shared objects.
 *
 * One file per plugin under the scan directories.  Fields:
 *   id, label, chord, command, repeat, greeter, session, listen, desc,
 *   desc_greeter, group
 * group is the heading, shown exactly as written.
 * chord is a keysym name, or Super+/Alt+/Shift+/Ctrl+ prefixes.
 * More than one chord line is allowed. Tokens after the chord name
 * the window managers that line is for. The first line that names
 * the running manager wins; otherwise the first line with no names,
 * or with the word default, wins. enabled and session use the same
 * rule (0 or 1). No enabled line means on. No session line means shown.
 * listen 0 is advertised only (the window manager owns the key).
 * A saved line names its window manager and applies only there.
 * The greeter file has no manager name.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "bhotkeys.h"

int bh_greeter;
Display *bh_dpy;

static int
field(char *line, const char *key, char *dst, size_t dstlen)
{
	size_t n = strlen(key);

	if (strncmp(line, key, n) != 0 || line[n] != ' ')
		return (0);
	line += n + 1;
	while (*line == ' ' || *line == '\t')
		line++;
	if (strlen(line) >= dstlen)
		return (0);
	strlcpy(dst, line, dstlen);
	return (1);
}

int
bh_chord_parse(const char *chord, KeySym *ks, int *need_super,
    int *need_alt, int *need_shift, int *need_ctrl)
{
	const char *name;
	char compact[BH_CHORD_MAX];
	size_t i, j;
	KeySym sym;

	*need_super = 0;
	*need_alt = 0;
	*need_shift = 0;
	*need_ctrl = 0;
	*ks = NoSymbol;
	if (chord == NULL || chord[0] == '\0')
		return (-1);
	j = 0;
	for (i = 0; chord[i] != '\0' && j + 1 < sizeof(compact); i++) {
		if (chord[i] == ' ')
			continue;
		compact[j++] = chord[i];
	}
	compact[j] = '\0';
	name = compact;
	for (;;) {
		if (strncmp(name, "Super+", 6) == 0) {
			*need_super = 1;
			name += 6;
		} else if (strncmp(name, "Alt+", 4) == 0) {
			*need_alt = 1;
			name += 4;
		} else if (strncmp(name, "Shift+", 6) == 0) {
			*need_shift = 1;
			name += 6;
		} else if (strncmp(name, "Ctrl+", 5) == 0) {
			*need_ctrl = 1;
			name += 5;
		} else
			break;
	}
	if (strcmp(name, "slash") == 0 || strcmp(name, "/") == 0)
		sym = XK_slash;
	else if (strcmp(name, "space") == 0)
		sym = XK_space;
	else if (strcmp(name, "question") == 0 || strcmp(name, "?") == 0)
		sym = XK_question;
	else if (strcmp(name, "grave") == 0 || strcmp(name, "`") == 0)
		sym = XK_grave;
	else if (strlen(name) == 1 && isalpha((unsigned char)name[0]))
		sym = (KeySym)tolower((unsigned char)name[0]);
	else
		sym = XStringToKeysym(name);
	if (sym == NoSymbol)
		return (-1);
	*ks = sym;
	return (0);
}

void
bh_chord_pretty(const char *chord, char *dst, size_t dstlen)
{
	char compact[BH_CHORD_MAX];
	char key[BH_CHORD_MAX];
	size_t i, j, n;
	const char *name;

	if (dstlen == 0)
		return;
	dst[0] = '\0';
	if (chord == NULL)
		return;
	j = 0;
	for (i = 0; chord[i] != '\0' && j + 1 < sizeof(compact); i++) {
		if (chord[i] == ' ')
			continue;
		compact[j++] = chord[i];
	}
	compact[j] = '\0';
	name = compact;
	dst[0] = '\0';
	for (;;) {
		const char *mod = NULL;
		size_t skip = 0;

		if (strncmp(name, "Super+", 6) == 0) {
			mod = "Super";
			skip = 6;
		} else if (strncmp(name, "Ctrl+", 5) == 0) {
			mod = "Ctrl";
			skip = 5;
		} else if (strncmp(name, "Alt+", 4) == 0) {
			mod = "Alt";
			skip = 4;
		} else if (strncmp(name, "Shift+", 6) == 0) {
			mod = "Shift";
			skip = 6;
		} else
			break;
		if (dst[0] != '\0')
			strlcat(dst, " + ", dstlen);
		strlcat(dst, mod, dstlen);
		name += skip;
	}
	if (strcmp(name, "slash") == 0 || strcmp(name, "/") == 0)
		strlcpy(key, "/", sizeof(key));
	else if (strcmp(name, "grave") == 0 || strcmp(name, "`") == 0)
		strlcpy(key, "`", sizeof(key));
	else if (strcmp(name, "question") == 0 || strcmp(name, "?") == 0)
		strlcpy(key, "?", sizeof(key));
	else if (strcmp(name, "space") == 0)
		strlcpy(key, "Space", sizeof(key));
	else if (strlen(name) == 1 && isalpha((unsigned char)name[0])) {
		key[0] = (char)toupper((unsigned char)name[0]);
		key[1] = '\0';
	} else
		strlcpy(key, name, sizeof(key));
	n = strlen(dst);
	if (n > 0 && n + 3 < dstlen)
		strlcat(dst, " + ", dstlen);
	strlcat(dst, key, dstlen);
}

void
apply_chord(struct bh_plugin *p)
{
	if (bh_chord_parse(p->chord, &p->ks, &p->need_super, &p->need_alt,
	    &p->need_shift, &p->need_ctrl) != 0) {
		p->ks = NoSymbol;
		p->enabled = 0;
	}
}

static int
read_plugin(const char *path, struct bh_plugin *p)
{
	FILE *fp;
	char *line = NULL;
	size_t cap = 0;
	char *nl;
	struct bh_wmpick chords, ens, sess;
	const char *picked;

	memset(p, 0, sizeof(*p));
	memset(&chords, 0, sizeof(chords));
	memset(&ens, 0, sizeof(ens));
	memset(&sess, 0, sizeof(sess));
	p->enabled = 1;
	p->listen = 1;
	p->session_ok = 1;
	fp = fopen(path, "r");
	if (fp == NULL)
		return (-1);
	while (getline(&line, &cap, fp) > 0) {
		nl = strchr(line, '\n');
		if (nl != NULL)
			*nl = '\0';
		if (line[0] == '\0' || line[0] == '#')
			continue;
		if (field(line, "id", p->id, sizeof(p->id)))
			continue;
		if (field(line, "label", p->label, sizeof(p->label)))
			continue;
		if (strncmp(line, "chord ", 6) == 0) {
			bh_wm_note(&chords, line + 6);
			continue;
		}
		if (strncmp(line, "enabled ", 8) == 0) {
			bh_wm_note(&ens, line + 8);
			continue;
		}
		if (field(line, "command", p->command, sizeof(p->command)))
			continue;
		if (field(line, "desc_greeter", p->desc_greeter,
		    sizeof(p->desc_greeter)))
			continue;
		if (field(line, "desc", p->desc, sizeof(p->desc)))
			continue;
		if (field(line, "group", p->group, sizeof(p->group)))
			continue;
		if (strncmp(line, "repeat ", 7) == 0)
			p->repeat = atoi(line + 7);
		else if (strncmp(line, "greeter ", 8) == 0)
			p->greeter_ok = atoi(line + 8);
		else if (strncmp(line, "listen ", 7) == 0)
			p->listen = atoi(line + 7);
		else if (strncmp(line, "session ", 8) == 0)
			bh_wm_note(&sess, line + 8);
	}
	free(line);
	fclose(fp);
	if (p->id[0] == '\0')
		return (-1);
	if (p->listen && p->command[0] == '\0')
		return (-1);
	if (p->label[0] == '\0')
		strlcpy(p->label, p->id, sizeof(p->label));
	picked = bh_wm_pick(&chords);
	if (picked != NULL)
		strlcpy(p->chord, picked, sizeof(p->chord));
	strlcpy(p->suggested, p->chord, sizeof(p->suggested));
	apply_chord(p);
	picked = bh_wm_pick(&ens);
	if (picked != NULL && (picked[0] == '0' || picked[0] == '1') &&
	    picked[1] == '\0')
		p->enabled = (picked[0] == '1');
	picked = bh_wm_pick(&sess);
	if (picked != NULL && (picked[0] == '0' || picked[0] == '1') &&
	    picked[1] == '\0')
		p->session_ok = (picked[0] == '1');
	return (0);
}

static void
scan_dir(struct bh_set *set, const char *dir)
{
	DIR *dp;
	struct dirent *de;
	char path[512];
	struct bh_plugin p;

	dp = opendir(dir);
	if (dp == NULL)
		return;
	while ((de = readdir(dp)) != NULL) {
		if (de->d_name[0] == '.')
			continue;
		if (snprintf(path, sizeof(path), "%s/%s", dir, de->d_name) >=
		    (int)sizeof(path))
			continue;
		if (read_plugin(path, &p) != 0)
			continue;
		bh_plugin_store(set, &p);
	}
	closedir(dp);
}

void
bh_config_path(char *path, size_t len)
{
	const char *xdg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (bh_greeter) {
		strlcpy(path, BH_GREETER_FILE, len);
		return;
	}
	if (xdg != NULL && xdg[0] != '\0') {
		snprintf(path, len, "%s/bhotkeys/session", xdg);
		return;
	}
	if (home == NULL)
		home = "/tmp";
	snprintf(path, len, "%s/.config/bhotkeys/session", home);
}

int
bh_load(struct bh_set *set)
{
	const char *extra = getenv("BHOTKEYS_PLUGINS");
	const char *home = getenv("HOME");
	char path[512];

	int junk_alt, junk_shift, junk_ctrl;

	bh_set_clear(set);
	set->panel_on = 1;
	set->panel_alt_on = 1;
	strlcpy(set->panel_chord, BH_PANEL_CHORD, sizeof(set->panel_chord));
	strlcpy(set->panel_alt_chord, BH_PANEL_ALT_CHORD,
	    sizeof(set->panel_alt_chord));
	scan_dir(set, PREFIX "/share/bhotkeys/plugins.d");
	if (home != NULL) {
		snprintf(path, sizeof(path), "%s/share/bhotkeys/plugins.d",
		    home);
		scan_dir(set, path);
	}
	if (extra != NULL && extra[0] != '\0')
		scan_dir(set, extra);
	bh_apply_overrides(set);
	if (bh_chord_parse(set->panel_chord, &set->panel_ks,
	    &set->panel_super, &junk_alt, &junk_shift, &junk_ctrl) != 0) {
		strlcpy(set->panel_chord, BH_PANEL_CHORD,
		    sizeof(set->panel_chord));
		set->panel_ks = XK_question;
		set->panel_super = 1;
	}
	if (bh_chord_parse(set->panel_alt_chord, &set->panel_alt_ks,
	    &set->panel_alt_super, &junk_alt, &junk_shift, &junk_ctrl) != 0) {
		strlcpy(set->panel_alt_chord, BH_PANEL_ALT_CHORD,
		    sizeof(set->panel_alt_chord));
		set->panel_alt_ks = XK_slash;
		set->panel_alt_super = 1;
	}
	bh_rebind(set);
	return (0);
}
