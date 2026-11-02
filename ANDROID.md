# mewFM — Android port

This file documents the state of the Android build of mewFM. The desktop
build is the primary target; the Android port is a lighter port that shares
the same feature goals but is still under active development.

## Build

Requirements:

- Android SDK at `ANDROID_HOME` or `ANDROID_SDK_ROOT`
- `build-tools` with a working `d8` and `apksigner`
- `platforms/android-33/android.jar`
- `kotlinc` in `PATH`
- `rsvg-convert`, `inkscape`, or ImageMagick `convert` for launcher icons
- `zip`, `keytool`

Build the APK:

```sh
./scripts/build.sh --android
```

Output: `build/android/mewFM.apk`.

## Features

Currently implemented:

- Directory listing with Name and Size columns
- Sort by 7 fields (`SortField`), with directory-first ordering
  - Tap the NAME or SIZE header to sort by that column; tap again to
    toggle ascending/descending. The other five fields (Type, Owner,
    Permissions, DateCreated, DateModified) are supported by the backend
    but have no column UI yet.
- Hidden-file toggle (empty-space context menu entry "Show hidden files" /
  "Hide hidden files")
- Symlink-follow flag (`setFollowSymlinks`, not exposed in the UI)
- Live filter (magnifier button at the right end of the top bar; the list
  re-filters on every keystroke and the active filter is shown in the
  status bar)
- Back / forward / up navigation (async)
- Create folder, create file, refresh (via the FAB)
- Sidebar with hardcoded shortcuts (Root, Home, Pictures, Downloads, DCIM)
- Single-tap selection, double-tap to navigate into a folder
- Open files in external apps via `ACTION_VIEW`, served through the
  minimal `FileContentProvider` (`android/src/com/mewfm/FileContentProvider.kt`)
- Toast notifications
- Ripple feedback, fling scrolling
- Multi-selection (long-press a row to enter; tap toggles; back exits)
  with an action bar offering Copy, Cut, Delete, Select All, and More
  (single-selection More exposes Open, Rename, Copy, Cut)
- Long-press on empty list space opens a context menu (Paste, New Folder,
  New File, Refresh)
- Rename, permanent delete, copy, cut, paste
  (paste runs on a background executor and refreshes the listing on completion)

## Known gaps

Everything below is missing compared with the desktop build:

### File operations

- Paste progress UI (paste runs asynchronously but only shows a start/finish toast)
- Trash (no Android equivalent path implemented)
- Archive extraction
- Properties dialog
- Permission editor

### UI

- Tabs
- Preview panel
- Integrated text editor
- Open With chooser
- Sort UI for the Type, Owner, Permissions, DateCreated, and DateModified
  fields (only NAME and SIZE are exposed today)
- Go-to-path input
- Breadcrumb segments (path is clipped text only)
- Bookmarks
- Collapsible sidebar sections
- Device / mount detection
- Selection count and free-space display
- Progress bar with pause / resume for copy / move
- Drag and drop (internal or external)
- Session persistence

### Android integration

- `ACTION_VIEW` / `ACTION_SEND` handling
- Runtime permission rationale and fallback
- Storage-volume enumeration (only `/storage/emulated/0` is assumed)

## Known issues

### Deprecated API

`MainActivity.onBackPressed()` is deprecated. Replace with
`OnBackPressedDispatcher` / `onBackPressedCallback`.

### Owner resolution

`ownerNameForUid()` only special-cases `0` (root) and `1000` (system);
every other uid is shown as a raw number. There is no equivalent of
`getpwuid` on Android.

### Creation time

`createdTime` is set to `mtime`. The `DateCreated` and `DateModified`
sorts are therefore identical. Exposing real ctime would need
`Files.readAttributes` with `BasicFileAttributes` or a `statx` call.

### Case-sensitive filter

`filter` matching is case-sensitive. The desktop build is also
case-sensitive, but most file managers use case-insensitive substring
matching.

## See also

- [README.md](README.md) — desktop build, features, configuration
- [ROADMAP.md](ROADMAP.md) — desktop roadmap
- [TODO.md](TODO.md) — desktop working checklist
