/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * The greeter has no window manager, so the list cannot be a managed
 * window under Florence. Keep Florence above the overlay and the shade.
 * xlogin keeps the keyboard grab, so key delivery is separate.
 */

#include <strings.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "panel_priv.h"

struct seen_win {
	Window	w;
	int	flo;
};

static struct seen_win seen[128];
static int nseen;
static Window shade_cached;

void
panel_shade_unmap(Display *dpy)
{
	if (shade_cached != None)
		XUnmapWindow(dpy, shade_cached);
}

static int
class_is_florence(Display *dpy, Window w)
{
	XClassHint h;
	int yes = 0;

	if (!XGetClassHint(dpy, w, &h))
		return (0);
	if ((h.res_name != NULL && strcasecmp(h.res_name, "florence") == 0) ||
	    (h.res_class != NULL && strcasecmp(h.res_class, "florence") == 0))
		yes = 1;
	if (h.res_name != NULL)
		XFree(h.res_name);
	if (h.res_class != NULL)
		XFree(h.res_class);
	return (yes);
}

static int
window_is_florence(Display *dpy, Window w)
{
	Window root, parent, *kids = NULL;
	unsigned int i, n = 0;

	if (class_is_florence(dpy, w))
		return (1);
	if (!XQueryTree(dpy, w, &root, &parent, &kids, &n))
		return (0);
	for (i = 0; kids != NULL && i < n; i++) {
		if (class_is_florence(dpy, kids[i])) {
			XFree(kids);
			return (1);
		}
	}
	if (kids != NULL)
		XFree(kids);
	return (0);
}

static int
cached_florence(Display *dpy, Window w)
{
	int i;

	for (i = 0; i < nseen; i++) {
		if (seen[i].w == w)
			return (seen[i].flo);
	}
	if (nseen >= (int)(sizeof(seen) / sizeof(seen[0])))
		return (window_is_florence(dpy, w));
	seen[nseen].w = w;
	seen[nseen].flo = window_is_florence(dpy, w);
	return (seen[nseen++].flo);
}

static Window
find_shade(Display *dpy, Window *kids, unsigned int n)
{
	unsigned int i;
	int scr = DefaultScreen(dpy);
	unsigned int sw = (unsigned)DisplayWidth(dpy, scr);
	unsigned int sh = (unsigned)DisplayHeight(dpy, scr);

	for (i = 0; i < n; i++) {
		XWindowAttributes a;

		if (!XGetWindowAttributes(dpy, kids[i], &a))
			continue;
		if (a.class == InputOnly && a.override_redirect &&
		    (unsigned)a.width == sw && (unsigned)a.height == sh)
			return (kids[i]);
	}
	return (None);
}

void
panel_below_osk(Display *dpy, Window panel)
{
	Window root, parent, *kids = NULL, shade;
	unsigned int i, n = 0;
	int panel_i = -1, shade_i = -1, top_flo = -1;
	Window stack[40];
	int ns = 0;

	if (!bh_greeter || panel == None)
		return;
	root = RootWindow(dpy, DefaultScreen(dpy));
	if (!XQueryTree(dpy, root, &root, &parent, &kids, &n) || kids == NULL)
		return;
	if (shade_cached == None)
		shade_cached = find_shade(dpy, kids, n);
	shade = shade_cached;
	for (i = 0; i < n; i++) {
		if (kids[i] == panel)
			panel_i = (int)i;
		else if (shade != None && kids[i] == shade)
			shade_i = (int)i;
		else if (cached_florence(dpy, kids[i]))
			top_flo = (int)i;
	}
	if (top_flo < 0 || (panel_i >= 0 && top_flo > panel_i &&
	    (shade_i < 0 || (top_flo > shade_i && panel_i > shade_i)))) {
		XFree(kids);
		return;
	}
	for (i = 0; i < n; i++) {
		if (kids[i] != panel && kids[i] != shade &&
		    cached_florence(dpy, kids[i]))
			XRaiseWindow(dpy, kids[i]);
	}
	for (i = n; i-- > 0 && ns < 36; ) {
		if (kids[i] != panel && kids[i] != shade &&
		    cached_florence(dpy, kids[i]))
			stack[ns++] = kids[i];
	}
	if (ns == 0) {
		XFree(kids);
		return;
	}
	stack[ns++] = panel;
	if (shade != None)
		stack[ns++] = shade;
	XRestackWindows(dpy, stack, ns);
	XFree(kids);
}
