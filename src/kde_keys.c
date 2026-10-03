/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * --print-keys kde. Meta+Ctrl+Alt+Shift+K as Qt writes it, with the
 * Qt key code in k->code for the extra column. A key Qt cannot name
 * drops the row. bh_qt_text is shared with lxqt.
 */

#include <string.h>

#include <X11/keysym.h>

#include "print_priv.h"

static int
qt_code(KeySym ks)
{
	if (ks >= XK_a && ks <= XK_z)
		return ((int)(ks - XK_a + XK_A));
	if (ks == XK_Print)
		return (0x01000009);
	if (ks == XK_space)
		return (0x20);
	if (ks == XK_slash)
		return (0x2f);
	if (ks == XK_grave)
		return (0x60);
	if (ks > 0 && ks < 0x100)
		return ((int)ks);
	return (0);
}

/* Qt text: a letter is upper, slash is /, space is Space, grave is ` */
void
bh_qt_text(const struct bh_key *k, char *dst, size_t len)
{
	if (strcmp(k->name, "slash") == 0)
		strlcpy(dst, "/", len);
	else if (strcmp(k->name, "space") == 0)
		strlcpy(dst, "Space", len);
	else if (strcmp(k->name, "grave") == 0)
		strlcpy(dst, "`", len);
	else if (strlen(k->name) == 1 && k->name[0] >= 'a' &&
	    k->name[0] <= 'z') {
		dst[0] = k->name[0] - 'a' + 'A';
		dst[1] = '\0';
	} else
		strlcpy(dst, k->name, len);
}

int
bh_spell_kde(char *dst, size_t len, struct bh_key *k)
{
	char key[64];
	int c;

	c = qt_code(k->ks);
	if (c == 0)
		return (0);
	if (k->super)
		c |= 0x10000000;
	if (k->ctrl)
		c |= 0x04000000;
	if (k->alt)
		c |= 0x08000000;
	if (k->shift)
		c |= 0x02000000;
	k->code = c;
	bh_join_mods(dst, len, k, "Meta+", "Ctrl+", "Alt+", "Shift+", "");
	bh_qt_text(k, key, sizeof(key));
	strlcat(dst, key, len);
	return (1);
}
