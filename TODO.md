# TODO

Concrete, verified-open items. Keep this list in sync with `ROADMAP.md`
(the ROADMAP is the user-facing view; this file is the working checklist).

## Features

- [ ] List / grid view toggle
- [ ] Permission editor in the Properties dialog

- [ ] Ability to view all copy/move history in mewFM from beginning till now.
- [ ] Copy/Move shold be atomic operation.
- [ ] When we copy/move files/dirs, we should be able to:
  - to see the src/dst of copy/move operation.
  - stop it(it should be atomic copy/move)

## Build / tooling


## Code quality

- [ ] `build.sh` hardcodes the four translation units (`impl.c`,
      `nanovg.c`, `main.cpp`, `FileManager.cpp`). Adding a source file
      requires editing the script by hand. Consider discovering sources
      automatically or documenting the manual step.
- [ ] `assets/fonts/Sofia_Sans/` is present but never referenced by any
      source or build step. Remove it or document why it is kept.
