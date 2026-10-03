/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * --print-keys gnome, and cinnamon, which stores the same text.
 * <Super><Control><Alt><Shift>k, as gsettings writes it.
 */

#include <string.h>

#include "print_priv.h"

int
bh_spell_gnome(char *dst, size_t len, struct bh_key *k)
{
	bh_join_mods(dst, len, k, "<Super>", "<Control>", "<Alt>", "<Shift>",
	    "");
	strlcat(dst, k->name, len);
	return (1);
}
