/*
 * Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Which rows the list shows, in which order. No column chosen means
 * alphabetical by the group string, then by name. A heading is that
 * string, as written in the plugin file.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "panel_priv.h"

static int
fold_has(const char *hay, const char *needle)
{
	size_t n, i;

	if (needle == NULL || needle[0] == '\0')
		return (1);
	if (hay == NULL)
		return (0);
	n = strlen(needle);
	for (; *hay != '\0'; hay++) {
		for (i = 0; i < n; i++) {
			unsigned char a = (unsigned char)hay[i];
			unsigned char b = (unsigned char)needle[i];

			if (a == '\0')
				return (0);
			if (tolower(a) != tolower(b))
				break;
		}
		if (i == n)
			return (1);
	}
	return (0);
}

static int
plugin_visible(const struct bh_plugin *p)
{
	if (bh_greeter)
		return (p->greeter_ok);
	return (p->session_ok);
}

const char *
panel_row_label(struct panel_ui *ui, int idx)
{
	if (idx == ROW_ALT)
		return ("Panel alt");
	if (idx < 0)
		return ("Panel");
	return (ui->set->at[idx]->label);
}

static const char *
row_chord(struct panel_ui *ui, int idx)
{
	if (idx == ROW_ALT)
		return (ui->set->panel_alt_chord);
	if (idx < 0)
		return (ui->set->panel_chord);
	return (ui->set->at[idx]->chord);
}

static const char *
row_group(struct panel_ui *ui, int idx)
{
	/* The Panel row has no descriptor. Its heading is its port. */
	if (idx < 0)
		return ("x11/bhotkeys");
	return (ui->set->at[idx]->group);
}

static int
cmp_row(struct panel_ui *ui, int a, int b)
{
	char pa[BH_CHORD_MAX], pb[BH_CHORD_MAX];
	int c;

	if (ui->sort_col == 1) {
		bh_chord_pretty(row_chord(ui, a), pa, sizeof(pa));
		bh_chord_pretty(row_chord(ui, b), pb, sizeof(pb));
		c = strcasecmp(pa, pb);
	} else
		c = strcasecmp(panel_row_label(ui, a), panel_row_label(ui, b));
	if (c == 0)
		c = (a > b) - (a < b);
	if (ui->sort_rev)
		c = -c;
	return (c);
}

static int
cmp_natural(struct panel_ui *ui, int a, int b)
{
	int c;

	c = strcasecmp(row_group(ui, a), row_group(ui, b));
	if (c != 0)
		return (c);
	c = strcasecmp(panel_row_label(ui, a), panel_row_label(ui, b));
	if (c == 0)
		c = (a > b) - (a < b);
	return (c);
}

static int
panel_matches(struct panel_ui *ui)
{
	char pretty[BH_CHORD_MAX];

	if (ui->query[0] == '\0')
		return (1);
	bh_chord_pretty(ui->set->panel_chord, pretty, sizeof(pretty));
	if (fold_has("Panel", ui->query) || fold_has(pretty, ui->query))
		return (1);
	if (fold_has(row_group(ui, ROW_PANEL), ui->query))
		return (1);
	return (fold_has(panel_blurb(NULL), ui->query));
}

static int
panel_alt_matches(struct panel_ui *ui)
{
	char pretty[BH_CHORD_MAX];
	const char *desc;

	if (ui->query[0] == '\0')
		return (1);
	desc = bh_greeter ? BH_PANEL_ALT_DESC_GREETER : BH_PANEL_ALT_DESC;
	bh_chord_pretty(ui->set->panel_alt_chord, pretty, sizeof(pretty));
	if (fold_has("Panel alt", ui->query) ||
	    fold_has(pretty, ui->query) ||
	    fold_has(ui->set->panel_alt_chord, ui->query))
		return (1);
	if (fold_has(row_group(ui, ROW_ALT), ui->query))
		return (1);
	return (fold_has(desc, ui->query));
}

const char *
panel_blurb(const struct bh_plugin *p)
{
	if (p == NULL)
		return (bh_greeter ? BH_PANEL_DESC_GREETER : BH_PANEL_DESC);
	if (bh_greeter && p->desc_greeter[0] != '\0')
		return (p->desc_greeter);
	return (p->desc);
}

static int
rows_fit(struct panel_ui *ui, int need)
{
	int *order, *y, *h;

	if (need <= ui->rows_cap)
		return (0);
	order = realloc(ui->order, (size_t)need * sizeof(*order));
	if (order == NULL)
		return (-1);
	ui->order = order;
	y = realloc(ui->lay_y, (size_t)need * sizeof(*y));
	if (y == NULL)
		return (-1);
	ui->lay_y = y;
	h = realloc(ui->lay_h, (size_t)need * sizeof(*h));
	if (h == NULL)
		return (-1);
	ui->lay_h = h;
	ui->rows_cap = need;
	return (0);
}

static int
group_fit(struct panel_ui *ui, int need)
{
	char (*g)[BH_GROUP_MAX];

	if (need <= ui->groups_cap)
		return (0);
	g = realloc(ui->groups, (size_t)need * sizeof(*g));
	if (g == NULL)
		return (-1);
	ui->groups = g;
	ui->groups_cap = need;
	return (0);
}

void
panel_refilter(struct panel_ui *ui)
{
	int *tmp;
	int i, j, n = 0;

	tmp = malloc(((size_t)ui->set->n + 2) * sizeof(*tmp));
	if (tmp == NULL)
		return;
	for (i = 0; i < ui->set->n; i++) {
		const struct bh_plugin *p = ui->set->at[i];
		char pretty[BH_CHORD_MAX];

		if (!plugin_visible(p))
			continue;
		if (ui->query[0] != '\0') {
			bh_chord_pretty(p->chord, pretty, sizeof(pretty));
			if (!fold_has(p->label, ui->query) &&
			    !fold_has(p->chord, ui->query) &&
			    !fold_has(pretty, ui->query) &&
			    !fold_has(p->desc, ui->query) &&
			    !fold_has(p->desc_greeter, ui->query) &&
			    !fold_has(row_group(ui, i), ui->query))
				continue;
		}
		tmp[n++] = i;
	}
	if (panel_matches(ui))
		tmp[n++] = ROW_PANEL;
	if (panel_alt_matches(ui))
		tmp[n++] = ROW_ALT;
	for (i = 1; i < n; i++) {
		int v = tmp[i];

		j = i;
		while (j > 0 && (ui->sort_col < 0 ?
		    cmp_natural(ui, tmp[j - 1], v) :
		    cmp_row(ui, tmp[j - 1], v)) > 0) {
			tmp[j] = tmp[j - 1];
			j--;
		}
		tmp[j] = v;
	}
	if (rows_fit(ui, n * 2) != 0) {
		free(tmp);
		return;
	}
	ui->ngroups = 0;
	ui->shown = 0;
	for (i = 0; i < n; i++) {
		const char *g = row_group(ui, tmp[i]);

		if (i == 0 || strcasecmp(g, row_group(ui, tmp[i - 1])) != 0) {
			int slot = ui->ngroups;

			if (group_fit(ui, slot + 1) == 0) {
				strlcpy(ui->groups[slot], g, BH_GROUP_MAX);
				ui->order[ui->shown++] = -2 - slot;
				ui->ngroups++;
			}
		}
		ui->order[ui->shown++] = tmp[i];
	}
	ui->nlay = 0;
	free(tmp);
}
