# mewFM

A minimal, fast file manager for Linux, written in C++ with GLFW, NanoVG, and oui-blendish.

## Features

- File listing with Name, Size, Type, Owner, and Permissions columns
- Top navigation bar: Home, Back, Forward, Up, and clickable breadcrumb
- Collapsible sidebar with Places and Bookmarks sections
- Hideable sidebar (toolbar button, Ctrl+B, F9), animated slide
- Multiple tabs (Ctrl+T, Ctrl+W, Ctrl+Tab / Ctrl+Shift+Tab)
- Resizable columns (drag separator in the header row)
- Sortable columns (click to sort, click again to toggle asc/desc)
- Extension-aware icons (images, video, audio, documents, fonts, code)
- Broken symlinks shown with a "Broken link" type (never hidden)
- Copy / move preserve symlinks as links (never followed)
- Preview panel for image thumbnails and text files (F3)
- Integrated text editor: right-click a text file → "Edit Here"
- Single-click select, double-click open
- Multi-selection: Ctrl+click, Shift+click, Ctrl+A
- Right-click context menu (row and empty-space)
- Trash support (freedesktop.org spec), restore, empty trash
- Keyboard: arrows, Enter, Backspace, F2, Del, Shift+Del, Ctrl+C/X/V,
  Ctrl+N, Ctrl+Shift+N, Ctrl+F, Ctrl+L, Ctrl+H, F5, F3
- Internal clipboard: copy or cut entries, paste into current directory
- Live name filter via Ctrl+F
- Status bar: item count, selection count, free space, progress bar with
  pause/resume for long copy/move operations
- Session persistence via `~/.config/mewFM/config`
- Command-line argument: `mewFM <path>`
- Embedded font and icons
- Extract tar / zip archives (right-click → "Extract")
- Properties dialog (right-click → "Properties")
- Toast notifications for short status messages

## Build

```sh
./scripts/build.sh --debug
```

`scripts/build.sh` compiles only the translation units that changed,
compiles them in parallel, links only when an object file is newer than
the binary, and installs `/usr/bin/mewFM` along with the desktop entry and
icon only when the binary or assets changed. Debug builds use `-O0 -g` for
fast iteration and proper debug symbols.

To regenerate `compile_commands.json` (for the LSP), run:

```sh
MEWFM_BEAR=1 ./scripts/build.sh --debug
```

Bear is skipped on normal builds to keep iteration fast. It runs
automatically the first time (when `compile_commands.json` is missing).

## Run

```sh
mewFM              # open the current directory
mewFM <path>       # open <path>
```

## Configuration

Session state is persisted to `$XDG_CONFIG_HOME/mewFM/config` (falling back
to `$HOME/.config/mewFM/config` when `XDG_CONFIG_HOME` is unset). The file
stores column widths, theme colors, sort field and direction, the
hidden-files toggle, sidebar and preview visibility, the last visited path,
collapsed sidebar sections, and user bookmarks.

## Requirements

Build:
```sh
sudo xbps-install -Su base-devel glfw-devel MesaLib-devel pkg-config
```

`xxd` is required to embed the font and icon assets. It is typically
included with vim; install `vim` (or an equivalent `xxd` package) if
`xxd` is not already available on your system.

Runtime (for archive extraction and "open with default app"):
```sh
sudo xbps-install -Su unzip tar xdg-utils
```

Optional (only for regenerating `compile_commands.json` for the LSP):
```sh
sudo xbps-install -Su bear
```

## Patch workflow

Changes are applied from JSON patch files under `patches/`. The
interactive menu (`./p`) provides a **create patch** entry that opens a
new patch file for editing, then validates and applies it with
`scripts/extractFiles.py`. Patches are removed after a successful
application.
