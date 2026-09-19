# rah

A minimal, fast file manager for Linux, written in C++ with GLFW, NanoVG, and oui-blendish.

## Features

- File listing with Name, Size, Type, Owner, and Permissions columns
- Top navigation bar: Home, Back, Forward, Up, and clickable breadcrumb
- Collapsible sidebar with Places, Devices, and Bookmarks sections
- Multiple tabs (Ctrl+T to open, Ctrl+W to close, Ctrl+Tab / Ctrl+Shift+Tab to switch)
- Resizable columns (drag separator in the header row)
- Sortable columns: click Name, Size, Type, Owner, or Permissions to sort; click again to toggle ascending/descending
- Extension-aware icons (images, video, audio, documents, fonts, source code)
- Preview panel for image thumbnails and text files (F3 to toggle)
- Integrated text editor: right-click a text file and choose "Edit Here" (Ctrl+S saves, Esc closes with a save prompt)
- Built-in video player (libmpv): right-click a video file (mp4, mkv, avi, mov, webm, ...) and choose "View" to play it in the main panel; play/pause, stop, seek, and volume controls; Esc or the on-screen X closes it
- Single-click to select, double-click to open
- Multi-selection: Ctrl+click toggles a row, Shift+click selects a range, Ctrl+A selects all
- Right-click context menu: row actions (Open, Edit Here, Extract, Restore when inside Trash, Copy, Cut, Rename, Delete, Properties, Add to Bookmarks, Remove from Bookmarks) and empty-space actions (Paste, New Folder, New File, Empty Trash when inside Trash, Refresh)
- Trash: Del moves entries to `$XDG_DATA_HOME/Trash` (or `~/.local/share/Trash`) as per the freedesktop.org spec; Shift+Del deletes permanently
  - Directories navigate into themselves
  - Executables run directly
  - Other files open with `xdg-open`
- Keyboard navigation: Up/Down arrows, Enter, Backspace (go up), F2 (rename), Del (move to trash), Shift+Del (permanent delete), Ctrl+C/Ctrl+X/Ctrl+V (copy/cut/paste), Ctrl+Shift+N (new folder), Ctrl+F (filter), Ctrl+L (go to path), Ctrl+H (toggle hidden files), F5 (refresh), F3 (toggle preview), Ctrl+T (new tab), Ctrl+W (close tab), Ctrl+Tab / Ctrl+Shift+Tab (switch tabs), Escape (quit)
- Internal clipboard: copy or cut one or more entries, then paste them into the current directory
- Scroll wheel support
- Live name filter via Ctrl+F: shows only entries whose name contains the typed substring; Escape or Cancel clears it
- Status bar: item count, selected count, free space on the current filesystem, and a real-time progress bar with pause / resume for long copy / move operations
- Session persistence via `~/.config/rah/config`:
  - Column widths
  - Last visited path
  - Hidden-file visibility
  - Collapsed state of sidebar sections
  - Sort field and direction
  - Font size
  - Theme colors
- Command-line argument: `rah <path>` opens that directory (or the parent, if a file is given)
- Font and icons embedded into the binary — no runtime asset files needed
- Built-in music player (libmpv): when the current directory contains audio files, a panel appears above the status bar with prev / play-pause / stop / next, a draggable seek slider, a volume slider, and animated level bars. Double-clicking an audio file starts internal playback; the panel persists while something is playing even after navigating away

## Requirements

Void Linux:

```sh
sudo xbps-install -Su base-devel glfw-devel MesaLib-devel pkg-config libmpv-devel
```

Other distributions: install the equivalent of GLFW 3, OpenGL development headers, and pkg-config.

Runtime dependencies: OpenGL 2.0 (works on Intel HD 3000 and older), GLFW's X11 backend, and libmpv (for the built-in music player).

Optional: `unzip` (install with `sudo xbps-install -Su unzip`) enables extraction of `.zip` archives. `tar` handles tar-based archives and is part of the base system.

## Build

```sh
./scripts/build.sh              # debug build (default)
./scripts/build.sh --debug
./scripts/build.sh --release
./scripts/build.sh --clean                  # remove build/debug and build/release
./scripts/build.sh --clean --debug          # remove only build/debug
./scripts/build.sh --clean --release        # remove only build/release
```

The binary is written to `build/debug/rah` or `build/release/rah`.

On a successful build, the script also installs:

- `build/<mode>/rah` → `/usr/bin/rah`

- `assets/icon.svg` → `/usr/share/icons/hicolor/scalable/apps/rah.svg`

- `assets/rah.desktop` → `/usr/share/applications/rah.desktop`

`sudo` is used only if the plain install fails.

## Run
```sh
rah                     # opens the last visited path, or $PWD on first run
rah ~/Documents         # opens ~/Documents
rah /path/to/file.txt   # opens the parent directory
```

## Configuration
Located at `~/.config/rah/config` (or `$XDG_CONFIG_HOME/rah/config`). Lines:

```text
col0=260
col1=90
col2=110
col3=100
col4=110
theme.bg=0.150,0.150,0.150
theme.text=0.900,0.900,0.900
theme.rowSelected=0.200,0.350,0.550
theme.rowHover=0.250,0.250,0.250
theme.rowStripe=1.000,1.000,1.000
path=/home/user
hidden=0
fontSize=14
sortField=0
sortDir=0
collapsed_places=0
collapsed_devices=0
collapsed_bookmarks=0
bookmark=/home/user/Documents
```

Key	Meaning
col0 .. col4	Column widths in pixels for Name, Size, Type, Owner, Permissions
path	Last visited directory
hidden	1 to show hidden files, 0 to hide
fontSize	Base font size (8–48). Row height follows automatically
sortField	Numeric sort column (0=Name, 1=Size, 2=Type, 3=Owner, 4=Permissions)
sortDir	0 for ascending, 1 for descending
collapsed_<key>	Collapse state for sidebar sections (places, devices, bookmarks)
bookmark	Absolute path of a sidebar bookmark; one line per bookmark (written on add)
theme.bg	Background color (R,G,B floats 0.0-1.0)
theme.text	Text color (R,G,B floats)
theme.rowSelected	Selected-row color (R,G,B floats)
theme.rowHover	Hovered-row color (R,G,B floats)
theme.rowStripe	Row stripe color (R,G,B floats; alpha fixed at 0.035)
subFont	Subtitle font family used by the video player (libmpv `sub-font`)
subFontSize	Subtitle font size (pixels) used by the video player (libmpv `sub-font-size`)

Values for `path`, `hidden`, and `collapsed_*` are written automatically. Column widths are written on drag release. fontSize is user-authored — `rah` does not overwrite it.

## Project Layout
```text
.
├── assets/
│   ├── fonts/Hermit/HurmitNerdFont-Regular.otf   # embedded font
│   ├── icon.svg
│   └── rah.desktop
├── scripts/
│   └── build.sh
├── src/
│   ├── FileManager.cpp
│   ├── FileManager.hpp
│   ├── Modal.hpp
│   ├── MusicPlayer.cpp
│   ├── MusicPlayer.hpp
│   ├── TextInput.hpp
│   └── main.cpp
└── third_party/
    ├── nanovg/                                   # rendering
    └── oui-blendish/                             # widget library
```

`build/generated/` (created by the build script) holds the xxd-converted font and icon-sheet headers. It's regenerated on every build and removed by `--clean`.

## Implementation Notes
- No exceptions. Errors are handled via return values and system error codes.

- No templates, no operator overloading, no RTTI.

- No macros except the mandatory NANOVG_GL2 and UI_INLINE shims required by the third-party libraries.

- NanoVG GL2 backend is used for maximum portability on older hardware.

- Assets are embedded via xxd -i into C headers so the binary is self-contained.

- File system access uses std::filesystem for iteration and POSIX stat / getpwuid for owner and permission metadata.

## License
Not yet chosen.
