#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "panel_priv.h"

static struct panel_ui ui;
static struct bh_set set;
static Window shade = None;
static int focus_tries;
static volatile sig_atomic_t nudged;
static volatile sig_atomic_t quit_req;

static void
on_usr1(int sig __unused)
{
	nudged = 1;
}

static void
on_term(int sig __unused)
{
	quit_req = 1;
}

static int xerr(Display *d, XErrorEvent *e) { (void)d; (void)e; return (0); }

static void
panel_pidfile(int on)
{
	char path[64];
	FILE *fp;

	snprintf(path, sizeof(path), "/tmp/bhotkeys-panel.%ld", (long)getuid());
	if (!on) {
		unlink(path);
		return;
	}
	fp = fopen(path, "w");
	if (fp == NULL)
		return;
	fprintf(fp, "%ld %d\n", (long)getpid(), ui.managed ? 1 : 0);
	fclose(fp);
}

static void
step_aside(Display *dpy)
{
	if (shade != None)
		XUnmapWindow(dpy, shade);
	XUngrabPointer(dpy, CurrentTime);
	XUngrabKeyboard(dpy, CurrentTime);
	XFlush(dpy);
}

static long
now_ms(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000L + tv.tv_usec / 1000L);
}

static int
caret_wait(long last, long now, int *on)
{
	long idle = now - last;
	int left;

	if (idle < 1000) {
		*on = 1;
		return ((int)(1000 - idle));
	}
	*on = ((idle / 1000) % 2) == 0;
	left = (int)(1000 - (idle % 1000));
	if (left < 1)
		left = 1;
	return (left);
}

static Window
make_shade(Display *dpy, int scr)
{
	XSetWindowAttributes attr;

	attr.override_redirect = True;
	attr.event_mask = ButtonPressMask | KeyPressMask;
	return (XCreateWindow(dpy, RootWindow(dpy, scr), 0, 0,
	    (unsigned)DisplayWidth(dpy, scr),
	    (unsigned)DisplayHeight(dpy, scr), 0, CopyFromParent, InputOnly,
	    CopyFromParent, CWOverrideRedirect | CWEventMask, &attr));
}

static Window
make_win(Display *dpy, int scr, int managed)
{
	XSetWindowAttributes attr;
	XWMHints wmh;
	XSizeHints sh;
	XColor bg, exact;
	Atom kind, normal;
	int x, y, px;
	Window win;

	panel_place(dpy, scr, &x, &y, &ui.win_w, &ui.win_h, &px);
	ui.font_px = px;
	if (!XAllocNamedColor(dpy, DefaultColormap(dpy, scr), "#161b22",
	    &bg, &exact))
		bg.pixel = BlackPixel(dpy, scr);
	attr.background_pixel = bg.pixel;
	attr.override_redirect = managed ? False : True;
	attr.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
	    Button1MotionMask | PointerMotionMask | KeyPressMask |
	    StructureNotifyMask | FocusChangeMask | PropertyChangeMask;
	win = XCreateWindow(dpy, RootWindow(dpy, scr), x, y,
	    (unsigned)ui.win_w, (unsigned)ui.win_h, 0, CopyFromParent,
	    InputOutput, CopyFromParent,
	    CWBackPixel | CWOverrideRedirect | CWEventMask, &attr);
	wmh.flags = InputHint;
	wmh.input = True;
	XSetWMHints(dpy, win, &wmh);
	sh.flags = USPosition | USSize | PPosition | PSize;
	sh.x = x;
	sh.y = y;
	sh.width = ui.win_w;
	sh.height = ui.win_h;
	XSetWMNormalHints(dpy, win, &sh);
	if (managed) {
		kind = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
		normal = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);
		XChangeProperty(dpy, win, kind, XA_ATOM, 32, PropModeReplace,
		    (unsigned char *)&normal, 1);
	}
	return (win);
}

static void
focus_win(Display *dpy, Window win)
{
	if (!bh_greeter)
		XSetInputFocus(dpy, win, RevertToPointerRoot, CurrentTime);
}

static void
sort_flip(struct panel_ui *u, int col)
{
	if (u->sort_col != col) {
		u->sort_col = col;
		u->sort_rev = 0;
	} else if (!u->sort_rev) {
		u->sort_rev = 1;
	} else {
		u->sort_col = -1;
		u->sort_rev = 0;
	}
	u->scroll = 0;
	u->saved_scroll = 0;
	panel_refilter(u);
	panel_ui_save(u);
}

static void
retarget(Display *dpy, int scr, Window *win, XftDraw **draw, Visual *vis,
    Colormap cmap)
{
	XEvent junk;
	XClassHint hint = { "bhotkeys-panel", "Bhotkeys" };

	ui.managed = !ui.managed;
	XUngrabKeyboard(dpy, CurrentTime);
	XUngrabPointer(dpy, CurrentTime);
	panel_capture(0);
	ui.capture = -2;
	if (shade != None)
		XUnmapWindow(dpy, shade);
	XDestroyWindow(dpy, *win);
	XftDrawDestroy(*draw);
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, ButtonPressMask, &junk))
		;
	*win = make_win(dpy, scr, ui.managed);
	XStoreName(dpy, *win, "bhotkeys");
	XSetClassHint(dpy, *win, &hint);
	XMapRaised(dpy, *win);
	*draw = XftDrawCreate(dpy, *win, vis, cmap);
	panel_touch_select(dpy, *win);
	if (!ui.managed && shade != None) {
		XMapWindow(dpy, shade);
		XRaiseWindow(dpy, *win);
		focus_win(dpy, *win);
	}
	panel_pidfile(1);
}

static void
handle_hit(Display *dpy, int scr, Window *winp, XftDraw **drawp,
    Visual *vis, Colormap cmap, int what, int row)
{
	if (panel_pw_click(dpy, *winp, &ui, what))
		return;
	if (what == HIT_MANAGE) {
		retarget(dpy, scr, winp, drawp, vis, cmap);
		return;
	}
	if (what == HIT_HEAD_NAME)
		sort_flip(&ui, 0);
	else if (what == HIT_HEAD_CHORD)
		sort_flip(&ui, 1);
	else if (what == HIT_CHECK && (row >= 0 || row == ROW_ALT)) {
		if (row == ROW_ALT)
			set.panel_alt_on = !set.panel_alt_on;
		else
			set.at[row]->enabled = !set.at[row]->enabled;
		panel_save(&ui);
		ui.capture = -2;
	} 	else if (what == HIT_RESET) {
		if (row == ROW_PANEL)
			strlcpy(set.panel_chord, BH_PANEL_CHORD,
			    sizeof(set.panel_chord));
		else if (row == ROW_ALT)
			strlcpy(set.panel_alt_chord, BH_PANEL_ALT_CHORD,
			    sizeof(set.panel_alt_chord));
		else if (row >= 0)
			strlcpy(set.at[row]->chord,
			    set.at[row]->suggested,
			    sizeof(set.at[row]->chord));
		panel_save(&ui);
		ui.capture = -2;
	} else if (what == HIT_SET) {
		ui.capture = row;
		ui.pwmode = 0;
		panel_capture(1);
		XGrabKeyboard(dpy, *winp, False, GrabModeAsync, GrabModeAsync,
		    CurrentTime);
	}
}

int
main(int argc, char **argv)
{
	Display *dpy;
	int scr, i, row, what;
	Window win;
	XEvent ev;
	XftDraw *draw;
	XftFont *font, *small;
	XftColor fg, dim, accent;
	Colormap cmap;
	Visual *vis;
	char face[32], face_sm[32];
	XClassHint hint = { "bhotkeys-panel", "Bhotkeys" };
	long last_key;

	ui.readonly = 0;
	ui.capture = -2;
	ui.managed = 0;
	ui.caret_on = 1;
	if (panel_admin(argc, argv, &i))
		return (i);
	if (bh_greeter)
		ui.readonly = 1;
	dpy = XOpenDisplay(NULL);
	if (dpy == NULL)
		return (1);
	bh_dpy = dpy;
	bh_load(&set);
	ui.set = &set;
	panel_ui_load(&ui);
	panel_refilter(&ui);
	XSetErrorHandler(xerr);
	scr = DefaultScreen(dpy);
	cmap = DefaultColormap(dpy, scr);
	vis = DefaultVisual(dpy, scr);
	shade = make_shade(dpy, scr);
	win = make_win(dpy, scr, 0);
	XStoreName(dpy, win, "bhotkeys");
	XSetClassHint(dpy, win, &hint);
	if (!ui.managed)
		XMapWindow(dpy, shade);
	XMapRaised(dpy, win);
	snprintf(face, sizeof(face), "sans-%d", ui.font_px);
	snprintf(face_sm, sizeof(face_sm), "sans-%d", ui.font_px * 2 / 3);
	font = XftFontOpenName(dpy, scr, face);
	small = XftFontOpenName(dpy, scr, face_sm);
	if (font == NULL || small == NULL)
		return (1);
	draw = XftDrawCreate(dpy, win, vis, cmap);
	panel_touch_select(dpy, win);
	XftColorAllocName(dpy, vis, cmap, "#f0f4f8", &fg);
	XftColorAllocName(dpy, vis, cmap, "#9aa8b5", &dim);
	XftColorAllocName(dpy, vis, cmap, "#58a8e0", &accent);
	focus_win(dpy, win);
	panel_keys_hold(dpy, win, CurrentTime);
	last_key = now_ms();
	{
		struct sigaction sa;

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = on_usr1;
		sigaction(SIGUSR1, &sa, NULL);
		sa.sa_handler = on_term;
		sigaction(SIGTERM, &sa, NULL);
		sigaction(SIGINT, &sa, NULL);
	}
	panel_pidfile(1);

	for (;;) {
		int note;

		if (quit_req || ui.close_req || panel_take_quit(dpy, &ui))
			break;
		note = panel_pw_note(&ui, now_ms());
		if (note < 0 || panel_record_dirty())
			panel_paint(dpy, win, draw, font, small, &fg, &dim,
			    &accent, &ui);
		if (nudged) {
			nudged = 0;
			ui.yield = 1;
			step_aside(dpy);
		}
		if (XPending(dpy) == 0) {
			int shown = ui.caret_on;
			int wait;

			wait = caret_wait(last_key, now_ms(), &ui.caret_on);
			if (shade != None && wait > 40)
				wait = 40;
			note = panel_pw_note(&ui, now_ms());
			if (note > 0 && wait > note)
				wait = note;
			if (panel_wait(dpy, &ui, wait) == 0) {
				caret_wait(last_key, now_ms(), &ui.caret_on);
				panel_keys_hold(dpy, win, CurrentTime);
			}
			note = panel_pw_note(&ui, now_ms());
			if (note < 0 || panel_record_dirty() ||
			    (ui.caret_on != shown && ui.capture < 0))
				panel_paint(dpy, win, draw, font, small, &fg,
				    &dim, &accent, &ui);
			continue;
		}
		XNextEvent(dpy, &ev);
		if (ev.type == Expose && ev.xexpose.count == 0 &&
		    ev.xexpose.window == win) {
			panel_paint(dpy, win, draw, font, small, &fg, &dim,
			    &accent, &ui);
		} else if (ev.type == MapNotify && ev.xmap.window == win) {
			focus_win(dpy, win);
			panel_keys_hold(dpy, win, CurrentTime);
		} else if (ev.type == ConfigureNotify &&
		    ev.xconfigure.window == win) {
			ui.win_w = ev.xconfigure.width;
			ui.win_h = ev.xconfigure.height;
		} else if (ev.type == FocusOut && ev.xfocus.window == win &&
		    !ui.managed && !ui.yield &&
		    ev.xfocus.mode != NotifyGrab && focus_tries < 3) {
			focus_tries++;
			if (bh_greeter)
				panel_keys_hold(dpy, win, CurrentTime);
			else
				focus_win(dpy, win);
		} else if (ev.type == GenericEvent) {
			int kind = panel_gesture(dpy, &ev, &ui);

			if (ui.pwmode && ui.drag && kind != 0)
				panel_pw_dismiss(dpy, &ui);
			if (kind == 4 || (kind == 3 && ui.drag)) {
				int moved = kind == 4 ? 0 : ui.drag_moved;

				if (!moved) {
					what = panel_hit(&ui, ui.drag_x,
					    ui.drag_y, &row);
					handle_hit(dpy, scr, &win, &draw, vis,
					    cmap, what, row);
				}
				if (kind == 3) {
					ui.drag = 0;
					ui.xtouch = 2;
					panel_ui_save(&ui);
				}
			}
			if (kind >= 2 && kind <= 4)
				panel_paint(dpy, win, draw, font, small, &fg,
				    &dim, &accent, &ui);
		} else if (ev.type == MotionNotify && ui.xtouch) {
			/* Touch updates own the drag. */
		} else if (ev.type == MotionNotify && ui.drag && !ui.xtouch &&
		    ev.xmotion.window == win) {
			int dx = ev.xmotion.x - ui.drag_x;
			int dy = ev.xmotion.y - ui.drag_y;

			if (dx < 0)
				dx = -dx;
			if (dy < 0)
				dy = -dy;
			if (dx > 6 || dy > 6)
				ui.drag_moved = 1;
			if (ui.drag_moved) {
				panel_scroll_set(&ui, ui.drag_scroll -
				    (ev.xmotion.y - ui.drag_y));
				panel_paint(dpy, win, draw, font, small, &fg,
				    &dim, &accent, &ui);
			}
		} else if ((ev.type == ButtonPress || ev.type == ButtonRelease) &&
		    ui.xtouch) {
			if (ev.type == ButtonRelease)
				ui.xtouch = 0;
		} else if (ev.type == ButtonRelease &&
		    ev.xbutton.window == win && ev.xbutton.button == 1 &&
		    ui.drag) {
			int px = ui.drag_x, py = ui.drag_y, moved = ui.drag_moved;

			ui.drag = 0;
			if (!moved) {
				what = panel_hit(&ui, px, py, &row);
				handle_hit(dpy, scr, &win, &draw, vis, cmap,
				    what, row);
			}
			panel_ui_save(&ui);
			panel_paint(dpy, win, draw, font, small, &fg, &dim,
			    &accent, &ui);
		} else if (!bh_greeter && ev.type == ButtonPress &&
		    shade != None && ev.xbutton.window == shade &&
		    !ui.managed) {
			if (panel_pw_dismiss(dpy, &ui)) {
				focus_win(dpy, win);
				panel_paint(dpy, win, draw, font, small, &fg,
				    &dim, &accent, &ui);
			} else
				break;
		} else if (ev.type == ButtonPress && ev.xbutton.window == win) {
			if (ev.xbutton.button == 4 || ev.xbutton.button == 5) {
				panel_scroll(&ui, ev.xbutton.button == 4 ?
				    -1 : 1, 0);
				panel_ui_save(&ui);
			} else if (ev.xbutton.button == 1) {
				ui.yield = 0;
				panel_keys_hold(dpy, win, ev.xbutton.time);
				what = panel_hit(&ui, ev.xbutton.x,
				    ev.xbutton.y, &row);
				if (what == HIT_OUT && !ui.managed && !bh_greeter)
					break;
				if (ui.pwmode && what != HIT_SEARCH &&
				    what != HIT_UNLOCK)
					panel_pw_dismiss(dpy, &ui);
				focus_win(dpy, win);
				if (ev.xbutton.y >= ui.body_y &&
				    ev.xbutton.y < ui.footer_y) {
					ui.drag = 1;
					ui.drag_x = ev.xbutton.x;
					ui.drag_y = ev.xbutton.y;
					ui.drag_scroll = ui.scroll;
					ui.drag_moved = 0;
				} else
					handle_hit(dpy, scr, &win, &draw, vis,
					    cmap, what, row);
			}
			panel_paint(dpy, win, draw, font, small, &fg, &dim,
			    &accent, &ui);
		} else if (ev.type == KeyPress && !panel_record_on()) {
			last_key = now_ms();
			ui.caret_on = 1;
			focus_tries = 0;
			if (panel_on_key(dpy, &ev.xkey, &ui))
				break;
			if (ui.drop_grab) {
				ui.drop_grab = 0;
				ui.yield = 1;
				if (shade != None)
					XUnmapWindow(dpy, shade);
				XUngrabPointer(dpy, CurrentTime);
				XUngrabKeyboard(dpy, CurrentTime);
				XFlush(dpy);
			}
			if (ui.release_keys) {
				ui.release_keys = 0;
				if (!bh_greeter)
					XUngrabKeyboard(dpy, CurrentTime);
				focus_win(dpy, win);
			}
			panel_paint(dpy, win, draw, font, small, &fg, &dim,
			    &accent, &ui);
		}
	}
	panel_ui_save(&ui);
	panel_capture(0);
	panel_pidfile(0);
	XCloseDisplay(dpy);
	return (0);
}
