/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Turn the list chords off or on without opening the list. Both
 * Panel and Panel alt change together, so --disable leaves no chord
 * that opens the list. Only those two lines of the session file are
 * rewritten.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "panel_priv.h"

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: bhotkeys-panel [--greeter] [{--enable|--disable}]\n");
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
close_open_list(void)
{
	char path[64], buf[32];
	FILE *fp;
	pid_t pid;

	snprintf(path, sizeof(path), "/tmp/bhotkeys-panel.%ld",
	    (long)getuid());
	fp = fopen(path, "r");
	if (fp == NULL)
		return;
	if (fgets(buf, sizeof(buf), fp) == NULL) {
		fclose(fp);
		return;
	}
	fclose(fp);
	pid = (pid_t)atoi(buf);
	if (pid <= 1 || pid == getpid() || kill(pid, 0) != 0)
		return;
	kill(pid, SIGTERM);
}

static void
put_line(FILE *out, const char *id, int on, const char *chord)
{
	if (bh_greeter || bh_wm_name() == NULL)
		fprintf(out, "%s %d %s\n", id, on ? 1 : 0, chord);
	else
		fprintf(out, "%s %d %s %s\n", id, on ? 1 : 0, chord,
		    bh_wm_name());
}

static int
set_enabled(int on)
{
	char path[512], tmp[600], line[512], chord[BH_CHORD_MAX];
	char id[BH_ID_MAX];
	FILE *fp, *out;
	struct stat st;
	int fd, found, found_alt, en;
	mode_t mode;

	bh_config_path(path, sizeof(path));
	fp = fopen(path, "r");
	if (fp == NULL && on)
		return (0);
	if (ensure_dir(path) != 0) {
		if (fp != NULL)
			fclose(fp);
		perror("bhotkeys-panel");
		return (-1);
	}
	if (snprintf(tmp, sizeof(tmp), "%s.XXXXXX", path) >= (int)sizeof(tmp)) {
		if (fp != NULL)
			fclose(fp);
		return (-1);
	}
	fd = mkstemp(tmp);
	if (fd < 0) {
		if (fp != NULL)
			fclose(fp);
		perror("bhotkeys-panel");
		return (-1);
	}
	out = fdopen(fd, "w");
	if (out == NULL) {
		if (fp != NULL)
			fclose(fp);
		close(fd);
		unlink(tmp);
		perror("bhotkeys-panel");
		return (-1);
	}
	mode = 0600;
	if (fp != NULL && fstat(fileno(fp), &st) == 0)
		mode = st.st_mode & 0777;
	found = found_alt = 0;
	if (fp != NULL) {
		while (fgets(line, sizeof(line), fp) != NULL) {
			if (!bh_session_ours(line, id, &en, chord))
				fputs(line, out);
			else if (strcmp(id, BH_PANEL_ID) == 0) {
				put_line(out, BH_PANEL_ID, on, chord);
				found = 1;
			} else if (strcmp(id, BH_PANEL_ALT_ID) == 0) {
				put_line(out, BH_PANEL_ALT_ID, on, chord);
				found_alt = 1;
			} else
				fputs(line, out);
		}
		fclose(fp);
	}
	if (on) {
		/*
		 * A missing line means the default, which is on.
		 */
		if (!found && !found_alt) {
			fclose(out);
			unlink(tmp);
			return (0);
		}
	} else {
		if (!found)
			put_line(out, BH_PANEL_ID, 0, BH_PANEL_CHORD);
		if (!found_alt)
			put_line(out, BH_PANEL_ALT_ID, 0, BH_PANEL_ALT_CHORD);
	}
	if (fchmod(fileno(out), mode) != 0) {
		fclose(out);
		unlink(tmp);
		perror("bhotkeys-panel");
		return (-1);
	}
	if (fclose(out) != 0) {
		unlink(tmp);
		perror("bhotkeys-panel");
		return (-1);
	}
	if (rename(tmp, path) != 0) {
		unlink(tmp);
		perror("bhotkeys-panel");
		return (-1);
	}
	return (0);
}

int
panel_admin(int argc, char **argv, int *status)
{
	int i, mode;

	mode = 0;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--greeter") == 0)
			bh_greeter = 1;
		else if (strcmp(argv[i], "--session") == 0)
			continue;
		else if (strcmp(argv[i], "--enable") == 0) {
			if (mode < 0) {
				usage();
				*status = 1;
				return (1);
			}
			mode = 1;
		} else if (strcmp(argv[i], "--disable") == 0) {
			if (mode > 0) {
				usage();
				*status = 1;
				return (1);
			}
			mode = -1;
		} else {
			usage();
			*status = 1;
			return (1);
		}
	}
	if (mode == 0)
		return (0);
	if (set_enabled(mode > 0) != 0) {
		*status = 1;
		return (1);
	}
	if (mode < 0)
		close_open_list();
	*status = 0;
	return (1);
}
