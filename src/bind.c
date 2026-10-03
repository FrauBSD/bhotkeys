/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Plugins are a list. A key hashes to the chain of plugins on that
 * key, in load order. The chain is the match. Nothing is dropped
 * because a count was reached.
 */

#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "bhotkeys.h"

void
bh_set_clear(struct bh_set *set)
{
	struct bh_plugin *p, *n;

	for (p = set->head; p != NULL; p = n) {
		n = p->next;
		free(p);
	}
	free(set->at);
	memset(set, 0, sizeof(*set));
}

static void
plugin_replace(struct bh_plugin *dst, const struct bh_plugin *src)
{
	struct bh_plugin *next = dst->next;

	*dst = *src;
	dst->next = next;
	dst->chord_next = NULL;
}

static int
at_add(struct bh_set *set, struct bh_plugin *np)
{
	struct bh_plugin **na;
	int cap;

	if (set->n < set->at_cap) {
		set->at[set->n++] = np;
		return (0);
	}
	cap = set->at_cap == 0 ? 16 : set->at_cap * 2;
	na = realloc(set->at, (size_t)cap * sizeof(*na));
	if (na == NULL)
		return (-1);
	set->at = na;
	set->at_cap = cap;
	set->at[set->n++] = np;
	return (0);
}

void
bh_plugin_store(struct bh_set *set, const struct bh_plugin *src)
{
	struct bh_plugin *np;
	int i;

	for (i = 0; i < set->n; i++) {
		if (strcmp(set->at[i]->id, src->id) != 0)
			continue;
		plugin_replace(set->at[i], src);
		return;
	}
	np = malloc(sizeof(*np));
	if (np == NULL)
		return;
	*np = *src;
	np->next = NULL;
	np->chord_next = NULL;
	if (at_add(set, np) != 0) {
		free(np);
		return;
	}
	if (set->tail != NULL)
		set->tail->next = np;
	else
		set->head = np;
	set->tail = np;
}

static KeySym
fold_key(KeySym ks)
{
	if (ks >= XK_A && ks <= XK_Z)
		return (ks - XK_A + XK_a);
	return (ks);
}

static unsigned
chord_slot(KeySym ks)
{
	unsigned h = (unsigned)fold_key(ks);

	h *= 16777619u;
	return (h & (BH_CHORD_SLOTS - 1));
}

void
bh_rebind(struct bh_set *set)
{
	struct bh_plugin *p, *t;
	unsigned slot;

	memset(set->chord, 0, sizeof(set->chord));
	for (p = set->head; p != NULL; p = p->next) {
		p->chord_next = NULL;
		if (p->ks == NoSymbol)
			continue;
		slot = chord_slot(p->ks);
		t = set->chord[slot];
		if (t == NULL) {
			set->chord[slot] = p;
			continue;
		}
		while (t->chord_next != NULL)
			t = t->chord_next;
		t->chord_next = p;
	}
}

static int
same_key(KeySym a, KeySym b)
{
	return (fold_key(a) == fold_key(b));
}

int
bh_panel_chord(const struct bh_set *set, KeySym ks, int super, int mod4)
{
	int sup = super || mod4;

	if (set->panel_ks == NoSymbol)
		return (0);
	if (set->panel_super && !sup)
		return (0);
	return (same_key(ks, set->panel_ks));
}

int
bh_panel_open(const struct bh_set *set, KeySym ks, int super, int mod4)
{
	if (set->panel_on && bh_panel_chord(set, ks, super, mod4))
		return (1);
	if (!set->panel_alt_on || set->panel_alt_ks == NoSymbol)
		return (0);
	if (set->panel_alt_super && !(super || mod4))
		return (0);
	return (same_key(ks, set->panel_alt_ks));
}

int
bh_match(const struct bh_set *set, KeySym ks, int super, unsigned int state,
    const struct bh_plugin **out)
{
	const struct bh_plugin *p;
	int sup = super || (state & Mod4Mask);
	int alt = (state & Mod1Mask) != 0;
	int shift = (state & ShiftMask) != 0;
	int ctrl = (state & ControlMask) != 0;

	*out = NULL;
	for (p = set->chord[chord_slot(ks)]; p != NULL; p = p->chord_next) {
		if (!p->enabled || !p->listen || p->ks == NoSymbol)
			continue;
		if (bh_greeter && !p->greeter_ok)
			continue;
		if (!bh_greeter && !p->session_ok)
			continue;
		if (p->need_super && !sup)
			continue;
		if (p->need_alt != alt)
			continue;
		if (p->need_ctrl != ctrl)
			continue;
		if (p->need_shift && !shift)
			continue;
		if (!p->need_shift && shift &&
		    !((ks >= XK_a && ks <= XK_z) || (ks >= XK_A && ks <= XK_Z)))
			continue;
		if (!same_key(ks, p->ks))
			continue;
		*out = p;
		return (1);
	}
	return (0);
}
