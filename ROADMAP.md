# Roadmap
## Done
☑ GLFW + NanoVG + oui-blendish baseline
☑ File listing (name, size, type, owner, permissions)
☑ Top bar: Home / Back / Forward / Up + breadcrumb
☑ Collapsible sidebar (Places, Devices)
☑ Column resizing with persistence
☑ Extension-aware icons
☑ Single-click select, double-click open
☑ Keyboard: arrows, Enter, Backspace, Ctrl+H, Escape
☑ Scroll, hover, truncation with ...
☑ Config file (columns, path, hidden, collapse, font size)
☑ CLI path argument
☑ Embedded font and icons (xxd)
☑ Build script with --debug, --release, --clean
☑ Auto-install to /usr/bin, icon, .desktop
☑ Resize cursor on column separators
☑ Separator lines between panels

## Phase 1 — Core file operations
☑ Adding sort mechanism to columns in mainView.
☑ Internal clipboard state (copy / cut)
☑ Paste into current directory
☑ Delete (with confirmation dialog)
☑ Rename (inline text field or modal)
☑ New folder (Ctrl+Shift+N)
☑ Keyboard shortcuts: Ctrl+C, Ctrl+X, Ctrl+V, F2, Del, Ctrl+Shift+N
☑ Modal dialog component (yes/no/ok)
☑ Inline text input component
☑ Progress feedback for long copy/move operations (with pause / resume)

## Phase 2 — Multi-selection
☑ Ctrl+click toggles individual rows
☑ Shift+click selects a range
☑ Ctrl+A selects all
☑ Selection-aware file operations (act on multiple)
☑ Selection preserved across scroll

## Phase 3 — Context menu
☑ Right-click on a row opens a Blendish menu
☑ Menu items: Open, Edit Here, Extract, Copy, Cut, Rename, Delete, Properties, Add to Bookmarks, Remove from Bookmarks
□ "Open With" menu item
☑ Right-click on empty space: Paste, New Folder, Refresh
□ Submenu for "Open With"

## Phase 4 — Sorting
☑ Click column header to sort
☑ Asc / desc toggle
☑ Indicator arrow in the header
☑ Persist sort column + direction in config

## Phase 5 — Convenience
☑ Search / filter box in top bar
☑ Live filter of the current directory
☑ Ctrl+L to type a path directly
☑ Refresh (F5)

## Phase 6 — System integration
☑ Trash support (~/.local/share/Trash)
☑ Restore from Trash
☑ Empty Trash
☑ Extract tar and zip archives
□ "Open With" chooser dialog
□ Mount / unmount detection for removable devices in sidebar

## Phase 7 — UX polish
☑ Status bar (item count, selected count, free space)
☑ Row striping or subtle separators
☑ Configurable theme (colors from config)
☑ Toast / notification for operation results
☑ "Properties" dialog (detailed stat + path)

## Phase 8 — Advanced
☑ Tabs (multiple directories in one window)
☑ User bookmarks (add / remove sidebar entries)
☑ Preview panel (thumbnail for images, text preview)
☑ Integrated text editor (right-click, Edit Here)
☑ Built-in music player (libmpv): panel above status bar, play/pause/stop/prev/next, seek slider, volume slider, animated level bars
☑ Built-in video player (libmpv): right-click a video file and choose "View" to play it in the main panel (play/pause, stop, seek, volume, Esc closes)
□ List / grid view toggle
□ Drag and drop (internal move, external in / out)
□ Symlink handling (broken-link marker, follow toggle)
□ Permission editor in Properties

## Suggested next step
Several roadmap items are already implemented in the code but were still marked as pending. The remaining work is:

Open With menu item, submenu, and chooser dialog (Phase 3).

Mount / unmount detection for removable devices (Phase 6).

List / grid view toggle (Phase 8).

Drag and drop (Phase 8).

Symlink handling (Phase 8).

Permission editor in Properties (Phase 8).
