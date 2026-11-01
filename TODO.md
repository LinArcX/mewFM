# TODO

Concrete, verified-open items. Keep this list in sync with `ROADMAP.md`
(the ROADMAP is the user-facing view; this file is the working checklist).

## Features

- [ ] Mount / unmount detection for removable devices in the sidebar
- [ ] List / grid view toggle
- [ ] Drag and drop: internal move (drag a row onto a folder row)
- [ ] Drag and drop: external out (drag a row to another application)
- [ ] Permission editor in the Properties dialog

## Build / tooling

- [ ] `doxygen(generate)` target in `./p` runs `doxygen`, but there is no
      `Doxyfile` and no `docs/` directory in the repository. Either add a
      `Doxyfile` or remove the menu entry.

## Code quality

- [ ] `build.sh` hardcodes the four translation units (`impl.c`,
      `nanovg.c`, `main.cpp`, `FileManager.cpp`). Adding a source file
      requires editing the script by hand. Consider discovering sources
      automatically or documenting the manual step.
- [ ] `assets/fonts/Sofia_Sans/` is present but never referenced by any
      source or build step. Remove it or document why it is kept.