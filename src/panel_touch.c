/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * One-finger drag on a touch screen. A mouse drag is Button1Motion.
 * Some touch screens deliver that as XI_Touch* and never as core motion.
 */

#include <string.h>

#include <X11/extensions/XInput2.h>

#include "panel_priv.h"

static int xi_opcode = -1;
static int touch_id = -1;
static int tap_only;

void
panel_touch_select(Display *dpy, Window win)
{
	XIEventMask mask;
	unsigned char bits[4];
	int major = 2, minor = 2, err, junk;

	if (!XQueryExtension(dpy, "XInputExtension", &xi_opcode, &junk, &err)) {
		xi_opcode = -1;
		return;
	}
	if (XIQueryVersion(dpy, &major, &minor) != Success) {
		xi_opcode = -1;
		return;
	}
	memset(bits, 0, sizeof(bits));
	if (XIMaskLen(XI_TouchEnd) > (int)sizeof(bits))
		return;
	XISetMask(bits, XI_TouchBegin);
	XISetMask(bits, XI_TouchUpdate);
	XISetMask(bits, XI_TouchEnd);
	mask.deviceid = XIAllMasterDevices;
	mask.mask_len = sizeof(bits);
	mask.mask = bits;
	XISelectEvents(dpy, win, &mask, 1);
}

static void
arm(struct panel_ui *ui, int x, int y)
{
	ui->drag = 1;
	ui->drag_x = x;
	ui->drag_y = y;
	ui->drag_scroll = ui->scroll;
	ui->drag_moved = 0;
	ui->xtouch = 1;
}

static void
move(struct panel_ui *ui, int x, int y)
{
	int dx = x - ui->drag_x;
	int dy = y - ui->drag_y;

	if (dx < 0)
		dx = -dx;
	if (dy < 0)
		dy = -dy;
	if (dx > 6 || dy > 6)
		ui->drag_moved = 1;
	if (ui->drag_moved)
		panel_scroll_set(ui, ui->drag_scroll - (y - ui->drag_y));
}

int
panel_gesture(Display *dpy, XEvent *ev, struct panel_ui *ui)
{
	XGenericEventCookie *cookie;
	XIDeviceEvent *te;

	if (ev->type != GenericEvent || xi_opcode < 0)
		return (0);
	cookie = &ev->xcookie;
	if (!XGetEventData(dpy, cookie))
		return (0);
	if (cookie->extension != xi_opcode || cookie->data == NULL) {
		XFreeEventData(dpy, cookie);
		return (0);
	}
	te = cookie->data;
	if (cookie->evtype == XI_TouchBegin) {
		touch_id = (int)te->detail;
		ui->drag_x = (int)te->event_x;
		ui->drag_y = (int)te->event_y;
		ui->drag_moved = 0;
		if (te->event_y >= ui->body_y && te->event_y < ui->footer_y) {
			tap_only = 0;
			arm(ui, ui->drag_x, ui->drag_y);
		} else {
			tap_only = 1;
			ui->drag = 0;
			ui->xtouch = 1;
		}
		XFreeEventData(dpy, cookie);
		return (1);
	}
	if (cookie->evtype == XI_TouchUpdate && touch_id == (int)te->detail) {
		int x = (int)te->event_x;
		int y = (int)te->event_y;
		int dx = x - ui->drag_x;
		int dy = y - ui->drag_y;

		if (dx < 0)
			dx = -dx;
		if (dy < 0)
			dy = -dy;
		if (tap_only) {
			if (dx > 6 || dy > 6)
				ui->drag_moved = 1;
			XFreeEventData(dpy, cookie);
			return (1);
		}
		if (ui->drag) {
			move(ui, x, y);
			XFreeEventData(dpy, cookie);
			return (ui->drag_moved ? 2 : 1);
		}
	}
	if (cookie->evtype == XI_TouchEnd && touch_id == (int)te->detail) {
		int tap = tap_only;
		int moved = ui->drag_moved;

		touch_id = -1;
		tap_only = 0;
		ui->xtouch = 0;
		XFreeEventData(dpy, cookie);
		if (tap)
			return (moved ? 1 : 4);
		return (ui->drag ? 3 : 1);
	}
	XFreeEventData(dpy, cookie);
	return (0);
}
