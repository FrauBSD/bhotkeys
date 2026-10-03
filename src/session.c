/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Saved checks and chords. A line names the window manager it
 * belongs to. Loading one manager does not apply another's lines.
 * The greeter file has no manager name.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bhotkeys.h"

static int
fields(const char *line, char *id, int *en, char *chord, char *wm)
{
	int n;

	wm[0] = '\0';
	n = sscanf(line, "%63s %d %63s %31s", id, en, chord, wm);
	if (n == 4 || n == 3)
		return (n);
	return (0);
}

int
bh_session_ours(const char *line, char *id, int *en, char *chord)
{
	char wm[32];
	const char *here;
	int n;

	n = fields(line, id, en, chord, wm);
	if (bh_greeter)
		return (n == 3);
	here = bh_wm_name();
	if (n != 4 || here == NULL)
		return (0);
	return (strcasecmp(wm, here) == 0);
}

int
bh_session_keep(const char *line)
{
	char id[BH_ID_MAX], chord[BH_CHORD_MAX], wm[32];
	const char *here;
	int en, n;

	if (bh_greeter)
		return (0);
	n = fields(line, id, &en, chord, wm);
	if (n == 0)
		return (1);
	if (n != 4)
		return (0);
	here = bh_wm_name();
	if (here == NULL)
		return (1);
	return (strcasecmp(wm, here) != 0);
}

void
bh_apply_overrides(struct bh_set *set)
{
	char path[512];
	FILE *fp;
	char line[256];
	char id[BH_ID_MAX], chord[BH_CHORD_MAX];
	int enabled, i;

	bh_config_path(path, sizeof(path));
	fp = fopen(path, "r");
	if (fp == NULL)
		return;
	while (fgets(line, sizeof(line), fp) != NULL) {
		if (!bh_session_ours(line, id, &enabled, chord))
			continue;
		if (strcmp(id, BH_PANEL_ID) == 0) {
			set->panel_on = enabled ? 1 : 0;
			strlcpy(set->panel_chord, chord,
			    sizeof(set->panel_chord));
			continue;
		}
		if (strcmp(id, BH_PANEL_ALT_ID) == 0) {
			set->panel_alt_on = enabled ? 1 : 0;
			strlcpy(set->panel_alt_chord, chord,
			    sizeof(set->panel_alt_chord));
			continue;
		}
		for (i = 0; i < set->n; i++) {
			if (strcmp(set->at[i]->id, id) != 0)
				continue;
			set->at[i]->enabled = enabled ? 1 : 0;
			strlcpy(set->at[i]->chord, chord,
			    sizeof(set->at[i]->chord));
			apply_chord(set->at[i]);
		}
	}
	fclose(fp);
}

static int
ensure_dir(const char *path)
{
	char dir[512];
	char *slash;

	if (strlcpy(dir, path, sizeof(dir)) >= sizeof(dir))
		return (-1);
	slash = strrchr(dir, '/');
	if (slash == NULL)
		return (-1);
	*slash = '\0';
	if (mkdir(dir, 0700) == 0 || errno == EEXIST)
		return (0);
	slash = strrchr(dir, '/');
	if (slash == NULL)
		return (-1);
	*slash = '\0';
	if (mkdir(dir, 0700) != 0 && errno != EEXIST)
		return (-1);
	*slash = '/';
	if (mkdir(dir, 0700) != 0 && errno != EEXIST)
		return (-1);
	return (0);
}

static void
write_current(FILE *fp, const struct bh_set *set, const char *wm)
{
	int i;

	if (wm != NULL) {
		fprintf(fp, "%s %d %s %s\n", BH_PANEL_ID,
		    set->panel_on ? 1 : 0, set->panel_chord, wm);
		fprintf(fp, "%s %d %s %s\n", BH_PANEL_ALT_ID,
		    set->panel_alt_on ? 1 : 0, set->panel_alt_chord, wm);
	} else {
		fprintf(fp, "%s %d %s\n", BH_PANEL_ID,
		    set->panel_on ? 1 : 0, set->panel_chord);
		fprintf(fp, "%s %d %s\n", BH_PANEL_ALT_ID,
		    set->panel_alt_on ? 1 : 0, set->panel_alt_chord);
	}
	for (i = 0; i < set->n; i++) {
		if (bh_greeter && !set->at[i]->greeter_ok)
			continue;
		if (!bh_greeter && !set->at[i]->session_ok)
			continue;
		if (wm != NULL)
			fprintf(fp, "%s %d %s %s\n", set->at[i]->id,
			    set->at[i]->enabled ? 1 : 0,
			    set->at[i]->chord, wm);
		else
			fprintf(fp, "%s %d %s\n", set->at[i]->id,
			    set->at[i]->enabled ? 1 : 0,
			    set->at[i]->chord);
	}
}

int
bh_save_session(const struct bh_set *set)
{
	char path[512], tmp[600], line[256];
	FILE *in, *out;
	const char *wm;
	struct stat st;
	int fd;
	mode_t mode;

	wm = bh_wm_name();
	if (!bh_greeter && wm == NULL)
		return (0);
	bh_config_path(path, sizeof(path));
	if (ensure_dir(path) != 0)
		return (-1);
	if (snprintf(tmp, sizeof(tmp), "%s.XXXXXX", path) >= (int)sizeof(tmp))
		return (-1);
	fd = mkstemp(tmp);
	if (fd < 0)
		return (-1);
	out = fdopen(fd, "w");
	if (out == NULL) {
		close(fd);
		unlink(tmp);
		return (-1);
	}
	mode = 0600;
	in = fopen(path, "r");
	if (in != NULL) {
		if (fstat(fileno(in), &st) == 0)
			mode = st.st_mode & 0777;
		while (fgets(line, sizeof(line), in) != NULL) {
			if (bh_session_keep(line))
				fputs(line, out);
		}
		fclose(in);
	}
	write_current(out, set, bh_greeter ? NULL : wm);
	if (fchmod(fileno(out), mode) != 0) {
		fclose(out);
		unlink(tmp);
		return (-1);
	}
	if (fclose(out) != 0) {
		unlink(tmp);
		return (-1);
	}
	if (rename(tmp, path) != 0) {
		unlink(tmp);
		return (-1);
	}
	return (0);
}
