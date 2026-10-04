[//]: # ($FrauBSD: bhotkeys/README.md 2026-10-03 18:57:00 -0700 Devin Teske $)

# bhotkeys

Plugin hotkey listener for BSD desktops.

`bhotkeys` watches key presses through the X Record extension and
runs the command of the plugin whose chord matches. It does not grab
the keyboard and it does not eat the event: the window manager and
every other client still see the key. One listener serves one
display; the login greeter runs its own. Every chord comes from a
plugin file, one file per action, dropped into a directory. The
only chords bhotkeys ships are the two that open its own chord list,
`Super+?` and `Super+/`, where a user can turn any plugin off or
record a new chord for it without editing a file.

Home: [FrauBSD/bhotkeys](https://github.com/FrauBSD/bhotkeys)

## Requirements

- X11 with the Record and XTest extensions, Xft + fontconfig
  (`x11`, `xext`, `xtst`, `xft` through `pkg-config`)
- A window manager; the listener recognizes afterstep, awesome,
  bspwm, bvwm, cinnamon, cwm, dwm, enlightenment, fluxbox, fvwm,
  gnome, herbstluftwm, i3, icewm, jwm, kde, lxde, lxqt, mate,
  openbox, pekwm, spectrwm, sway, windowmaker, and xfce by name
- Plugins. None are included; see Packages below

## Build / install

BSD make. `PREFIX` and `DESTDIR` are honored.

```sh
make
make install    # PREFIX=/usr/local by default
make clean
```

Installs `bhotkeys`, `bhotkeys-panel`, `bhotkeys-passwd`, and
`bhotkeys-start` into `bin`; the per-manager apply scripts into
`libexec/bhotkeys`; the shell libraries they share into
`${PREFIX}/bhotkeys`; the man pages; and an empty
`share/bhotkeys/plugins.d` for packages to fill.

## Usage

One line in `~/.xsession` or the window manager rc:

```sh
bhotkeys-start mate
```

That starts [bosd](https://github.com/FrauBSD/bosd) if it is
installed (`--no-bosd` skips it), starts the listener told which
window manager it is under, and runs that manager's apply script
once the manager is up. Without a name the listener detects the
manager itself.

```sh
bhotkeys --daemon              # the session listener, detects the WM
bhotkeys --daemon --wm kde     # no detection, no wait
bhotkeys --panel               # open the chord list from a shell
bhotkeys --print-keys i3       # the chords spelled for i3
bhotkeys-panel --disable       # no chord opens the list
bhotkeys-panel --enable        # both chords open it again
bhotkeys-passwd                # set the greeter unlock password (root)
```

`Super+?` opens the list. A check turns a plugin off. Set keys
records the next chord for that row; Reset puts the plugin's own
chord back. Column headings sort. Typing filters by label and
description. Esc closes. The list is an overlay by default; Window
hands it to the window manager as an ordinary window.

## Plugins

A plugin is a short text file in `share/bhotkeys/plugins.d`
(system), `~/share/bhotkeys/plugins.d` (user), or `$BHOTKEYS_PLUGINS`.
A later directory replaces the same `id`.

```
id lock
group x11/bhotkeys-lock
label Lock screen
chord Super+l
enabled 0 gnome
command xlock-screen
desc Lock the session.
```

`chord`, `enabled`, and `session` lines take window manager names
after the value, so one file speaks to every desktop:

```
chord Super+e default fvwm
chord Super+Shift+e kde
```

A plugin with `listen 0` and no `command` is a row for a chord the
window manager already owns. It appears in the list so the user can
see it, turn it off, or move it, and the apply script carries that
choice to the window manager's own configuration. That is how the
desktop packages below put MATE's or KDE's stock shortcuts in the
same list as everything else.

The full grammar, the resolution rule, and the session file format
are in `bhotkeys(1)`.

## Apply scripts

The listener does not consume keys, so a chord it handles would also
reach the focused window. Each window manager has an apply script in
`libexec/bhotkeys` that reads `bhotkeys --print-keys <wm>` and asks
the manager to grab every listener chord and run `true`. The focused
client never sees it; the manager's own bindings are left alone.
Scripts exist for bvwm, cinnamon, dwm, fluxbox, fvwm, gnome, i3,
kde, lxde, lxqt, mate, openbox, windowmaker, xfce, and xmonad.
`bhotkeys-start` runs the right one; the panel runs it again after
every save.

## Why bhotkeys

Every desktop has a keyboard shortcut dialog, and none of them agree.
GNOME keeps bindings in gsettings, KDE in kglobalaccel, Xfce in
xfconf, MATE in dconf under different schemas, i3 in a config file
that must be reloaded, fvwm in a config file that must be re-read,
and the tiling managers in source. Move a laptop from one to another
and `Super+L` locks the screen in two of them, opens a launcher in a
third, and does nothing in the rest. A tool like xbindkeys or sxhkd
sits beside the desktop with its own file and its own grabs, and the
first time it and the window manager both want `Print`, one of them
loses silently.

bhotkeys does not grab. It records. The key still reaches the window
manager, so nothing the desktop ships is broken by installing it, and
nothing has to be unbound to make room. What it adds is one list
that shows every chord the session has, from whichever package
provided it, with one check box and one Set keys button per row, and
one place to turn a vendor's shortcut off. Because that list is the
same under every window manager, the plugin files are too: a package
ships one descriptor, says which chord it wants on which desktops,
and is done. The apply scripts carry the user's choices back into
each desktop's native store, so the shortcut dialog the desktop
already has keeps telling the truth.

The greeter gets the same treatment. A separate listener runs for
the login screen with its own override file and a password on the
list, so a laptop can take a screenshot or toggle airplane mode
before anyone logs in, and a passer-by cannot rebind it.

### Compared with the usual suspects

- **xbindkeys**, **sxhkd**, **xchainkeys** grab chords. A chord the
  window manager also binds is a race. bhotkeys records and lets the
  apply script ask the manager to do the grabbing, on the manager's
  own terms.
- Desktop shortcut dialogs each know one desktop. bhotkeys shows the
  union, and writes back through the desktop's own store.
- Window manager configs bind to commands in a file the user edits.
  A bhotkeys plugin is dropped in by a package and removed with it;
  the user's overrides live in one session file, not in the config.
- None of them have a greeter story. bhotkeys has a second listener,
  a read-only list, and `bhotkeys-passwd`.

## Packages

bhotkeys installs no actions. These packages each drop plugin files
into `share/bhotkeys/plugins.d`:

- [framework-keyboard](https://github.com/FrauBSD/framework-keyboard):
  Framework Laptop Fn row (volume, brightness, display, airplane,
  audio output, media)
- [bhotkeys-screenshot](https://github.com/FrauBSD/bhotkeys-screenshot):
  `Print`, `Alt+Print`, and the greeter screenshot
- [bhotkeys-lock](https://github.com/FrauBSD/bhotkeys-lock): `Super+L`
- [bhotkeys-extend](https://github.com/FrauBSD/bhotkeys-extend): `Super+E`
- [bhotkeys-lid-state](https://github.com/FrauBSD/bhotkeys-lid-state): `Super+Z`
- [bhotkeys-albert](https://github.com/FrauBSD/bhotkeys-albert): `Super+Space`
- [framework-autorotate-hotkey](https://github.com/FrauBSD/framework-autorotate-hotkey):
  `Super+R` on the Framework Laptop 12
- [bhotkeys-gnome](https://github.com/FrauBSD/bhotkeys-gnome),
  [bhotkeys-kde](https://github.com/FrauBSD/bhotkeys-kde),
  [bhotkeys-mate](https://github.com/FrauBSD/bhotkeys-mate),
  [bhotkeys-xfce](https://github.com/FrauBSD/bhotkeys-xfce),
  [bhotkeys-i3](https://github.com/FrauBSD/bhotkeys-i3): that
  desktop's stock shortcuts as rows, so they can be seen, turned
  off, or moved from the same list
- [bvwm](https://github.com/FrauBSD/bvwm) ships its tiling and focus
  chords as `bvwm-*` plugins

X11 only. The plugin format and the list are display-agnostic by
design; the Record extension is not.
