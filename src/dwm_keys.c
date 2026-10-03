/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * --print-keys dwm. Mod4Mask|ControlMask|Mod1Mask|ShiftMask, XK_k,
 * for a dwm keys[] row. No modifier is 0.
 */

#include <string.h>

#include "print_priv.h"

int
bh_spell_dwm(char *dst, size_t len, struct bh_key *k)
{
	bh_join_mods(dst, len, k, "Mod4Mask", "ControlMask", "Mod1Mask",
	    "ShiftMask", "|");
	if (dst[0] == '\0')
		strlcpy(dst, "0", len);
	strlcat(dst, ", XK_", len);
	strlcat(dst, k->name, len);
	return (1);
}
