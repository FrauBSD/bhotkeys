/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * --print-keys xfce. <Super><Primary><Alt><Shift>k, as xfconf
 * stores it.
 */

#include <string.h>

#include "print_priv.h"

int
bh_spell_xfce(char *dst, size_t len, struct bh_key *k)
{
	bh_join_mods(dst, len, k, "<Super>", "<Primary>", "<Alt>", "<Shift>",
	    "");
	strlcat(dst, k->name, len);
	return (1);
}
