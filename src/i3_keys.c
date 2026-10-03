/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * --print-keys i3. Mod4+Ctrl+Mod1+Shift+k, an i3 bindsym.
 */

#include <string.h>

#include "print_priv.h"

int
bh_spell_i3(char *dst, size_t len, struct bh_key *k)
{
	bh_join_mods(dst, len, k, "Mod4", "Ctrl", "Mod1", "Shift", "+");
	if (dst[0] != '\0')
		strlcat(dst, "+", len);
	strlcat(dst, k->name, len);
	return (1);
}
