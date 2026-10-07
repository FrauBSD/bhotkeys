/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Fork a plugin command, and open or dismiss the chord list.
 * The greeter child runs as root, and BackSpace there is sent
 * with XTest. A pid in /tmp/bhotkeys-panel.<uid> is a list
 * already on screen.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include "bhotkeys.h"

void
bh_backspace(void)
{
	KeyCode bs;

	if (!bh_greeter || bh_dpy == NULL)
		return;
	bs = XKeysymToKeycode(bh_dpy, XK_BackSpace);
	if (bs == 0)
		return;
	XTestFakeKeyEvent(bh_dpy, bs, True, CurrentTime);
	XTestFakeKeyEvent(bh_dpy, bs, False, CurrentTime);
	XFlush(bh_dpy);
}

int
bh_panel_up(void)
{
	char path[64], buf[32];
	FILE *fp;
	pid_t pid;

	snprintf(path, sizeof(path), "/tmp/bhotkeys-panel.%ld", (long)getuid());
	fp = fopen(path, "r");
	if (fp == NULL)
		return (0);
	if (fgets(buf, sizeof(buf), fp) == NULL) {
		fclose(fp);
		return (0);
	}
	fclose(fp);
	pid = (pid_t)atoi(buf);
	return (pid > 1 && kill(pid, 0) == 0);
}

static void
greeter_child_env(void)
{
	const char *home = getenv("HOME");
	char path[512];

	if (getenv("BOSD_PATH") == NULL && home != NULL && home[0] != '\0') {
		if (snprintf(path, sizeof(path), "%s/theme/osd", home) <
		    (int)sizeof(path))
			setenv("BOSD_PATH", path, 0);
	}
	setenv("HOME", "/root", 1);
	setenv("XDG_RUNTIME_DIR", "/var/run/xdg/root", 1);
	setenv("BHOTKEYS_GREETER", "1", 1);
	setenv("DISPLAY_GREETER", "1", 1);
	setenv("USER", "root", 1);
	setenv("LOGNAME", "root", 1);
	setenv("AUDIO_VOLMEM_FILE", "/tmp/greeter-audio-output-volumes", 1);
}

int
bh_exec(const char *command)
{
	pid_t pid;
	char buf[BH_CMD_MAX];
	char *args[16];
	char *tok, *save;
	int n = 0;
	const char *bin;
	char path[512];

	if (command == NULL || command[0] == '\0')
		return (-1);
	if (strlcpy(buf, command, sizeof(buf)) >= sizeof(buf))
		return (-1);
	for (tok = strtok_r(buf, " ", &save); tok != NULL && n < 15;
	    tok = strtok_r(NULL, " ", &save))
		args[n++] = tok;
	if (n == 0)
		return (-1);
	args[n] = NULL;
	pid = fork();
	if (pid < 0)
		return (-1);
	if (pid != 0)
		return (pid);
	if (bh_greeter)
		greeter_child_env();
	bin = getenv("FRAMEWORK_KEYBOARD_BIN");
	if (bin != NULL && strchr(args[0], '/') == NULL &&
	    snprintf(path, sizeof(path), "%s/%s", bin, args[0]) <
	    (int)sizeof(path) && access(path, X_OK) == 0)
		execv(path, args);
	execvp(args[0], args);
	_exit(127);
}

static int
panel_is_open(void)
{
	char path[64], buf[32];
	FILE *fp;
	pid_t pid;

	snprintf(path, sizeof(path), "/tmp/bhotkeys-panel.%ld", (long)getuid());
	fp = fopen(path, "r");
	if (fp == NULL)
		return (0);
	if (fgets(buf, sizeof(buf), fp) == NULL) {
		fclose(fp);
		return (0);
	}
	fclose(fp);
	pid = (pid_t)atoi(buf);
	if (pid <= 1 || kill(pid, 0) != 0) {
		unlink(path);
		return (0);
	}
	kill(pid, SIGTERM);
	return (1);
}

int
bh_dismiss_panel(void)
{
	char path[64], buf[32];
	FILE *fp;
	pid_t pid;
	int i;

	snprintf(path, sizeof(path), "/tmp/bhotkeys-panel.%ld", (long)getuid());
	fp = fopen(path, "r");
	if (fp == NULL)
		return (0);
	if (fgets(buf, sizeof(buf), fp) == NULL) {
		fclose(fp);
		return (0);
	}
	fclose(fp);
	pid = (pid_t)atoi(buf);
	if (pid <= 1 || kill(pid, 0) != 0) {
		unlink(path);
		return (0);
	}
	/*
	 * Second field is 1 after the Window control. That list is a
	 * normal window and stays up under xlock. The overlay has no
	 * second field, or 0, and has to be unmapped first.
	 */
	if (strchr(buf, ' ') != NULL && atoi(strchr(buf, ' ')) == 1)
		return (0);
	kill(pid, SIGTERM);
	for (i = 0; i < 40; i++) {
		if (kill(pid, 0) != 0)
			return (1);
		usleep(10000);
	}
	return (1);
}

int
bh_run_panel(void)
{
	pid_t pid;
	char *argv[4];

	if (panel_is_open())
		return (0);
	pid = fork();
	if (pid < 0)
		return (-1);
	if (pid != 0)
		return (0);
	argv[0] = "bhotkeys-panel";
	argv[1] = bh_greeter ? "--greeter" : "--session";
	argv[2] = NULL;
	execvp("bhotkeys-panel", argv);
	execl(PREFIX "/bin/bhotkeys-panel", "bhotkeys-panel", argv[1],
	    (char *)NULL);
	_exit(127);
}
