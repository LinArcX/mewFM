# Roadmap

## Done
☑ GLFW + NanoVG + oui-blendish baseline
☑ File listing (name, size, type, owner, permissions)
☑ Top bar: Home / Back / Forward / Up + breadcrumb
☑ Collapsible sidebar (Places, Bookmarks)
☑ Hideable sidebar (toolbar button, Ctrl+B, F9) with animated slide
☑ Column resizing with persistence
☑ Sortable columns with sort indicators and persistence
☑ Extension-aware icons
☑ Single-click select, double-click open
☑ Multi-selection (Ctrl+click, Shift+click, Ctrl+A)
☑ Internal clipboard (copy / cut) and paste
☑ Delete, Rename, New Folder, New File
☑ Context menu (row + empty space)
☑ Filter box (Ctrl+F), Go-to-path (Ctrl+L), Refresh (F5)
☑ Hidden-files toggle (Ctrl+H), persisted
☑ Tabs (new / close / next / prev), per-tab state
☑ User bookmarks (add / remove, persisted)
☑ Collapsible sidebar sections (collapsed state persisted)
☑ Mount / unmount detection for removable devices in the sidebar
☑ Trash support: move to trash, restore, empty trash (freedesktop.org spec)
☑ Extract tar / zip archives
☑ Properties dialog
☑ Preview panel (image thumbnail / text excerpt, F3, animated slide)
☑ Integrated text editor (right-click → "Edit Here")
☑ "Open With" chooser dialog with search, MIME-type filtering, default app highlighting, and right-click to set default
☑ Symlink handling (broken-link marker, link-preserving copy/move, follow toggle in the empty-space context menu)
☑ Drag and drop: external in (drop files from the OS into the file list or a folder row to copy them in)
☑ Chunked copy / move state machine with progress bar and pause / resume
☑ Status bar: item count, selection count, free space, progress bar
☑ Toast notifications for short status messages
☑ Session persistence via `~/.config/mewFM/config`
☑ Command-line argument: `mewFM <path>`
☑ Embedded font and icons

## TODO
□ List / grid view toggle
□ Drag and drop: internal move (drag a row onto a folder row)
□ Drag and drop: external out (drag a row to another application)
□ Permission editor in Properties
