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

MODE="debug"; MODE_SET=0; CLEAN=0; FILENAME=""; ANDROID_BUILD=0
for arg in "$@"; do
  case "$arg" in
    --debug)   MODE="debug";   MODE_SET=1 ;;
    --release) MODE="release"; MODE_SET=1 ;;
    --clean)   CLEAN=1 ;;
    --android) ANDROID_BUILD=1 ;;
    -h|--help) echo "Usage: scripts/build.sh [--debug|--release|--clean|--android] [filename]"; exit 0 ;;
    -*) echo "${C_RED}build.sh: unknown option: $arg${C_RESET}" >&2; exit 1 ;;
    *)  FILENAME="$arg" ;;
  esac
done

if [ "$ANDROID_BUILD" = "1" ]; then
  echo "${C_BOLD}${C_CYAN}>>> Building Android APK${C_RESET}"
  
  if [ -z "$ANDROID_HOME" ] && [ -z "$ANDROID_SDK_ROOT" ]; then
    echo "Error: ANDROID_HOME or ANDROID_SDK_ROOT is not set." >&2
    exit 1
  fi
  SDK_DIR=${ANDROID_HOME:-$ANDROID_SDK_ROOT}
  BUILD_TOOLS=""
  if [ -d "$SDK_DIR/build-tools" ]; then
    latest_bt="$(ls -1 "$SDK_DIR/build-tools" 2>/dev/null | sort -V | tail -n 1)"
    if [ -n "$latest_bt" ] && [ -x "$SDK_DIR/build-tools/$latest_bt/d8" ]; then
      BUILD_TOOLS="$SDK_DIR/build-tools/$latest_bt"
    fi
  fi
  if [ -z "$BUILD_TOOLS" ]; then
    echo "Error: no build-tools directory with a usable d8 found under $SDK_DIR/build-tools" >&2
    echo "Install one via sdkmanager, e.g.: sdkmanager 'build-tools;34.0.0'" >&2
    exit 1
  fi
  echo "  ${C_CYAN}using build-tools:${C_RESET} $BUILD_TOOLS"
  PLATFORM="$SDK_DIR/platforms/android-33/android.jar"
  
  if [ ! -f "$PLATFORM" ]; then
    echo "Error: android.jar not found at $PLATFORM" >&2
    exit 1
  fi
  
  KOTLINC_BIN="$(command -v kotlinc || true)"
  if [ -z "$KOTLINC_BIN" ]; then
    echo "Error: kotlinc not found in PATH." >&2
    exit 1
  fi

  KOTLIN_STDLIB=""
  # 1. Explicit overrides
  if [ -n "${KOTLIN_STDLIB_PATH:-}" ] && [ -f "$KOTLIN_STDLIB_PATH" ]; then
    KOTLIN_STDLIB="$KOTLIN_STDLIB_PATH"
  elif [ -n "${KOTLIN_HOME:-}" ] && [ -f "$KOTLIN_HOME/lib/kotlin-stdlib.jar" ]; then
    KOTLIN_STDLIB="$KOTLIN_HOME/lib/kotlin-stdlib.jar"
  fi

  # 2. Common install locations (Void, Arch, Debian, SDKMAN, manual)
  if [ -z "$KOTLIN_STDLIB" ]; then
    for candidate in \
      /usr/lib/kotlin/kotlin-stdlib.jar \
      /usr/lib/kotlin/lib/kotlin-stdlib.jar \
      /usr/share/kotlin/kotlin-stdlib.jar \
      /usr/share/kotlin/lib/kotlin-stdlib.jar \
      /usr/lib/kotlin-stdlib.jar \
      /opt/kotlin/lib/kotlin-stdlib.jar \
      "$HOME/.sdkman/candidates/kotlin/current/lib/kotlin-stdlib.jar" \
    ; do
      if [ -f "$candidate" ]; then
        KOTLIN_STDLIB="$candidate"
        break
      fi
    done
  fi

  # 3. Relative to the kotlinc binary (works when it is a real file or symlink)
  if [ -z "$KOTLIN_STDLIB" ]; then
    KOTLINC_REAL="$(readlink -f "$KOTLINC_BIN" 2>/dev/null || echo "$KOTLINC_BIN")"
    for candidate in \
      "$(dirname "$KOTLINC_REAL")/../lib/kotlin-stdlib.jar" \
      "$(dirname "$KOTLINC_REAL")/kotlin-stdlib.jar" \
    ; do
      if [ -f "$candidate" ]; then
        KOTLIN_STDLIB="$candidate"
        break
      fi
    done
  fi

  # 4. Last-resort filesystem search
  if [ -z "$KOTLIN_STDLIB" ]; then
    found="$(find /usr /opt /usr/local -name 'kotlin-stdlib.jar' -type f 2>/dev/null | head -n 1)"
    if [ -n "$found" ]; then
      KOTLIN_STDLIB="$found"
    fi
  fi

  if [ -z "$KOTLIN_STDLIB" ]; then
    echo "Error: kotlin-stdlib.jar not found." >&2
    echo "Set KOTLIN_STDLIB_PATH=/path/to/kotlin-stdlib.jar, or install the kotlin-stdlib package." >&2
    exit 1
  fi

  echo "  ${C_CYAN}using kotlin-stdlib:${C_RESET} $KOTLIN_STDLIB"
  if [ -f build/android/mewFM.apk ] \
     && [ ! android/AndroidManifest.xml -nt build/android/mewFM.apk ] \
     && [ ! build/android/dex_user/classes.dex -nt build/android/mewFM.apk ] \
     && [ ! build/android/dex_lib/classes.dex -nt build/android/mewFM.apk ]; then
    _NEEDS_BUILD=0
    while IFS= read -r _f; do
      if [ "$_f" -nt build/android/mewFM.apk ]; then _NEEDS_BUILD=1; break; fi
    done < <(find android/src -name '*.kt' 2>/dev/null)
    if [ "$_NEEDS_BUILD" = "0" ]; then
      echo "${C_DIM}>>> Android APK up to date: build/android/mewFM.apk${C_RESET}"
      exit 0
    fi
  fi
  mkdir -p build/android/classes build/android/dex_user build/android/dex_lib
  
  _NEED_KOTLINC=0
  if [ ! -f build/android/classes.jar ]; then
    _NEED_KOTLINC=1
  else
    while IFS= read -r _f; do
      if [ "$_f" -nt build/android/classes.jar ]; then _NEED_KOTLINC=1; break; fi
    done < <(find android/src -name '*.kt' 2>/dev/null)
  fi
  if [ "$_NEED_KOTLINC" = "1" ]; then
    echo "  ${C_CYAN}>>> Compiling Kotlin${C_RESET}"
    rm -rf build/android/classes && mkdir -p build/android/classes
    kotlinc android/src \
      -classpath "$PLATFORM" \
      -d build/android/classes
    if [ $? -ne 0 ]; then echo "Kotlin compilation failed." >&2; exit 1; fi
    jar cf build/android/classes.jar -C build/android/classes .
    if [ $? -ne 0 ]; then echo "jar failed." >&2; exit 1; fi
  else
    echo "  ${C_DIM}>>> Kotlin up to date${C_RESET}"
  fi
    
  if [ $? -ne 0 ]; then echo "Kotlin compilation failed." >&2; exit 1; fi

  if [ ! -f build/android/dex_lib/classes.dex ] || [ "$KOTLIN_STDLIB" -nt build/android/dex_lib/classes.dex ]; then
    echo "  ${C_CYAN}>>> DEX kotlin-stdlib (cached)${C_RESET}"
    rm -rf build/android/dex_lib && mkdir -p build/android/dex_lib
    "$BUILD_TOOLS/d8" --lib "$PLATFORM" --min-api 33 \
      --output build/android/dex_lib "$KOTLIN_STDLIB"
    if [ $? -ne 0 ]; then echo "DEX stdlib failed." >&2; exit 1; fi
  else
    echo "  ${C_DIM}>>> DEX kotlin-stdlib up to date${C_RESET}"
  fi

  if [ ! -f build/android/dex_user/classes.dex ] || [ build/android/classes.jar -nt build/android/dex_user/classes.dex ]; then
    echo "  ${C_CYAN}>>> DEX app${C_RESET}"
    rm -rf build/android/dex_user && mkdir -p build/android/dex_user
    "$BUILD_TOOLS/d8" --lib "$PLATFORM" --classpath "$KOTLIN_STDLIB" --min-api 33 \
      --output build/android/dex_user build/android/classes.jar
    if [ $? -ne 0 ]; then echo "DEX app failed." >&2; exit 1; fi
  else
    echo "  ${C_DIM}>>> DEX app up to date${C_RESET}"
  fi

  ICON_SRC="assets/icon.svg"
  ICON_CACHE="build/android_cache"
  ICON_DIR="$ICON_CACHE/res_icons"
  ICON_ZIP="$ICON_CACHE/res_icons.zip"
  if [ -f "$ICON_SRC" ]; then
    if [ -f "$ICON_ZIP" ] && [ ! "$ICON_SRC" -nt "$ICON_ZIP" ]; then
      echo "  ${C_DIM}>>> Launcher icons up to date${C_RESET}"
    else
      _RSVG="$(command -v rsvg-convert || true)"
      _INK="$(command -v inkscape || true)"
      _CVT="$(command -v convert || true)"
      _CONV=""
      if [ -n "$_RSVG" ]; then _CONV="rsvg"
      elif [ -n "$_INK" ]; then _CONV="inkscape"
      elif [ -n "$_CVT" ]; then _CONV="convert"
      fi
      if [ -z "$_CONV" ]; then
        echo "  ${C_RED}error:${C_RESET} no SVG-to-PNG converter found." >&2
        echo "  Install one: sudo xbps-install -Su librsvg" >&2
        exit 1
      fi
      echo "  ${C_CYAN}>>> Generating launcher icons from $ICON_SRC${C_RESET}"
      rm -rf "$ICON_DIR"
      for pair in mdpi:48 hdpi:72 xhdpi:96 xxhdpi:144 xxxhdpi:192; do
        _d="${pair%%:*}"; _s="${pair##*:}"
        mkdir -p "$ICON_DIR/mipmap-$_d"
        case "$_CONV" in
          rsvg)     "$_RSVG" -w "$_s" -h "$_s" -o "$ICON_DIR/mipmap-$_d/ic_launcher.png" "$ICON_SRC" ;;
          inkscape) "$_INK" "$ICON_SRC" --export-type=png \
                      --export-filename="$ICON_DIR/mipmap-$_d/ic_launcher.png" \
                      -w "$_s" -h "$_s" 2>/dev/null ;;
          convert)  "$_CVT" -background none -density 384 -resize "${_s}x${_s}" \
                      "$ICON_SRC" "$ICON_DIR/mipmap-$_d/ic_launcher.png" ;;
        esac
      done
      rm -f "$ICON_ZIP"
      "$BUILD_TOOLS/aapt2" compile --dir "$ICON_DIR" -o "$ICON_ZIP"
      if [ $? -ne 0 ]; then echo "Icon resource compile failed." >&2; exit 1; fi
    fi
  else
    echo "  ${C_YELLOW}warning:${C_RESET} $ICON_SRC not found, APK will use default icon." >&2
  fi

  echo "  ${C_CYAN}>>> Packaging resources and manifest${C_RESET}"
  AAPT_INPUTS=()
  if [ -f "$ICON_CACHE/res_icons.zip" ]; then
    AAPT_INPUTS+=("$ICON_CACHE/res_icons.zip")
  fi
  if [ -f "$ICON_CACHE/res_base.zip" ]; then
    AAPT_INPUTS+=("$ICON_CACHE/res_base.zip")
  fi
  "$BUILD_TOOLS/aapt2" link \
    -o build/android/app-unsigned.apk \
    -I "$PLATFORM" \
    --manifest android/AndroidManifest.xml \
    --min-sdk-version 33 \
    --target-sdk-version 33 \
    ${AAPT_INPUTS[@]+"${AAPT_INPUTS[@]}"}
    
  if [ $? -ne 0 ]; then echo "aapt2 link failed." >&2; exit 1; fi

  echo "  ${C_CYAN}>>> Adding DEX to APK${C_RESET}"
  ( cd build/android/dex_user && zip -q ../app-unsigned.apk classes.dex )
  if [ $? -ne 0 ]; then echo "Adding app DEX failed." >&2; exit 1; fi
  cp -f build/android/dex_lib/classes.dex build/android/dex_lib/classes2.dex
  ( cd build/android/dex_lib && zip -q ../app-unsigned.apk classes2.dex )
  if [ $? -ne 0 ]; then echo "Adding stdlib DEX failed." >&2; exit 1; fi

  echo "  ${C_CYAN}>>> Aligning and Signing${C_RESET}"
  "$BUILD_TOOLS/zipalign" -f -p 4 build/android/app-unsigned.apk build/android/app-aligned.apk
  
  # Generate a debug keystore if one doesn't exist
  if [ ! -f ~/.android/debug.keystore ]; then
    mkdir -p ~/.android
    keytool -genkey -v -keystore ~/.android/debug.keystore -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US" 2>/dev/null
  fi

  "$BUILD_TOOLS/apksigner" sign --ks ~/.android/debug.keystore --ks-pass pass:android --key-pass pass:android --out build/android/mewFM.apk build/android/app-aligned.apk
  
  if [ $? -eq 0 ]; then
    echo "${C_GREEN}>>> done:${C_RESET} ${C_BOLD}build/android/mewFM.apk${C_RESET}"
  else
    echo "Signing failed." >&2
    exit 1
  fi
  exit 0
fi

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
    -lX11 -lm -o "$OUT/mewFM"
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
