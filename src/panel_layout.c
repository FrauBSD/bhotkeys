/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Row heights and scroll limits for the chord list.
 */

#include <string.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "panel_priv.h"

int
panel_text_w(Display *dpy, XftFont *font, const char *s, int n)
{
	XGlyphInfo g;

	if (s == NULL)
		return (0);
	if (n < 0)
		n = (int)strlen(s);
	if (n <= 0)
		return (0);
	XftTextExtentsUtf8(dpy, font, (FcChar8 *)s, n, &g);
	return (g.xOff);
}

int
panel_wrap(Display *dpy, XftFont *font, const char *s, int max_w,
    char lines[][120], int maxn)
{
	const char *p = s;
	int n = 0;

	if (s == NULL || s[0] == '\0' || max_w < 16)
		return (0);
	while (*p != '\0' && n < maxn) {
		int len = (int)strlen(p);
		int take, i, sp, lo, hi;

		lo = 0;
		hi = len;
		while (lo < hi) {
			int mid = (lo + hi + 1) / 2;

			if (panel_text_w(dpy, font, p, mid) <= max_w)
				lo = mid;
			else
				hi = mid - 1;
		}
		take = lo;
		sp = -1;
		if (take < len) {
			for (i = 0; i < take; i++) {
				if (p[i] == ' ')
					sp = i;
			}
			if (sp > 0)
				take = sp;
		}
		if (take < 1)
			take = 1;
		if (take >= 120)
			take = 119;
		memcpy(lines[n], p, (size_t)take);
		lines[n][take] = '\0';
		n++;
		p += take;
		while (*p == ' ')
			p++;
	}
	return (n);
}

static void
metrics(struct panel_ui *ui)
{
	ui->search_x = 14;
	ui->search_y = 16;
	ui->search_h = ui->font_px + 20;
	ui->search_w = ui->win_w - 28;
	/*
	 * The close mark is a square the height of the field. The field's
	 * left margin (14) sits on both sides of that square.
	 */
	if (bh_greeter)
		ui->search_w = ui->win_w - ui->search_h - 42;
	ui->header_h = ui->font_px + 16;
	ui->footer_h = ui->font_px + 26;
	ui->header_y = ui->search_y + ui->search_h + 12;
	ui->footer_y = ui->win_h - ui->footer_h;
	/*
	 * The scroll marks sit in this gap. Rows start below the upper
	 * one and stop above the lower one.
	 */
	ui->body_y = ui->header_y + ui->header_h + 20;
	ui->body_h = ui->footer_y - ui->body_y - 24;
	if (ui->body_h < ui->font_px)
		ui->body_h = ui->font_px;
	ui->name_x = 20 + ui->font_px * 2;
	if (ui->name_x < 84)
		ui->name_x = 84;
	ui->act_x = ui->win_w - 230;
	if (ui->act_x < ui->win_w / 2)
		ui->act_x = ui->win_w * 3 / 5;
	ui->chord_x = ui->name_x + (ui->act_x - ui->name_x) / 2;
}

int
panel_group_gap(XftFont *font, int row)
{
	if (row <= 0 || font == NULL)
		return (0);
	return (font->ascent + font->descent);
}

void
panel_layout(Display *dpy, struct panel_ui *ui, XftFont *font, XftFont *small)
{
	int i, y, desc_w, max, title_h, line_h;

	metrics(ui);
	title_h = font->ascent + font->descent;
	line_h = small->ascent + small->descent + 2;
	desc_w = ui->act_x - ui->name_x - 12;
	y = 0;
	ui->nlay = ui->shown;
	for (i = 0; i < ui->shown; i++) {
		const char *desc;
		char lines[6][120];
		int nlines = 0;
		int idx = ui->order[i];
		char names[4][120];
		int nt;

		if (ROW_HEAD(idx)) {
			int gap = panel_group_gap(small, i);

			ui->lay_y[i] = y;
			ui->lay_h[i] = gap + small->ascent + small->descent + 8;
			y += ui->lay_h[i];
			continue;
		}
		desc = idx == ROW_ALT ?
		    (bh_greeter ? BH_PANEL_ALT_DESC_GREETER : BH_PANEL_ALT_DESC) :
		    panel_blurb(idx < 0 ? NULL : ui->set->at[idx]);
		nt = panel_wrap(dpy, font, panel_row_label(ui, idx),
		    ui->chord_x - ui->name_x - 12, names, 4);
		if (nt < 1)
			nt = 1;
		nlines = panel_wrap(dpy, small, desc, desc_w, lines, 6);
		ui->lay_y[i] = y;
		ui->lay_h[i] = 10 + nt * title_h + 8 + 12;
		if (nlines > 0)
			ui->lay_h[i] += nlines * line_h;
		y += ui->lay_h[i];
	}
	ui->content_h = y;
	max = ui->content_h - ui->body_h;
	if (max < 0)
		max = 0;
	if (ui->scroll < 0)
		ui->scroll = 0;
	if (ui->scroll > max)
		ui->scroll = max;
	ui->reset_x = ui->act_x +
	    panel_text_w(dpy, small, "Set keys", -1) + 20;
}

void
panel_scroll_set(struct panel_ui *ui, int y)
{
	int max;

	max = ui->content_h - ui->body_h;
	if (max < 0)
		max = 0;
	if (y < 0)
		y = 0;
	if (y > max)
		y = max;
	ui->scroll = y;
	if (ui->query[0] == '\0')
		ui->saved_scroll = y;
}

void
panel_scroll(struct panel_ui *ui, int dir, int page)
{
	int max, i;

	max = ui->content_h - ui->body_h;
	if (max < 0)
		max = 0;
	if (page)
		ui->scroll += dir * (ui->body_h > 40 ? ui->body_h - 24 : 40);
	else if (dir > 0) {
		for (i = 0; i < ui->nlay; i++) {
			if (ui->lay_y[i] > ui->scroll + 1) {
				ui->scroll = ui->lay_y[i];
				break;
			}
		}
	} else {
		for (i = ui->nlay - 1; i >= 0; i--) {
			if (ui->lay_y[i] < ui->scroll - 1) {
				ui->scroll = ui->lay_y[i];
				break;
			}
		}
	}
	if (ui->scroll < 0)
		ui->scroll = 0;
	if (ui->scroll > max)
		ui->scroll = max;
	if (ui->query[0] == '\0')
		ui->saved_scroll = ui->scroll;
}

void
panel_place(Display *dpy, int scr, int *x, int *y, int *w, int *h, int *px)
{
	Window root = RootWindow(dpy, scr);
	Window child;
	int rx, ry, wx, wy, n = 0, i, mx, my, mw, mh;
	unsigned int mask;
	XRRMonitorInfo *mon;

	mx = 0;
	my = 0;
	mw = DisplayWidth(dpy, scr);
	mh = DisplayHeight(dpy, scr);
	if (XQueryPointer(dpy, root, &root, &child, &rx, &ry, &wx, &wy, &mask)) {
		mon = XRRGetMonitors(dpy, root, True, &n);
		if (mon != NULL) {
			for (i = 0; i < n; i++) {
				if (rx < mon[i].x || ry < mon[i].y)
					continue;
				if (rx >= mon[i].x + mon[i].width)
					continue;
				if (ry >= mon[i].y + mon[i].height)
					continue;
				mx = mon[i].x;
				my = mon[i].y;
				mw = mon[i].width;
				mh = mon[i].height;
				break;
			}
			XRRFreeMonitors(mon);
		}
	}
	*px = mh / 42;
	if (*px < 18)
		*px = 18;
	if (*px > 32)
		*px = 32;
	*w = mw * 2 / 3;
	if (*w < 860)
		*w = 860;
	if (*w > mw - 48)
		*w = mw - 48;
	*h = mh * 3 / 4;
	if (*h > mh - 48)
		*h = mh - 48;
	*x = mx + (mw - *w) / 2;
	*y = my + (mh - *h) / 2;
}
