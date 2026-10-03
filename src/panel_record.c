/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * xlogin holds the keyboard. RECORD sees the press without taking
 * the grab or the focus; xlogin does not accept either back.
 * The callback only queues. Key handling runs after the reply
 * read, so a round trip cannot stall the server.
 */

#include <string.h>
#include <poll.h>

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/extensions/record.h>

#include "panel_priv.h"

#define REC_MAX 32

static Display *dpy_data;
static XRecordContext ctx;
static int rec_on;
static int rec_failed;
static int rec_close;
static int rec_dirty;
static struct {
	KeyCode code;
	unsigned int state;
	Time time;
} rec_q[REC_MAX];
static int rec_n;

static void
on_rec(XPointer closure __unused, XRecordInterceptData *data)
{
	const xEvent *xev;

	if (data->category == XRecordFromServer && data->data != NULL &&
	    data->data_len >= 2 && rec_n < REC_MAX) {
		xev = (const xEvent *)data->data;
		if (xev->u.u.type == KeyPress) {
			rec_q[rec_n].code = (KeyCode)xev->u.u.detail;
			rec_q[rec_n].state = xev->u.keyButtonPointer.state;
			rec_q[rec_n].time = xev->u.keyButtonPointer.time;
			rec_n++;
		}
	}
	XRecordFreeData(data);
}

static void
apply_keys(Display *dpy, struct panel_ui *ui)
{
	int i, n;

	n = rec_n;
	rec_n = 0;
	if (ui->yield)
		return;
	for (i = 0; i < n; i++) {
		XKeyEvent ev;

		memset(&ev, 0, sizeof(ev));
		ev.type = KeyPress;
		ev.display = dpy;
		ev.keycode = rec_q[i].code;
		ev.state = rec_q[i].state;
		ev.time = rec_q[i].time;
		if (panel_on_key(dpy, &ev, ui))
			rec_close = 1;
		rec_dirty = 1;
	}
	if (ui->drop_grab) {
		ui->drop_grab = 0;
		ui->yield = 1;
		panel_shade_unmap(dpy);
		XUngrabPointer(dpy, CurrentTime);
		XUngrabKeyboard(dpy, CurrentTime);
	}
	if (ui->release_keys)
		ui->release_keys = 0;
}

static void
arm(Display *dpy)
{
	int major, minor;
	XRecordRange *range;
	XRecordClientSpec client;

	if (rec_on || rec_failed || !bh_greeter)
		return;
	if (!XRecordQueryVersion(dpy, &major, &minor)) {
		rec_failed = 1;
		return;
	}
	dpy_data = XOpenDisplay(NULL);
	if (dpy_data == NULL) {
		rec_failed = 1;
		return;
	}
	range = XRecordAllocRange();
	if (range == NULL) {
		rec_failed = 1;
		return;
	}
	range->device_events.first = KeyPress;
	range->device_events.last = KeyRelease;
	client = XRecordAllClients;
	ctx = XRecordCreateContext(dpy, 0, &client, 1, &range, 1);
	XFree(range);
	if (ctx == 0) {
		rec_failed = 1;
		return;
	}
	XSync(dpy, False);
	if (!XRecordEnableContextAsync(dpy_data, ctx, on_rec, NULL)) {
		rec_failed = 1;
		return;
	}
	rec_on = 1;
}

int
panel_record_on(void)
{
	return (rec_on);
}

int
panel_record_dirty(void)
{
	int d = rec_dirty;

	rec_dirty = 0;
	return (d);
}

int
panel_take_quit(Display *dpy, struct panel_ui *ui)
{
	struct pollfd pfd;

	if (!bh_greeter)
		return (0);
	if (!rec_on)
		arm(dpy);
	if (!rec_on)
		return (0);
	pfd.fd = ConnectionNumber(dpy_data);
	pfd.events = POLLIN;
	pfd.revents = 0;
	if (poll(&pfd, 1, 0) > 0)
		XRecordProcessReplies(dpy_data);
	apply_keys(dpy, ui);
	return (rec_close);
}

int
panel_wait(Display *dpy, struct panel_ui *ui, int wait_ms)
{
	struct pollfd pfd[2];
	int nfd = 1;
	int pr;

	if (bh_greeter && !rec_on)
		arm(dpy);
	pfd[0].fd = ConnectionNumber(dpy);
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;
	if (rec_on) {
		pfd[1].fd = ConnectionNumber(dpy_data);
		pfd[1].events = POLLIN;
		pfd[1].revents = 0;
		nfd = 2;
	}
	pr = poll(pfd, (nfds_t)nfd, wait_ms);
	if (rec_on && (pfd[1].revents & POLLIN))
		XRecordProcessReplies(dpy_data);
	if (rec_on)
		apply_keys(dpy, ui);
	if (pr > 0 && (pfd[0].revents & POLLIN))
		return (1);
	return (0);
}
