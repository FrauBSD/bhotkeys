/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Super+/ list. Name and chord headers stay put. The list scrolls
 * between them and the footer.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>

#include "panel_priv.h"

static void
text(XftDraw *d, XftColor *c, XftFont *f, int x, int y, const char *s)
{
	if (s == NULL || s[0] == '\0')
		return;
	XftDrawStringUtf8(d, c, f, x, y, (FcChar8 *)s, (int)strlen(s));
}

static void
draw_check(Display *dpy, XftDraw *d, XftColor *c, XftFont *f, int x, int y,
    int on)
{
	int bw = panel_text_w(dpy, f, "[", 1);
	int xw = panel_text_w(dpy, f, "x", 1);

	text(d, c, f, x, y, "[");
	if (on)
		text(d, c, f, x + bw, y, "x");
	text(d, c, f, x + bw + xw, y, "]");
}

static void
clip(XftDraw *draw, int x, int y, int w, int h)
{
	XRectangle r;

	if (w < 1)
		w = 1;
	if (h < 1)
		h = 1;
	r.x = (short)x;
	r.y = (short)y;
	r.width = (unsigned short)w;
	r.height = (unsigned short)h;
	XftDrawSetClipRectangles(draw, 0, 0, &r, 1);
}

static void
chevron(Display *dpy, Drawable dst, GC gc, int cx, int cy, int spread,
    int down)
{
	int rise, ytip, ybase;

	rise = spread / 3;
	if (rise < 2)
		rise = 2;
	if (down) {
		ytip = cy + rise;
		ybase = cy - rise;
	} else {
		ytip = cy - rise;
		ybase = cy + rise;
	}
	XDrawLine(dpy, dst, gc, cx - spread, ybase, cx, ytip);
	XDrawLine(dpy, dst, gc, cx, ytip, cx + spread, ybase);
}

static unsigned long
pix(Display *dpy, const char *name)
{
	XColor c, exact;

	if (!XAllocNamedColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)),
	    name, &c, &exact))
		return (BlackPixel(dpy, DefaultScreen(dpy)));
	return (c.pixel);
}

static void
draw_lock(Display *dpy, Window win, GC gc, int x, int y, int s, int locked)
{
	int t = s / 8;
	int bw, bh, bx, by, sw, sx, sy;

	if (t < 2)
		t = 2;
	bw = s * 5 / 8;
	bh = s * 2 / 5;
	bx = x + (s - bw) / 2;
	by = y + s - bh - t;
	sw = bw - 2 * t;
	sx = x + (s - sw) / 2;
	sy = y + t + t / 2;
	XSetLineAttributes(dpy, gc, (unsigned)t, LineSolid, CapButt, JoinMiter);
	XDrawLine(dpy, win, gc, sx, by, sx, sy);
	XDrawLine(dpy, win, gc, sx, sy, sx + sw, sy);
	if (locked)
		XDrawLine(dpy, win, gc, sx + sw, sy, sx + sw, by);
	else
		XDrawLine(dpy, win, gc, sx + sw, sy, sx + sw, sy + 2 * t);
	XDrawRectangle(dpy, win, gc, bx, by, (unsigned)bw, (unsigned)bh);
	XSetLineAttributes(dpy, gc, 0, LineSolid, CapButt, JoinMiter);
}

static void
draw_x(Display *dpy, Window win, GC gc, int x, int y, int s)
{
	int t = s / 8;
	int m = s / 5;

	if (t < 2)
		t = 2;
	XSetLineAttributes(dpy, gc, (unsigned)t, LineSolid, CapButt, JoinMiter);
	XDrawLine(dpy, win, gc, x + m, y + m, x + s - m, y + s - m);
	XDrawLine(dpy, win, gc, x + s - m, y + m, x + m, y + s - m);
	XSetLineAttributes(dpy, gc, 0, LineSolid, CapButt, JoinMiter);
}

void
panel_paint(Display *dpy, Window win, XftDraw *draw, XftFont *font,
    XftFont *small, XftColor *fg, XftColor *dim, XftColor *accent,
    struct panel_ui *ui)
{
	GC gc;
	XGCValues gv;
	Pixmap pm = None;
	Window onscreen = win;
	XftDraw *nd;
	int i, max, line_h, sbase, smbase, caret_y, caret_h;
	unsigned long box_px, head_px, line_px, foot_px;
	char pretty[BH_CHORD_MAX];
	const char *foot;

	panel_layout(dpy, ui, font, small);
	pm = XCreatePixmap(dpy, win, (unsigned)ui->win_w, (unsigned)ui->win_h,
	    (unsigned)DefaultDepth(dpy, DefaultScreen(dpy)));
	if (pm != None) {
		nd = XftDrawCreate(dpy, pm,
		    DefaultVisual(dpy, DefaultScreen(dpy)),
		    DefaultColormap(dpy, DefaultScreen(dpy)));
		if (nd == NULL) {
			XFreePixmap(dpy, pm);
			pm = None;
		} else {
			win = (Window)pm;
			draw = nd;
		}
	}
	box_px = pix(dpy, "#10161d");
	head_px = pix(dpy, "#1b222b");
	line_px = pix(dpy, "#3d4c5c");
	foot_px = pix(dpy, "#161b22");
	gv.foreground = foot_px;
	gc = XCreateGC(dpy, win, GCForeground, &gv);
	XFillRectangle(dpy, win, gc, 0, 0, (unsigned)ui->win_w,
	    (unsigned)ui->win_h);
	sbase = ui->search_y + (ui->search_h + font->ascent - font->descent) / 2;
	smbase = ui->search_y +
	    (ui->search_h + small->ascent - small->descent) / 2;
	caret_y = sbase - font->ascent;
	caret_h = font->ascent + font->descent;
	if (caret_y < ui->search_y + 2) caret_y = ui->search_y + 2;
	if (caret_y + caret_h > ui->search_y + ui->search_h - 2) caret_h = ui->search_y + ui->search_h - 2 - caret_y;

	XSetForeground(dpy, gc, box_px);
	XFillRectangle(dpy, win, gc, ui->search_x, ui->search_y,
	    (unsigned)ui->search_w, (unsigned)ui->search_h);
	XSetForeground(dpy, gc, accent->pixel);
	XDrawRectangle(dpy, win, gc, ui->search_x, ui->search_y,
	    (unsigned)ui->search_w - 1, (unsigned)ui->search_h - 1);

	if (ui->capture == -1)
		text(draw, accent, small, ui->search_x + 12, smbase,
		    "Press the chord for this panel. Escape cancels.");
	else if (ui->capture >= 0)
		text(draw, accent, small, ui->search_x + 12, smbase,
		    "Press the new chord. Escape cancels.");
	else if (ui->pwmode) {
		char stars[64];
		size_t n = strlen(ui->pw);
		size_t k;

		if (n >= sizeof(stars))
			n = sizeof(stars) - 1;
		for (k = 0; k < n; k++)
			stars[k] = '*';
		stars[n] = '\0';
		if (n == 0)
			text(draw, dim, font, ui->search_x + 18, sbase,
			    "Password");
		else
			text(draw, fg, font, ui->search_x + 12, sbase, stars);
		if (ui->caret_on) {
			int cx = ui->search_x + 12 +
			    panel_text_w(dpy, font, stars, (int)n);
			XFillRectangle(dpy, win, gc, cx, caret_y, 2,
			    (unsigned)caret_h);
		}
	} else if (ui->pwbad != 0) {
		text(draw, dim, font, ui->search_x + 18, sbase,
		    "Password incorrect.");
	} else {
		int cx;

		if (ui->query[0] == '\0')
			text(draw, dim, font, ui->search_x + 18, sbase,
			    "Search chords and actions");
		else
			text(draw, fg, font, ui->search_x + 12, sbase,
			    ui->query);
		if (ui->caret_on) {
			int qn = (int)strlen(ui->query);

			if (ui->qpos < 0)
				ui->qpos = 0;
			if (ui->qpos > qn)
				ui->qpos = qn;
			cx = ui->search_x + 12;
			if (ui->qpos > 0)
				cx += panel_text_w(dpy, font, ui->query, ui->qpos);
			XFillRectangle(dpy, win, gc, cx, caret_y, 2,
			    (unsigned)caret_h);
		}
	}
	if (bh_greeter) {
		int side = ui->search_h;
		int gap_x = ui->search_x + ui->search_w;
		int gap_w = ui->win_w - gap_x;
		int lx = gap_x + (gap_w - side) / 2;

		XSetForeground(dpy, gc, accent->pixel);
		draw_x(dpy, win, gc, lx, ui->search_y, side);
	}

	XSetForeground(dpy, gc, head_px);
	XFillRectangle(dpy, win, gc, 0, ui->header_y, (unsigned)ui->win_w,
	    (unsigned)ui->header_h);
	text(draw, ui->sort_col == 0 ? accent : dim, small, ui->name_x,
	    ui->header_y + small->ascent + 6, "Name");
	text(draw, ui->sort_col == 1 ? accent : dim, small, ui->chord_x,
	    ui->header_y + small->ascent + 6, "Key chord");
	if (ui->sort_col == 0 || ui->sort_col == 1) {
		const char *lab = ui->sort_col == 0 ? "Name" : "Key chord";
		int lab_x = ui->sort_col == 0 ? ui->name_x : ui->chord_x;
		int base = ui->header_y + small->ascent + 6;
		int spread = 6;

		XSetForeground(dpy, gc, accent->pixel);
		chevron(dpy, win, gc,
		    lab_x + panel_text_w(dpy, small, lab, -1) + 12 + spread,
		    base - (small->ascent - small->descent) / 2,
		    spread, ui->sort_rev);
	}

	{
		XRectangle r;

		r.x = 0;
		r.y = (short)ui->body_y;
		r.width = (unsigned short)ui->win_w;
		r.height = (unsigned short)ui->body_h;
		XSetClipRectangles(dpy, gc, 0, 0, &r, 1, YXBanded);
	}
	clip(draw, 0, ui->body_y, ui->win_w, ui->body_h);
	line_h = small->ascent + small->descent + 2;
	for (i = 0; i < ui->nlay; i++) {
		int idx = ui->order[i];
		int sy = ui->body_y + ui->lay_y[i] - ui->scroll;
		int base = sy + 6 + font->ascent;
		int mark = (ui->capture == idx);
		const char *desc, *ch;
		char lines[6][120];
		int nlines, li;
		const struct bh_plugin *p = NULL;

		if (sy + ui->lay_h[i] < ui->body_y ||
		    sy >= ui->body_y + ui->body_h)
			continue;
		if (ROW_HEAD(idx)) {
			int slot = -idx - 2;
			int by = sy + ui->lay_h[i] - 4;

			if (slot >= 0 && slot < ui->ngroups) {
				text(draw, accent, small, 14,
				    by - small->descent, ui->groups[slot]);
				XSetForeground(dpy, gc, line_px);
				XDrawLine(dpy, win, gc, 14, by,
				    ui->win_w - 14, by);
			}
			continue;
		}
		if (idx == ROW_ALT) {
			desc = bh_greeter ? BH_PANEL_ALT_DESC_GREETER :
			    BH_PANEL_ALT_DESC;
			ch = ui->set->panel_alt_chord;
		} else if (idx < 0) {
			desc = panel_blurb(NULL);
			ch = ui->set->panel_chord;
		} else {
			p = ui->set->at[idx];
			desc = panel_blurb(p);
			ch = p->chord;
		}
		bh_chord_pretty(ch, pretty, sizeof(pretty));
		XSetForeground(dpy, gc, line_px);
		if (i + 1 < ui->nlay && !ROW_HEAD(ui->order[i + 1]))
			XDrawLine(dpy, win, gc, 12, sy + ui->lay_h[i] - 4,
			    ui->win_w - 12, sy + ui->lay_h[i] - 4);
		if (!ui->readonly && idx >= 0)
			draw_check(dpy, draw, fg, font, 18, base, p->enabled);
		else if (!ui->readonly && idx == ROW_ALT)
			draw_check(dpy, draw, fg, font, 18, base,
			    ui->set->panel_alt_on);
		{
			char names[4][120];
			int nt, ti;
			int name_w = ui->chord_x - ui->name_x - 12;

			nt = panel_wrap(dpy, font, panel_row_label(ui, idx),
			    name_w, names, 4);
			if (nt < 1)
				nt = 1;
			clip(draw, ui->name_x, ui->body_y,
			    ui->chord_x - ui->name_x - 8, ui->body_h);
			for (ti = 0; ti < nt; ti++)
				text(draw, fg, font, ui->name_x,
				    base + ti * (font->ascent + font->descent),
				    names[ti]);
		}
		clip(draw, ui->chord_x, ui->body_y,
		    ui->act_x - ui->chord_x - 8, ui->body_h);
		text(draw, accent, small, ui->chord_x,
		    base - (font->ascent - small->ascent) / 2, pretty);
		clip(draw, 0, ui->body_y, ui->win_w, ui->body_h);
		if (!ui->readonly) {
			text(draw, mark ? accent : dim, small, ui->act_x, base,
			    "Set keys");
			if (idx == ROW_PANEL) {
				if (strcmp(ui->set->panel_chord,
				    BH_PANEL_CHORD) != 0 &&
				    strcmp(ui->set->panel_chord, "Super+?") != 0)
					text(draw, dim, small, ui->reset_x,
					    base, "Reset");
			} else if (idx == ROW_ALT) {
				if (strcmp(ui->set->panel_alt_chord,
				    BH_PANEL_ALT_CHORD) != 0 &&
				    strcmp(ui->set->panel_alt_chord, "Super+/") != 0)
					text(draw, dim, small, ui->reset_x,
					    base, "Reset");
			} else if (strcmp(p->chord, p->suggested) != 0)
				text(draw, dim, small, ui->reset_x, base,
				    "Reset");
		}
		if (p != NULL && !p->listen)
			text(draw, dim, small, ui->act_x,
			    base + 4 + small->ascent + small->descent,
			    "window manager");
		nlines = panel_wrap(dpy, small, desc,
		    ui->act_x - ui->name_x - 12, lines, 6);
		{
			char names[4][120];
			int nt = panel_wrap(dpy, font, panel_row_label(ui, idx),
			    ui->chord_x - ui->name_x - 12, names, 4);
			int extra = 0;

			if (nt > 1)
				extra = (nt - 1) * (font->ascent + font->descent);
			clip(draw, ui->name_x, ui->body_y,
			    ui->act_x - ui->name_x - 8, ui->body_h);
			for (li = 0; li < nlines; li++)
				text(draw, dim, small, ui->name_x,
				    base + extra + font->descent + 8 +
				    small->ascent + li * line_h, lines[li]);
		}
		clip(draw, 0, ui->body_y, ui->win_w, ui->body_h);
	}
	XftDrawSetClip(draw, NULL);
	XSetClipMask(dpy, gc, None);

	max = ui->content_h - ui->body_h;
	XSetForeground(dpy, gc, accent->pixel);
	if (ui->scroll > 0)
		chevron(dpy, win, gc, ui->win_w / 2,
		    (ui->header_y + ui->header_h + ui->body_y) / 2, 16, 0);
	if (max > 0 && ui->scroll < max)
		chevron(dpy, win, gc, ui->win_w / 2,
		    (ui->body_y + ui->body_h + ui->footer_y) / 2, 16, 1);

	XSetForeground(dpy, gc, foot_px);
	XFillRectangle(dpy, win, gc, 0, ui->footer_y, (unsigned)ui->win_w,
	    (unsigned)ui->footer_h);
	if (ui->pwmode)
		foot = "Enter unlocks. Escape cancels.";
	else if (bh_greeter && ui->readonly)
		foot = "Locked. The lock makes this list writable.";
	else
		foot = "Esc to close. Check enables. Set keys, then press the chord. Reset restores it.";
	{
		int clip_w = bh_greeter ? ui->win_w - ui->search_h - 40 :
		    ui->win_w - 150;

		clip(draw, 12, ui->footer_y, clip_w, ui->footer_h);
	}
	text(draw, dim, small, 14, ui->footer_y + small->ascent + 8, foot);
	XftDrawSetClip(draw, NULL);
	if (bh_greeter) {
		int side = ui->search_h;
		int lx = ui->win_w - 14 - side;
		int ly = ui->footer_y + (ui->footer_h - side) / 2;

		XSetForeground(dpy, gc, accent->pixel);
		draw_lock(dpy, win, gc, lx, ly, side, ui->readonly);
	} else
		text(draw, accent, small, ui->win_w - 120,
		    ui->footer_y + small->ascent + 8,
		    ui->managed ? "Overlay" : "Window");
	if (pm != None) {
		XCopyArea(dpy, pm, onscreen, gc, 0, 0, (unsigned)ui->win_w,
		    (unsigned)ui->win_h, 0, 0);
		XftDrawDestroy(draw);
		XFreePixmap(dpy, pm);
	}
	XFreeGC(dpy, gc);
}

int
panel_hit(struct panel_ui *ui, int x, int y, int *row)
{
	int i, sy;

	*row = -2;
	if (y < 0 || x < 0 || x >= ui->win_w || y >= ui->win_h)
		return (HIT_OUT);
	if (bh_greeter && x >= ui->search_x + ui->search_w &&
	    y >= ui->search_y && y < ui->search_y + ui->search_h)
		return (HIT_CLOSE);
	if (y >= ui->search_y && y < ui->search_y + ui->search_h &&
	    x >= ui->search_x && x < ui->search_x + ui->search_w)
		return (HIT_SEARCH);
	if (y >= ui->footer_y) {
		if (bh_greeter && x >= ui->win_w - 14 - ui->search_h)
			return (HIT_UNLOCK);
		if (!bh_greeter && x >= ui->win_w - 180)
			return (HIT_MANAGE);
		return (HIT_NONE);
	}
	if (y >= ui->header_y && y < ui->header_y + ui->header_h) {
		if (x >= ui->chord_x && x < ui->act_x)
			return (HIT_HEAD_CHORD);
		if (x >= ui->name_x && x < ui->chord_x)
			return (HIT_HEAD_NAME);
		return (HIT_NONE);
	}
	if (y < ui->body_y || y >= ui->body_y + ui->body_h)
		return (HIT_NONE);
	sy = y - ui->body_y + ui->scroll;
	for (i = 0; i < ui->nlay; i++) {
		int idx;

		if (sy < ui->lay_y[i] || sy >= ui->lay_y[i] + ui->lay_h[i])
			continue;
		idx = ui->order[i];
		if (ROW_HEAD(idx))
			return (HIT_NONE);
		*row = idx;
		if (ui->readonly)
			return (HIT_NONE);
		if (idx == ROW_PANEL) {
			if (x >= ui->act_x && x < ui->reset_x)
				return (HIT_SET);
			if (x >= ui->reset_x &&
			    strcmp(ui->set->panel_chord, BH_PANEL_CHORD) != 0 &&
			    strcmp(ui->set->panel_chord, "Super+?") != 0)
				return (HIT_RESET);
			return (HIT_NONE);
		}
		if (x < ui->name_x - 8)
			return (HIT_CHECK);
		if (x >= ui->act_x && x < ui->reset_x)
			return (HIT_SET);
		if (x >= ui->reset_x) {
			int show;

			if (idx == ROW_ALT)
				show = strcmp(ui->set->panel_alt_chord,
				    BH_PANEL_ALT_CHORD) != 0 &&
				    strcmp(ui->set->panel_alt_chord,
				    "Super+/") != 0;
			else
				show = strcmp(ui->set->at[idx]->chord,
				    ui->set->at[idx]->suggested) != 0;
			if (show)
				return (HIT_RESET);
		}
		return (HIT_NONE);
	}
	return (HIT_NONE);
}
