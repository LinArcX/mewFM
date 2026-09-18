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
□ Modal dialog component (yes/no/ok)
□ Inline text input component
□ Progress feedback for long copy/move operations

## Phase 2 — Multi-selection
☑ Ctrl+click toggles individual rows
☑ Shift+click selects a range
☑ Ctrl+A selects all
□ Selection-aware file operations (act on multiple)
□ Selection preserved across scroll

## Phase 3 — Context menu
□ Right-click on a row opens a Blendish menu
□ Menu items: Open, Open With, Copy, Cut, Rename, Delete, Properties
□ Right-click on empty space: Paste, New Folder, Refresh
□ Submenu for "Open With"

## Phase 4 — Sorting
☑ Click column header to sort
☑ Asc / desc toggle
☑ Indicator arrow in the header
☑ Persist sort column + direction in config

## Phase 5 — Convenience
□ Search / filter box in top bar
□ Live filter of the current directory
□ Ctrl+L to type a path directly
□ Refresh (F5)

## Phase 6 — System integration
□ Trash support (~/.local/share/Trash)
□ Restore from Trash
□ Empty Trash
□ "Open With" chooser dialog
□ Mount / unmount detection for removable devices in sidebar

## Phase 7 — UX polish
□ Status bar (item count, selected count, free space)
□ Row striping or subtle separators
□ Configurable theme (colors from config)
□ Toast / notification for operation results
□ "Properties" dialog (detailed stat + path)

## Phase 8 — Advanced
□ Tabs (multiple directories in one window)
□ User bookmarks (add / remove sidebar entries)
□ Preview panel (thumbnail for images, text preview)
□ List / grid view toggle
□ Drag and drop (internal move, external in / out)
□ Symlink handling (broken-link marker, follow toggle)
□ Permission editor in Properties

## Suggested next step
Phase 1 is the biggest functional gap. Within it, start with:

Modal dialog component — needed by delete and rename.

Inline text input — needed by rename and new folder.

Then wire up operations one at a time: new folder → rename → delete → copy/cut/paste.

That sequence gives you usable, testable features at every step.
