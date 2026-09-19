#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# ---------- colors ----------
if [ -t 1 ]; then
  C_RESET=$'\033[0m'
  C_DIM=$'\033[2m'
  C_BOLD=$'\033[1m'
  C_CYAN=$'\033[36m'
  C_YELLOW=$'\033[33m'
  C_GREEN=$'\033[32m'
  C_RED=$'\033[31m'
else
  C_RESET=""; C_DIM=""; C_BOLD=""
  C_CYAN=""; C_YELLOW=""; C_GREEN=""; C_RED=""
fi

# ---------- arguments ----------
MODE="debug"
MODE_SET=0
CLEAN=0
FILENAME=""
for arg in "$@"; do
  case "$arg" in
    --debug)   MODE="debug";   MODE_SET=1 ;;
    --release) MODE="release"; MODE_SET=1 ;;
    --clean)   CLEAN=1 ;;
    -h|--help)
      echo "Usage: scripts/build.sh [--debug|--release|--clean] [filename]"
      exit 0
      ;;
    -*) echo "${C_RED}build.sh: unknown option: $arg${C_RESET}" >&2; exit 1 ;;
    *)  FILENAME="$arg" ;;
  esac
done

# ---------- clean ----------
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

# ---------- build ----------
OUT="build/$MODE"
GEN="build/generated"
mkdir -p "$OUT" "$GEN"

CXXFLAGS="-std=c++17 -O2 -Wunused -Wunreachable-code -Isrc -I$GEN -Ithird_party/nanovg -Ithird_party/oui-blendish -DGL_GLEXT_PROTOTYPES"
CFLAGS="-O2 -Ithird_party/nanovg -Ithird_party/oui-blendish -DGL_GLEXT_PROTOTYPES"

echo "${C_BOLD}${C_CYAN}>>> mode:${C_RESET} ${C_BOLD}$MODE${C_RESET}"

echo "${C_CYAN}>>> embedding assets (xxd)${C_RESET}"
xxd -i -n fontData assets/fonts/Hermit/HurmitNerdFont-Regular.otf > "$GEN/HurmitFont.hpp"
xxd -i -n iconData third_party/oui-blendish/blender_icons16.png > "$GEN/BlenderIcons.hpp"

echo "${C_CYAN}>>> compiling implementation TU (C)${C_RESET}"
gcc $CFLAGS -fgnu89-inline -c third_party/impl.c -o "$OUT/impl.o"
gcc $CFLAGS -c third_party/nanovg/nanovg.c -o "$OUT/nanovg.o"

echo "${C_CYAN}>>> compiling sources (C++)${C_RESET}"
g++ $CXXFLAGS -c src/main.cpp -o "$OUT/main.o"
g++ $CXXFLAGS -c src/FileManager.cpp -o "$OUT/FileManager.o"
g++ $CXXFLAGS -c src/MusicPlayer.cpp -o "$OUT/MusicPlayer.o"
g++ $CXXFLAGS -c src/VideoPlayer.cpp -o "$OUT/VideoPlayer.o"

echo "${C_CYAN}>>> linking${C_RESET}"
g++ "$OUT/impl.o" "$OUT/nanovg.o" "$OUT/main.o" "$OUT/FileManager.o" "$OUT/MusicPlayer.o" "$OUT/VideoPlayer.o" \
  $(pkg-config --cflags --libs glfw3 gl) \
  -lmpv \
  -lm -o "$OUT/rah"

echo "${C_GREEN}>>> done:${C_RESET} ${C_BOLD}$OUT/rah${C_RESET}"

install_asset() {
  local src="$1"
  local dst="$2"
  if [ ! -f "$src" ]; then
    echo "  ${C_YELLOW}warning:${C_RESET} missing $src" >&2
    return 1
  fi
  if install -D -m 644 "$src" "$dst" 2>/dev/null; then
    echo "  ${C_GREEN}installed:${C_RESET} ${C_DIM}$dst${C_RESET}"
  else
    sudo install -D -m 644 "$src" "$dst"
    echo "  ${C_GREEN}installed:${C_RESET} ${C_DIM}$dst${C_RESET}"
  fi
}

echo "${C_CYAN}>>> installing rah to /usr/bin${C_RESET}"
if install -m 755 "$OUT/rah" /usr/bin/rah 2>/dev/null; then
  echo "${C_GREEN}>>> installed:${C_RESET} ${C_BOLD}/usr/bin/rah${C_RESET}"
else
  echo "  ${C_DIM}requires sudo${C_RESET}"
  sudo install -m 755 "$OUT/rah" /usr/bin/rah
  echo "${C_GREEN}>>> installed:${C_RESET} ${C_BOLD}/usr/bin/rah${C_RESET}"
fi

install_asset assets/icon.svg    /usr/share/icons/hicolor/scalable/apps/rah.svg
install_asset assets/rah.desktop /usr/share/applications/rah.desktop

echo "${C_GREEN}>>> done${C_RESET}"

echo "${C_GREEN}>>> launching rah${C_RESET}"
"$OUT/rah"

echo ""
while true; do
  read -r -p "Were you happy with the new feature? [1] Yes  [2] No: " answer
  case "$answer" in
    1|y|Y|yes|YES|Yes)
      if [ -n "$FILENAME" ]; then
        rm "$FILENAME"
        echo "Patch: $FILENAME has been removed"
      else
        echo "Yes"
      fi
      scc src/
      break ;;
    2|n|N|no|NO|No)
      echo "GoodBye!"
      scc src/
      break ;;
    *) echo "Please answer 1 (Yes) or 2 (No)." ;;
  esac
done
