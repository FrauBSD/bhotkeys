/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * --print-keys format. One row per plugin shown in this session,
 * tab separated: id, listen, enabled, chord, label. The chord is
 * spelled the way that window manager's own configuration writes
 * it, by the speller in that window manager's *_keys.c. kde puts
 * the Qt key code before the label. fvwm and bvwm print Key lines
 * instead (fvwm_keys.c).
 *
 * A listen 1 chord on an XF86 key is left out: it inserts no
 * character, so there is nothing for the window manager to
 * swallow. Panel and Panel alt are listen 1 rows. No column is
 * ever empty, so a tab-separated read gets every field.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "bhotkeys.h"
#include "print_priv.h"

struct fmt {
	const char *name;
	int	(*spell)(char *dst, size_t len, struct bh_key *k);
	int	code;		/* print k->code before the label */
};

struct seen {
	char	text[SPELL_MAX];
	struct seen *next;
};

static const struct fmt fmts[] = {
	{ "bvwm", NULL, 0 },
	{ "cinnamon", bh_spell_gnome, 0 },
	{ "dwm", bh_spell_dwm, 0 },
	{ "fluxbox", bh_spell_fluxbox, 0 },
	{ "fvwm", NULL, 0 },
	{ "gnome", bh_spell_gnome, 0 },
	{ "i3", bh_spell_i3, 0 },
	{ "kde", bh_spell_kde, 1 },
	{ "lxde", bh_spell_openbox, 0 },
	{ "lxqt", bh_spell_lxqt, 0 },
	{ "mate", bh_spell_mate, 0 },
	{ "openbox", bh_spell_openbox, 0 },
	{ "windowmaker", bh_spell_wmaker, 0 },
	{ "xfce", bh_spell_xfce, 0 },
	{ "xmonad", bh_spell_xmonad, 0 },
	{ NULL, NULL, 0 }
};

/*
 * The modifiers the chord needs, in Super, Control, Alt, Shift
 * order, each spelled as the caller says and joined by sep.
 */
void
bh_join_mods(char *dst, size_t len, const struct bh_key *k, const char *sup,
    const char *ctl, const char *alt, const char *shf, const char *sep)
{
	const char *m[4];
	int n = 0, i;

	if (k->super)
		m[n++] = sup;
	if (k->ctrl)
		m[n++] = ctl;
	if (k->alt)
		m[n++] = alt;
	if (k->shift)
		m[n++] = shf;
	dst[0] = '\0';
	for (i = 0; i < n; i++) {
		if (i > 0)
			strlcat(dst, sep, len);
		strlcat(dst, m[i], len);
	}
}

static int
seen_has(const struct seen *head, const char *text)
{
	for (; head != NULL; head = head->next) {
		if (strcmp(head->text, text) == 0)
			return (1);
	}
	return (0);
}

static void
seen_add(struct seen **head, const char *text)
{
	struct seen *n;

	n = malloc(sizeof(*n));
	if (n == NULL)
		return;
	strlcpy(n->text, text, sizeof(n->text));
	n->next = *head;
	*head = n;
}

static void
seen_free(struct seen *head)
{
	struct seen *n;

	while (head != NULL) {
		n = head->next;
		free(head);
		head = n;
	}
}

static void
row(const struct fmt *f, struct seen **seen, const char *id, int listen,
    int enabled, KeySym ks, int super, int ctrl, int alt, int shift,
    const char *label)
{
	struct bh_key k;
	char text[SPELL_MAX];
	const char *name;

	if (ks == NoSymbol)
		return;
	if (ks >= XK_A && ks <= XK_Z)
		ks = ks - XK_A + XK_a;
	name = XKeysymToString(ks);
	if (name == NULL)
		return;
	if (listen && strncmp(name, "XF86", 4) == 0)
		return;
	memset(&k, 0, sizeof(k));
	k.name = name;
	k.ks = ks;
	k.super = super;
	k.ctrl = ctrl;
	k.alt = alt;
	k.shift = shift;
	if (!f->spell(text, sizeof(text), &k))
		return;
	if (listen && enabled) {
		if (seen_has(*seen, text))
			return;
		seen_add(seen, text);
	}
	printf("%s\t%d\t%d\t%s\t", id, listen ? 1 : 0, enabled ? 1 : 0,
	    text);
	if (f->code)
		printf("%d\t", k.code);
	printf("%s\n", label[0] != '\0' ? label : id);
}

void
bh_print_formats(FILE *fp)
{
	const char *indent = "        ";
	size_t col, len;
	int i;

	col = fprintf(fp, "Formats:");
	for (i = 0; fmts[i].name != NULL; i++) {
		len = strlen(fmts[i].name);
		if (col + 1 + len > 80) {
			fprintf(fp, "\n%s", indent);
			col = strlen(indent);
		}
		fprintf(fp, " %s", fmts[i].name);
		col += 1 + len;
	}
	fprintf(fp, "\n");
}

int
bh_print_keys(const struct bh_set *set, const char *format)
{
	const struct fmt *f = NULL;
	struct seen *seen = NULL;
	KeySym ks;
	int i, super, ctrl, alt, shift;

	for (i = 0; fmts[i].name != NULL; i++) {
		if (strcasecmp(fmts[i].name, format) == 0) {
			f = &fmts[i];
			break;
		}
	}
	if (f == NULL)
		return (-1);
	if (f->spell == NULL) {
		bh_print_fvwm_keys(set);
		return (0);
	}
	for (i = 0; i < set->n; i++) {
		const struct bh_plugin *p = set->at[i];

		if (!p->session_ok)
			continue;
		row(f, &seen, p->id, p->listen, p->enabled, p->ks,
		    p->need_super, p->need_ctrl, p->need_alt, p->need_shift,
		    p->label);
	}
	if (bh_chord_parse(set->panel_chord, &ks, &super, &alt, &shift,
	    &ctrl) == 0)
		row(f, &seen, BH_PANEL_ID, 1, set->panel_on, ks, super, ctrl,
		    alt, shift, "Panel");
	if (bh_chord_parse(set->panel_alt_chord, &ks, &super, &alt, &shift,
	    &ctrl) == 0)
		row(f, &seen, BH_PANEL_ALT_ID, 1, set->panel_alt_on, ks,
		    super, ctrl, alt, shift, "Panel alt");
	seen_free(seen);
	return (0);
}
