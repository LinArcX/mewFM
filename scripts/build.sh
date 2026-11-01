#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

SCRIPT_PATH="$(cd "$(dirname "$0")" && pwd)/$(basename "$0")"
# Bear's ptrace tracing is very slow. Only wrap when the LSP manifest
# needs refreshing: on the first build, or when MEWFM_BEAR=1 is set.
if [ -z "$MEWFM_BEAR_WRAPPED" ] && command -v bear >/dev/null 2>&1; then
  if [ "${MEWFM_BEAR:-0}" = "1" ] || [ ! -f compile_commands.json ]; then
    export MEWFM_BEAR_WRAPPED=1
    exec bear -- "$SCRIPT_PATH" "$@"
  fi
fi

if [ -t 1 ]; then
  C_RESET=$'\033[0m'; C_DIM=$'\033[2m'; C_BOLD=$'\033[1m'
  C_CYAN=$'\033[36m'; C_YELLOW=$'\033[33m'; C_GREEN=$'\033[32m'; C_RED=$'\033[31m'
else
  C_RESET=""; C_DIM=""; C_BOLD=""; C_CYAN=""; C_YELLOW=""; C_GREEN=""; C_RED=""
fi

MODE="debug"; MODE_SET=0; CLEAN=0; FILENAME=""
for arg in "$@"; do
  case "$arg" in
    --debug)   MODE="debug";   MODE_SET=1 ;;
    --release) MODE="release"; MODE_SET=1 ;;
    --clean)   CLEAN=1 ;;
    -h|--help) echo "Usage: scripts/build.sh [--debug|--release|--clean] [filename]"; exit 0 ;;
    -*) echo "${C_RED}build.sh: unknown option: $arg${C_RESET}" >&2; exit 1 ;;
    *)  FILENAME="$arg" ;;
  esac
done

if [ "$CLEAN" = "1" ]; then
  echo "${C_BOLD}${C_CYAN}>>> clean mode${C_RESET}"
  if [ "$MODE_SET" = "1" ]; then
    TARGETS=("build/$MODE")
  else
    TARGETS=("build/debug" "build/release")
  fi
  for t in "${TARGETS[@]}"; do
    if [ -d "$t" ]; then
      echo "  ${C_DIM}rm -rf $t${C_RESET}"
      rm -rf "$t"
    fi
  done
  if [ -d build/generated ]; then
    echo "  ${C_DIM}rm -rf build/generated${C_RESET}"
    rm -rf build/generated
  fi
  echo "${C_GREEN}>>> clean done${C_RESET}"
  exit 0
fi

OUT="build/$MODE"; GEN="build/generated"
mkdir -p "$OUT" "$GEN"

case "$MODE" in
  debug)   OPT="-O0 -g" ;;
  release) OPT="-O2"    ;;
esac

CXXFLAGS="-std=c++17 $OPT -Wunused -Wunreachable-code -MMD -MP -Isrc -I$GEN -Ithird_party/nanovg -Ithird_party/oui-blendish -DGL_GLEXT_PROTOTYPES"
CFLAGS="$OPT -MMD -MP -Ithird_party/nanovg -Ithird_party/oui-blendish -DGL_GLEXT_PROTOTYPES"

echo "${C_BOLD}${C_CYAN}>>> mode:${C_RESET} ${C_BOLD}$MODE${C_RESET}"

# Regenerate an embedded header only when its source asset is newer.
embed_if_needed() {
  local out="$1" src="$2" name="$3"
  if [ -f "$out" ] && [ "$out" -nt "$src" ]; then
    return 0
  fi
  echo "${C_CYAN}>>> embedding $src${C_RESET}"
  xxd -i -n "$name" "$src" > "$out"
}
embed_if_needed "$GEN/HurmitFont.hpp"   assets/fonts/Hermit/HurmitNerdFont-Regular.otf fontData
embed_if_needed "$GEN/BlenderIcons.hpp" third_party/oui-blendish/blender_icons16.png   iconData

# A TU is up to date when its .o exists and is not older than any header
# listed in its -MMD dependency file.
tu_is_current() {
  local obj="$1"
  [ -f "$obj" ] || return 1
  local dep="${obj%.o}.d"
  [ -f "$dep" ] || return 1
  local f
  for f in $(tr '\\' ' ' < "$dep" | sed 's/^[^:]*://'); do
    [ -e "$f" ] || continue
    if [ "$f" -nt "$obj" ]; then
      return 1
    fi
  done
  return 0
}

pids=()
labels=()
spawn() {
  local label="$1"; shift
  echo "${C_CYAN}>>> $label${C_RESET}"
  "$@" &
  pids+=("$!")
  labels+=("$label")
}

if ! tu_is_current "$OUT/impl.o"; then
  spawn "compiling impl.c" gcc $CFLAGS -fgnu89-inline -c third_party/impl.c -o "$OUT/impl.o"
fi
if ! tu_is_current "$OUT/nanovg.o"; then
  spawn "compiling nanovg.c" gcc $CFLAGS -c third_party/nanovg/nanovg.c -o "$OUT/nanovg.o"
fi
if ! tu_is_current "$OUT/main.o"; then
  spawn "compiling main.cpp" g++ $CXXFLAGS -c src/main.cpp -o "$OUT/main.o"
fi
if ! tu_is_current "$OUT/FileManager.o"; then
  spawn "compiling FileManager.cpp" g++ $CXXFLAGS -c src/FileManager.cpp -o "$OUT/FileManager.o"
fi

fail=0
for i in "${!pids[@]}"; do
  if ! wait "${pids[$i]}"; then
    echo "${C_RED}  FAILED: ${labels[$i]}${C_RESET}" >&2
    fail=1
  fi
done
if [ "$fail" != "0" ]; then
  exit 1
fi

link_needed=0
if [ ! -f "$OUT/mewFM" ]; then
  link_needed=1
else
  for o in "$OUT/impl.o" "$OUT/nanovg.o" "$OUT/main.o" "$OUT/FileManager.o"; do
    if [ "$o" -nt "$OUT/mewFM" ]; then
      link_needed=1
      break
    fi
  done
fi

if [ "$link_needed" = "1" ]; then
  echo "${C_CYAN}>>> linking${C_RESET}"
  g++ "$OUT/impl.o" "$OUT/nanovg.o" "$OUT/main.o" "$OUT/FileManager.o" \
    $(pkg-config --cflags --libs glfw3 gl) \
    -lm -o "$OUT/mewFM"
  echo "${C_GREEN}>>> done:${C_RESET} ${C_BOLD}$OUT/mewFM${C_RESET}"
else
  echo "${C_DIM}>>> up to date: $OUT/mewFM${C_RESET}"
fi

install_asset() {
  local src="$1" dst="$2"
  if [ ! -f "$src" ]; then
    echo "  ${C_YELLOW}warning:${C_RESET} missing $src" >&2; return 1
  fi
  if [ -f "$dst" ] && cmp -s "$src" "$dst"; then
    return 0
  fi
  if install -D -m 644 "$src" "$dst" 2>/dev/null; then
    echo "  ${C_GREEN}installed:${C_RESET} ${C_DIM}$dst${C_RESET}"
  else
    sudo install -D -m 644 "$src" "$dst"
    echo "  ${C_GREEN}installed:${C_RESET} ${C_DIM}$dst${C_RESET}"
  fi
}

if [ -f /usr/bin/mewFM ] && cmp -s "$OUT/mewFM" /usr/bin/mewFM; then
  echo "${C_DIM}>>> /usr/bin/mewFM is up to date${C_RESET}"
else
  echo "${C_CYAN}>>> installing to /usr/bin${C_RESET}"
  if install -m 755 "$OUT/mewFM" /usr/bin/mewFM 2>/dev/null; then
    echo "${C_GREEN}>>> installed:${C_RESET} ${C_BOLD}/usr/bin/mewFM${C_RESET}"
  else
    sudo install -m 755 "$OUT/mewFM" /usr/bin/mewFM
    echo "${C_GREEN}>>> installed:${C_RESET} ${C_BOLD}/usr/bin/mewFM${C_RESET}"
  fi
fi

install_asset assets/icon.svg    /usr/share/icons/hicolor/scalable/apps/mewFM.svg
install_asset assets/mewFM.desktop /usr/share/applications/mewFM.desktop

echo "${C_GREEN}>>> done${C_RESET}"
