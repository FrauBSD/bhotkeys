/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * --print-keys mate. <ModN><Control><Alt><Shift>k, where ModN is
 * the modifier holding Super_L, as xmodmap -pm shows it. MATE grabs
 * that bit. <Super> is a virtual mask and the key still reaches the
 * client.
 */

#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include "print_priv.h"

static const char *
mate_super(void)
{
	static char tag[8];
	Display *dpy;
	XModifierKeymap *map;
	KeyCode kc;
	int i, j, found = 0;

	if (tag[0] != '\0')
		return (tag);
	strlcpy(tag, "<Mod4>", sizeof(tag));
	dpy = XOpenDisplay(NULL);
	if (dpy == NULL)
		return (tag);
	map = XGetModifierMapping(dpy);
	if (map != NULL) {
		for (i = Mod1MapIndex; i <= Mod5MapIndex && !found; i++) {
			for (j = 0; j < map->max_keypermod; j++) {
				kc = map->modifiermap[i * map->max_keypermod +
				    j];
				if (kc == 0)
					continue;
				if (XkbKeycodeToKeysym(dpy, kc, 0, 0) !=
				    XK_Super_L)
					continue;
				snprintf(tag, sizeof(tag), "<Mod%d>",
				    i - Mod1MapIndex + 1);
				found = 1;
				break;
			}
		}
		XFreeModifiermap(map);
	}
	XCloseDisplay(dpy);
	return (tag);
}

int
bh_spell_mate(char *dst, size_t len, struct bh_key *k)
{
	bh_join_mods(dst, len, k, mate_super(), "<Control>", "<Alt>",
	    "<Shift>", "");
	strlcat(dst, k->name, len);
	return (1);
}
