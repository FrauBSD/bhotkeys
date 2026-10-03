/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Shared by print_keys.c and the per-window-manager spellers in
 * *_keys.c. A speller writes one chord the way that window
 * manager's configuration writes it, and returns 0 to drop the row.
 */

#ifndef PRINT_PRIV_H
#define PRINT_PRIV_H

#include <stddef.h>

#include <X11/Xlib.h>

#define SPELL_MAX	160

struct bh_key {
	const char *name;	/* keysym name, a letter lowercased */
	KeySym	ks;
	int	super, ctrl, alt, shift;
	int	code;		/* Qt key code, kde only */
};

void	bh_join_mods(char *dst, size_t len, const struct bh_key *k,
	    const char *sup, const char *ctl, const char *alt,
	    const char *shf, const char *sep);
void	bh_qt_text(const struct bh_key *k, char *dst, size_t len);

int	bh_spell_dwm(char *dst, size_t len, struct bh_key *k);
int	bh_spell_fluxbox(char *dst, size_t len, struct bh_key *k);
int	bh_spell_gnome(char *dst, size_t len, struct bh_key *k);
int	bh_spell_i3(char *dst, size_t len, struct bh_key *k);
int	bh_spell_kde(char *dst, size_t len, struct bh_key *k);
int	bh_spell_lxqt(char *dst, size_t len, struct bh_key *k);
int	bh_spell_mate(char *dst, size_t len, struct bh_key *k);
int	bh_spell_openbox(char *dst, size_t len, struct bh_key *k);
int	bh_spell_wmaker(char *dst, size_t len, struct bh_key *k);
int	bh_spell_xfce(char *dst, size_t len, struct bh_key *k);
int	bh_spell_xmonad(char *dst, size_t len, struct bh_key *k);

#endif /* PRINT_PRIV_H */
