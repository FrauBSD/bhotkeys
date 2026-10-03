/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Greeter unlock field. A wrong password is a short note, then the
 * search field is back. Escape, a click outside the field, or the
 * lock again leaves the field without a note.
 */

#include <string.h>
#include <strings.h>
#include <sys/time.h>

#include <X11/Xlib.h>

#include "panel_priv.h"

void
panel_keys_hold(Display *dpy, Window win, Time t)
{
	(void)t;
	if (!bh_greeter || win == None)
		return;
	/*
	 * Do not grab the keyboard or move the focus. xlogin does not
	 * take either back, and the greeter stays dead until the
	 * display is reset. RECORD feeds the list without taking them.
	 */
	panel_below_osk(dpy, win);
}

void
panel_pw_clear(struct panel_ui *ui)
{
	ui->pwmode = 0;
	ui->pwbad = 0;
	ui->pwbad_until = 0;
	explicit_bzero(ui->pw, sizeof(ui->pw));
}

void
panel_pw_fail(struct panel_ui *ui, int code)
{
	struct timeval tv;

	ui->pwmode = 0;
	explicit_bzero(ui->pw, sizeof(ui->pw));
	ui->pwbad = code;
	gettimeofday(&tv, NULL);
	ui->pwbad_until = tv.tv_sec * 1000L + tv.tv_usec / 1000L + 1200L;
}

int
panel_pw_click(Display *dpy, Window win, struct panel_ui *ui, int what)
{
	if (what == HIT_CLOSE) {
		ui->close_req = 1;
		return (1);
	}
	if (what == HIT_UNLOCK && bh_greeter) {
		if (ui->readonly && !ui->pwmode) {
			ui->pwmode = 1;
			ui->pw[0] = '\0';
			ui->pwbad = 0;
			ui->pwbad_until = 0;
			ui->capture = -2;
			panel_capture(0);
			panel_keys_hold(dpy, win, CurrentTime);
		} else {
			if (!ui->readonly)
				ui->readonly = 1;
			panel_pw_clear(ui);
		}
		return (1);
	}
	if (ui->pwmode && what != HIT_SEARCH)
		panel_pw_clear(ui);
	return (0);
}

int
panel_pw_dismiss(Display *dpy, struct panel_ui *ui)
{
	(void)dpy;
	if (!ui->pwmode)
		return (0);
	panel_pw_clear(ui);
	return (1);
}

int
panel_pw_note(struct panel_ui *ui, long now)
{
	long left;

	if (ui->pwbad_until <= 0)
		return (0);
	left = ui->pwbad_until - now;
	if (left > 0)
		return (left > 1000000L ? 1000000 : (int)left);
	ui->pwbad = 0;
	ui->pwbad_until = 0;
	return (-1);
}
