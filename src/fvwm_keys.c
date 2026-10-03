/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Print the chord list as fvwm Key lines so a helper can hand them
 * to FvwmCommand. The output is the list the panel shows in this
 * session.
 *
 * A listen 0 plugin is a window-manager key: its action is the
 * plugin command, run through bhotkeys-bvwm-run. A listen 1 plugin
 * is a listener key: its action is Nop, so the window manager grabs
 * the chord and the focused client does not see it, while the
 * listener still does. A disabled plugin, either kind, prints the
 * action "-", which unbinds the chord. Unbinds print first so a
 * disabled plugin cannot release a chord an enabled one needs.
 * A listen 0 plugin with no command is not printed at all.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "bhotkeys.h"

#define ACT_UNBIND	"-"
#define ACT_NOP		"Nop"

static void
emit_one(const char *name, const char *mods, const char *act)
{
	if (strcmp(act, ACT_UNBIND) == 0 || strcmp(act, ACT_NOP) == 0)
		printf("Key %s A %s %s\n", name, mods, act);
	else
		printf("Key %s A %s Exec exec bhotkeys-bvwm-run %s\n",
		    name, mods, act);
}

/*
 * fvwm modifier letters. N is "none"; the field cannot be empty.
 */
static void
spell_mods(char *mods, int super, int ctrl, int alt, int shift)
{
	int n = 0;

	if (super)
		mods[n++] = '4';
	if (shift)
		mods[n++] = 'S';
	if (ctrl)
		mods[n++] = 'C';
	if (alt)
		mods[n++] = 'M';
	if (n == 0)
		mods[n++] = 'N';
	mods[n] = '\0';
}

static void
emit(KeySym ks, int super, int ctrl, int alt, int shift, const char *act)
{
	const char *name;
	char mods[8], noshift[8];

	if (ks >= XK_A && ks <= XK_Z)
		ks = ks - XK_A + XK_a;
	name = XKeysymToString(ks);
	if (name == NULL)
		return;
	spell_mods(mods, super, ctrl, alt, shift);
	emit_one(name, mods, act);
	if (strlen(name) == 1 && islower((unsigned char)name[0])) {
		char up[2];

		up[0] = (char)toupper((unsigned char)name[0]);
		up[1] = '\0';
		emit_one(up, mods, act);
	}
	/*
	 * Shift+` arrives as grave, quoteleft, or asciitilde depending
	 * on the server. Bind the names bvwm actually sees.
	 */
	if (ks == XK_grave) {
		emit_one("quoteleft", mods, act);
		if (shift) {
			emit_one("asciitilde", mods, act);
			spell_mods(noshift, super, ctrl, alt, 0);
			emit_one("asciitilde", noshift, act);
		}
	}
}

/*
 * NULL means print nothing: a listen 0 plugin with no command only
 * documents a key the window manager owns, so there is nothing to
 * bind or unbind.
 */
static const char *
action_of(const struct bh_plugin *p)
{
	if (!p->listen && p->command[0] == '\0')
		return (NULL);
	if (!p->enabled)
		return (ACT_UNBIND);
	if (p->listen)
		return (ACT_NOP);
	return (p->command);
}

static void
emit_panel(const char *chord, int on, int unbinds)
{
	KeySym ks;
	int super, alt, shift, ctrl;

	if (bh_chord_parse(chord, &ks, &super, &alt, &shift, &ctrl) != 0)
		return;
	if (on == unbinds)
		return;
	emit(ks, super, ctrl, alt, shift, on ? ACT_NOP : ACT_UNBIND);
}

/*
 * One pass over the list. unbinds 1 prints only the "-" lines;
 * unbinds 0 prints the rest.
 */
static void
pass(const struct bh_set *set, int unbinds)
{
	const struct bh_plugin *p;
	const char *act;
	int i;

	for (i = 0; i < set->n; i++) {
		p = set->at[i];
		if (!p->session_ok)
			continue;
		act = action_of(p);
		if (act == NULL)
			continue;
		if ((strcmp(act, ACT_UNBIND) == 0) != unbinds)
			continue;
		emit(p->ks, p->need_super, p->need_ctrl, p->need_alt,
		    p->need_shift, act);
	}
	emit_panel(set->panel_chord, set->panel_on, unbinds);
	emit_panel(set->panel_alt_chord, set->panel_alt_on, unbinds);
}

void
bh_print_fvwm_keys(const struct bh_set *set)
{
	pass(set, 1);
	pass(set, 0);
}
