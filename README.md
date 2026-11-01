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
- Preview panel for image thumbnails and text files (F3)
- Integrated text editor: right-click a text file → "Edit Here"
- Single-click select, double-click open
- Multi-selection: Ctrl+click, Shift+click, Ctrl+A
- Right-click context menu (row and empty-space)
- Trash support (freedesktop.org spec), restore, empty trash
- Keyboard: arrows, Enter, Backspace, F2, Del, Shift+Del, Ctrl+C/X/V,
  Ctrl+Shift+N, Ctrl+F, Ctrl+L, Ctrl+H, F5, F3
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

`scripts/build.sh` wraps itself with `bear` when available (to produce
`compile_commands.json`), compiles the C and C++ translation units, links
the binary, and installs `/usr/bin/mewFM` along with the desktop entry and
icon.

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

Void Linux:
```sh
sudo xbps-install -Su base-devel glfw-devel MesaLib-devel pkg-config
