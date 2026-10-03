#ifndef BHOTKEYS_H
#define BHOTKEYS_H

#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#define BH_ID_MAX	64
#define BH_LABEL_MAX	96
#define BH_CHORD_MAX	64
#define BH_CMD_MAX	256
#define BH_DESC_MAX	160
#define BH_GROUP_MAX	48
#define BH_CHORD_SLOTS	256	/* hash width; the chain is not a cap */
#define BH_WM_WAIT	30	/* seconds the daemon waits for a manager */
#ifndef PREFIX
#define PREFIX		"/usr/local"
#endif
#define BH_GREETER_FILE	PREFIX "/etc/bhotkeys/greeter"
#define BH_PASSWD_FILE	PREFIX "/etc/bhotkeys.passwd"
#define BH_PANEL_ID		"panel"
#define BH_PANEL_CHORD		"Super+question"
#define BH_PANEL_ALT_ID		"panel-alt"
#define BH_PANEL_ALT_CHORD	"Super+slash"

struct bh_plugin {
	char	id[BH_ID_MAX];
	char	label[BH_LABEL_MAX];
	char	chord[BH_CHORD_MAX];
	char	suggested[BH_CHORD_MAX];
	char	command[BH_CMD_MAX];
	char	desc[BH_DESC_MAX];
	char	desc_greeter[BH_DESC_MAX];
	char	group[BH_GROUP_MAX];
	int	repeat;
	int	greeter_ok;
	int	session_ok;	/* 0: hidden from this user session */
	int	enabled;
	int	listen;		/* 0: show only; the WM owns the key */
	KeySym	ks;
	int	need_super;
	int	need_alt;
	int	need_shift;
	int	need_ctrl;
	struct bh_plugin *next;	/* load order */
	struct bh_plugin *chord_next;	/* plugins on this key */
};

struct bh_set {
	struct bh_plugin *head;
	struct bh_plugin *tail;
	struct bh_plugin **at;	/* at[0 .. n), for row numbers */
	int	n;
	int	at_cap;
	struct bh_plugin *chord[BH_CHORD_SLOTS];
	char	panel_chord[BH_CHORD_MAX];
	char	panel_alt_chord[BH_CHORD_MAX];
	KeySym	panel_ks;
	KeySym	panel_alt_ks;
	int	panel_super;
	int	panel_alt_super;
	int	panel_on;	/* 0: Super+? does not open the list */
	int	panel_alt_on;	/* 0: Super+/ does not open the list */
};

extern int		bh_greeter;
extern Display	       *bh_dpy;

/*
 * The resolution of one field's chord, enabled, or session lines:
 * the value of the first line naming the running manager, and of
 * the first line with no names or the word default. Empty when no
 * such line has been seen. Any number of lines may be noted.
 */
struct bh_wmpick {
	char	specific[BH_CHORD_MAX];
	char	fallback[BH_CHORD_MAX];
};

void	bh_wm_note(struct bh_wmpick *pick, const char *rest);
const char *bh_wm_pick(const struct bh_wmpick *pick);
const char *bh_wm_name(void);
int	bh_wm_force(const char *name);
void	bh_wm_wait(int secs);
void	apply_chord(struct bh_plugin *p);
void	bh_set_clear(struct bh_set *set);
void	bh_plugin_store(struct bh_set *set, const struct bh_plugin *src);
void	bh_rebind(struct bh_set *set);
int	bh_session_ours(const char *line, char *id, int *en, char *chord);
int	bh_session_keep(const char *line);
void	bh_apply_overrides(struct bh_set *set);
int	bh_load(struct bh_set *set);
int	bh_save_session(const struct bh_set *set);
void	bh_config_path(char *path, size_t len);
int	bh_chord_parse(const char *chord, KeySym *ks, int *need_super,
	    int *need_alt, int *need_shift, int *need_ctrl);
int	bh_match(const struct bh_set *set, KeySym ks, int super,
	    unsigned int state, const struct bh_plugin **out);
int	bh_panel_chord(const struct bh_set *set, KeySym ks, int super,
	    int mod4);
int	bh_panel_open(const struct bh_set *set, KeySym ks, int super,
	    int mod4);
void	bh_print_fvwm_keys(const struct bh_set *set);
int	bh_print_keys(const struct bh_set *set, const char *format);
void	bh_print_formats(FILE *fp);
int	bh_exec(const char *command);
void	bh_chord_pretty(const char *chord, char *dst, size_t dstlen);
void	bh_backspace(void);
int	bh_panel_up(void);
int	bh_run_panel(void);
int	bh_dismiss_panel(void);
int	bh_listen(struct bh_set *set);

#endif
