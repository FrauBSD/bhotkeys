#ifndef PANEL_PRIV_H
#define PANEL_PRIV_H

#include <X11/Xft/Xft.h>

#include "bhotkeys.h"

#define HIT_NONE	0
#define HIT_CHECK	1
#define HIT_SET		2
#define HIT_RESET	3
#define HIT_SEARCH	4
#define HIT_MANAGE	5
#define HIT_HEAD_NAME	6
#define HIT_HEAD_CHORD	7
#define HIT_UNLOCK	8
#define HIT_OUT		9
#define HIT_CLOSE	10

int	panel_admin(int argc, char **argv, int *status);

#define BH_PANEL_DESC \
	"Open this list. Stays on until: bhotkeys-panel --disable"
#define BH_PANEL_DESC_GREETER \
	"Open this list. Stays on until: bhotkeys-panel --greeter --disable"
#define BH_PANEL_ALT_DESC \
	"Open this list. The check turns this chord off."
#define BH_PANEL_ALT_DESC_GREETER \
	"Open this list. The check turns this chord off."

#define ROW_PANEL	(-1)
#define ROW_ALT		(-100)
#define ROW_HEAD(i)	((i) <= -2 && (i) != ROW_ALT)

struct panel_ui {
	struct bh_set *set;
	int	readonly;
	int	capture;	/* -2 idle, -1 panel chord, >=0 plugin */
	int	managed;
	int	scroll;		/* pixels into the list */
	int	saved_scroll;	/* position to restore; search does not clobber it */
	int	sort_col;	/* -1 none, 0 name, 1 chord */
	int	sort_rev;
	int	font_px;
	int	win_w;
	int	win_h;
	int	search_x;
	int	search_y;
	int	search_w;
	int	search_h;
	int	header_y;
	int	header_h;
	int	body_y;
	int	body_h;
	int	footer_y;
	int	footer_h;
	int	name_x;
	int	chord_x;
	int	act_x;
	int	reset_x;
	int	content_h;
	int	nlay;
	int	caret_on;
	int	pwmode;
	int	drop_grab;	/* hotkey: let the target take the screen */
	int	close_req;
	int	release_keys;	/* end of chord capture */
	int	pwbad;		/* 1 wrong, 2 password file missing */
	long	pwbad_until;	/* millisecond the note comes down; 0 off */
	int	yield;		/* a chord handed the screen to another client */
	int	drag;
	int	drag_y;
	int	drag_x;
	int	drag_scroll;
	int	drag_moved;
	int	xtouch;		/* a touch owns the current drag */
	int	qpos;		/* byte offset of the search caret */
	int	ngroups;
	char	(*groups)[BH_GROUP_MAX];
	int	groups_cap;
	int	*order;
	int	rows_cap;
	int	shown;
	int	*lay_y;
	int	*lay_h;
	char	query[64];
	char	pw[64];
};

int	panel_text_w(Display *dpy, XftFont *font, const char *s, int n);
int	panel_wrap(Display *dpy, XftFont *font, const char *s, int max_w,
	    char lines[][120], int maxn);
void	panel_refilter(struct panel_ui *ui);
void	panel_place(Display *dpy, int scr, int *x, int *y, int *w, int *h,
	    int *px);
int	panel_group_gap(XftFont *font, int row);
void	panel_layout(Display *dpy, struct panel_ui *ui, XftFont *font,
	    XftFont *small);
void	panel_scroll(struct panel_ui *ui, int dir, int page);
void	panel_scroll_set(struct panel_ui *ui, int y);
void	panel_ui_load(struct panel_ui *ui);
void	panel_ui_save(const struct panel_ui *ui);
void	panel_paint(Display *dpy, Window win, XftDraw *draw, XftFont *font,
	    XftFont *small, XftColor *fg, XftColor *dim, XftColor *accent,
	    struct panel_ui *ui);
int	panel_hit(struct panel_ui *ui, int x, int y, int *row);
void	panel_capture(int on);
void	panel_save(struct panel_ui *ui);
int	panel_on_key(Display *dpy, XKeyEvent *ev, struct panel_ui *ui);
void	panel_keys_hold(Display *dpy, Window win, Time t);
void	panel_below_osk(Display *dpy, Window panel);
void	panel_pw_clear(struct panel_ui *ui);
void	panel_pw_fail(struct panel_ui *ui, int code);
int	panel_pw_click(Display *dpy, Window win, struct panel_ui *ui, int what);
int	panel_pw_dismiss(Display *dpy, struct panel_ui *ui);
int	panel_pw_note(struct panel_ui *ui, long now);
const char *panel_row_label(struct panel_ui *ui, int idx);
void	panel_touch_select(Display *dpy, Window win);
int	panel_gesture(Display *dpy, XEvent *ev, struct panel_ui *ui);
const char *panel_blurb(const struct bh_plugin *p);
int	panel_take_quit(Display *dpy, struct panel_ui *ui);
int	panel_wait(Display *dpy, struct panel_ui *ui, int wait_ms);
int	panel_record_on(void);
int	panel_record_dirty(void);
void	panel_shade_unmap(Display *dpy);

#endif
