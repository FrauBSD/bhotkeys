/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Dispatch bhotkeys. --panel replaces this process with the chord
 * list. --print-keys format prints the chord list spelled for that
 * window manager and exits. Otherwise the process loads plugins and
 * listens. --greeter selects the login greeter. --wm name stands in
 * for the detected window manager.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "bhotkeys.h"

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: bhotkeys --daemon [--greeter] [--wm name]\n"
	    "       bhotkeys --panel [--greeter]\n"
	    "       bhotkeys --print-keys format [--wm name]\n"
	    "       bhotkeys --print-wm [--wm name]\n");
	bh_print_formats(stderr);
}

int
main(int argc, char **argv)
{
	struct bh_set set;
	const char *format = NULL;
	const char *wm = NULL;
	const char *name;
	int panel = 0;
	int daemon = 0;
	int print_wm = 0;
	int i;

	memset(&set, 0, sizeof(set));

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-d") == 0 ||
		    strcmp(argv[i], "--daemon") == 0) {
			daemon = 1;
			continue;
		}
		if (strcmp(argv[i], "--greeter") == 0)
			bh_greeter = 1;
		else if (strcmp(argv[i], "--panel") == 0)
			panel = 1;
		else if (strcmp(argv[i], "--print-keys") == 0 && i + 1 < argc)
			format = argv[++i];
		else if (strcmp(argv[i], "--print-wm") == 0)
			print_wm = 1;
		else if (strcmp(argv[i], "--wm") == 0 && i + 1 < argc)
			wm = argv[++i];
		else {
			usage();
			return (1);
		}
	}
	if (wm != NULL && bh_wm_force(wm) != 0) {
		fprintf(stderr, "bhotkeys: unknown window manager: %s\n", wm);
		return (1);
	}
	if (print_wm) {
		if (daemon || panel || format != NULL || bh_greeter) {
			usage();
			return (1);
		}
		bh_wm_wait(BH_WM_WAIT);
		name = bh_wm_name();
		if (name == NULL)
			return (1);
		printf("%s\n", name);
		return (0);
	}
	if (format != NULL) {
		bh_load(&set);
		if (bh_print_keys(&set, format) != 0) {
			usage();
			return (1);
		}
		return (0);
	}
	if (!panel && argc < 2) {
		usage();
		return (1);
	}
	if (panel) {
		execlp("bhotkeys-panel", "bhotkeys-panel",
		    bh_greeter ? "--greeter" : "--session", (char *)NULL);
		perror("bhotkeys-panel");
		return (1);
	}
	/*
	 * The session file may start the listener before the window
	 * manager. Wait for one so enabled, chord, and session lines
	 * resolve as the panel will later see them. The greeter has
	 * no window manager.
	 */
	if (!bh_greeter)
		bh_wm_wait(BH_WM_WAIT);
	bh_load(&set);
	return (bh_listen(&set));
}
