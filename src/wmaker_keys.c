/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * --print-keys windowmaker. Mod4+Control+Mod1+Shift+k, a Window
 * Maker SHORTCUT.
 */

#include <string.h>

#include "print_priv.h"

int
bh_spell_wmaker(char *dst, size_t len, struct bh_key *k)
{
	bh_join_mods(dst, len, k, "Mod4", "Control", "Mod1", "Shift", "+");
	if (dst[0] != '\0')
		strlcat(dst, "+", len);
	strlcat(dst, k->name, len);
	return (1);
}
