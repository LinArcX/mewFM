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

## Requirements

Void Linux:
```sh
sudo xbps-install -Su base-devel glfw-devel MesaLib-devel pkg-config
