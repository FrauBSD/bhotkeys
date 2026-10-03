# Changelog

Newest first. Each section is a git tag; the bullets are what landed
in that tag (from the previous tag, or from the start of the
repository for 1.0).

## 1.0 (2026-10-03)

- session listener on the X Record extension; no keyboard grab, no
  consumed events, one instance per display held by a lock
- plugin files: id, label, group, chord, enabled, session, listen,
  command, repeat, greeter, desc, desc\_greeter; scanned from the
  system directory, `~/share/bhotkeys/plugins.d`, and
  `BHOTKEYS_PLUGINS`, later directories replacing the same id
- per-window-manager chord, enabled, and session lines; first
  specific line wins, else first fallback; `default` and bare lines
  are fallbacks; no cap on lines or names
- window manager detection by process name for 25 managers;
  `--wm name` to force one; the daemon waits up to 30 s for a known
  manager before loading plugins
- chord list (`bhotkeys-panel`): overlay or decorated window, check
  box per row, Set keys records the next chord, Reset restores the
  plugin's chord, sortable columns, search over label and
  description, saved sort and scroll
- built-in Panel (`Super+?`) and Panel alt (`Super+/`) rows;
  `bhotkeys-panel --disable` turns both off, `--enable` turns both
  on; `bhotkeys --panel` opens the list regardless
- session overrides in `~/.config/bhotkeys/session`, keyed by window
  manager; the listener re-reads plugins when the file changes
- greeter listener (`--greeter`) with its own lock, override file
  under `${PREFIX}/etc/bhotkeys/greeter`, read-only list unlocked by
  `bhotkeys-passwd`, hash in `${PREFIX}/etc/bhotkeys.passwd`
- `--print-keys format` for bvwm, cinnamon, dwm, fluxbox, fvwm,
  gnome, i3, kde, lxde, lxqt, mate, openbox, windowmaker, xfce, and
  xmonad; tab rows for most, fvwm Key lines (Exec, Nop, unbind) for
  fvwm and bvwm
- apply scripts in `libexec/bhotkeys` for those fifteen managers,
  sharing `bhotkeys-seq.subr`, `bhotkeys-plugin.subr`, and
  `bhotkeys-openbox.subr`; each asks the manager to grab the
  listener's chords and run true, and carries list changes into the
  manager's own store; `--wait` polls for the manager first
- `bhotkeys-start [--no-bosd] [manager]`: one line for `.xsession`
  that starts bosd, the listener, and the apply script
- `PREFIX` baked into the binaries, scripts, and man pages at build
- man pages: bhotkeys(1), bhotkeys-panel(1), bhotkeys-passwd(1),
  bhotkeys-start(1)
- no actions shipped; plugins come from separate packages
