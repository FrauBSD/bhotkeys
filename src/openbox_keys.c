/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * --print-keys openbox, and lxde, whose window manager is openbox.
 * W-C-A-S-k, an openbox keybind.
 */

#include <string.h>

#include "print_priv.h"

int
bh_spell_openbox(char *dst, size_t len, struct bh_key *k)
{
	bh_join_mods(dst, len, k, "W-", "C-", "A-", "S-", "");
	strlcat(dst, k->name, len);
	return (1);
}
