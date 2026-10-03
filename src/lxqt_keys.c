/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * --print-keys lxqt. Qt text with grave as QuoteLeft and question
 * as ?, then percent-encoded for a globalkeyshortcuts.conf group
 * name.
 */

#include <string.h>

#include "print_priv.h"

int
bh_spell_lxqt(char *dst, size_t len, struct bh_key *k)
{
	char seq[SPELL_MAX], key[64], one[2];
	const char *s;

	bh_join_mods(seq, sizeof(seq), k, "Meta+", "Ctrl+", "Alt+", "Shift+",
	    "");
	if (strcmp(k->name, "grave") == 0)
		strlcpy(key, "QuoteLeft", sizeof(key));
	else if (strcmp(k->name, "question") == 0)
		strlcpy(key, "?", sizeof(key));
	else
		bh_qt_text(k, key, sizeof(key));
	strlcat(seq, key, sizeof(seq));
	dst[0] = '\0';
	one[1] = '\0';
	for (s = seq; *s != '\0'; s++) {
		switch (*s) {
		case '%':
			strlcat(dst, "%25", len);
			break;
		case '+':
			strlcat(dst, "%2B", len);
			break;
		case '/':
			strlcat(dst, "%2F", len);
			break;
		case '?':
			strlcat(dst, "%3F", len);
			break;
		default:
			one[0] = *s;
			strlcat(dst, one, len);
		}
	}
	return (1);
}
