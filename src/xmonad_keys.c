/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * --print-keys xmonad. (mod4Mask .|. shiftMask, xK_k), an xmonad
 * key. No modifier is 0.
 */

#include <stdio.h>
#include <string.h>

#include "print_priv.h"

int
bh_spell_xmonad(char *dst, size_t len, struct bh_key *k)
{
	char mask[SPELL_MAX];

	bh_join_mods(mask, sizeof(mask), k, "mod4Mask", "controlMask",
	    "mod1Mask", "shiftMask", " .|. ");
	if (mask[0] == '\0')
		strlcpy(mask, "0", sizeof(mask));
	snprintf(dst, len, "(%s, xK_%s)", mask, k->name);
	return (1);
}
