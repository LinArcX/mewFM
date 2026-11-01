#!/usr/bin/bash
# to run this file: ./p
#
# nvim tips:
#   file manager: ":Neotree" or ", b f"
#     to get help on Neotree, just press ?
#     toggle between Neotree: Shift-f
#   splits
#     sp: horizontal split
#     vs: vertical split
#     only: keep only this split(close others)
#   switch between source/header: F2
#   search files: ctrl-f, alt-f, C/Tab
#   search strings: ctrl-g, alt-g, /
#   find string/text in whole project: ft <text> | rg <text>
#   folding/unfolding:
#     zM: close all folds.
#     zR: open all folds.
#     za: toggle one fold under cursor.
#     zA: toggle All folds under cursor.
#     zc: close one fold under cursor.
#     zC: close All folds under cursor.
#   documentation:
#     , c c: generate doxygen doc for a class
#     , n f: generate doxygen doc for a function
#     run: doxygen in root of project from a terminal
#   lsp: ,l
#   find/replace:
#     project: ambr <src> <dst> <path>
#     current buffer: :%s/foo/bar/g
#   diff between two files/dirs: delta <DIR1> <DIR2>
#   debuggers: dap in neovim, gf2, gdb
#   find docs of c standard librariy: 
#     - install man-pages-devel and man <method>
#     - zeal
#     - www.devdocs.io 

# Notification sounds are opt-in. Set these to absolute paths in
# your shell rc to play a sound when a build succeeds or fails.
# Default empty -> silent.
export MEWFM_SOUND_SUCCESS="${MEWFM_SOUND_SUCCESS:-}"
export MEWFM_SOUND_ERROR="${MEWFM_SOUND_ERROR:-}"

# Plays an optional build-result sound. No-op when the matching
# env var is empty, the file is missing, or mpg123 is unavailable.
function playSound()
{
  local kind="$1"
  local soundFile=""
  if [ "$kind" = "error" ]; then
    soundFile="$MEWFM_SOUND_ERROR"
  elif [ "$kind" = "success" ]; then
    soundFile="$MEWFM_SOUND_SUCCESS"
  fi
  if [ -z "$soundFile" ] || [ ! -f "$soundFile" ]; then
    return
  fi
  if ! command -v mpg123 > /dev/null 2>&1; then
    return
  fi
  mpg123 -f 3000 "$soundFile" > /dev/null 2>&1 || true
}

export MEWRM_DEVICE_IP="${MEWRM_DEVICE_IP:-}"

# Returns the target device IP. Uses $MEWRM_DEVICE_IP when set,
# otherwise prompts interactively. Prints the IP to stdout on success.
function pickDeviceIp()
{
  local ip="$MEWRM_DEVICE_IP"
  if [ -z "$ip" ]; then
    read -r -p "Device IP: " ip
  fi
  if [ -z "$ip" ]; then
    echo "No device IP provided (set MEWRM_DEVICE_IP or answer the prompt)." >&2
    return 1
  fi
  printf '%s\n' "$ip"
}

function applyPatch() {
  if [ -n $1 ]; then
    if python3 scripts/extractFiles.py "$1"; then
      echo "Patch Success!"
      ./scripts/build.sh --debug
    else
      echo "Patch Failed!"
    fi
  fi
}

menu () {
  commands=(
    # patch
    "create patch"
    "apply patch"

    # real device
    "android-check"
    "pair device"
    "build android(arm64_v8a)"
    "install on real device(arm64_v8a) with wifi"
 
    # emulator
    "build android(x86_64)"
    "launch qemu emulator (android x86-64)" "connect to emulator"
    "install .apk to emulator" "uninstall .apk from emulator"
    "adb run" "adb log" "adb close apk" "adb reboot android OS" "adb poweroff android OS"


    # debug
    "build(debug)" "run(debug)"  "gf2" "gf2 attach" "rr record" "gf2 --rr-replay" "clean(debug)"

    # release
    "build(release)" "run(release)" "clean(release)"

    # android
    "build(android)" "install(android)" "run(android)" "clean(android)"
  
    "start xephyr" "stop xephyr"
 
    # documentation
    "doxygen(show)" 
 
    # static analyzer
    "cppcheck(generate)"
    "cppcheck(show)"
 
    "frama-c" "frama-c (eva)" "ivette (eva)" "frama-c-gui (eva)"
  
    # code coverage
    "lcov"
    "gcovr"
    "genhtml"
    "show lcov report"
    "kcov(generate)"
    "kcov(show)"
 
    # we can't use lcov, gcov, gcovr(requires: gcc-multilib) with zagros, because these tools asks us to re-compile zagros with:
    #   --coverage -fprofile-arcs -ftest-coverage 
    #   linker flags: -L/usr/lib/gcc/x86_64-unknown-linux-gnu/14.2/32/ -lgcov
    # which is not possible. because there's no libc compiled with zagros. it's statically linked.
    # but we can use them for zagros_test
  
    # trace/profile
    "valgrind(memcheck)" "callgrind" "kcachegrind" "cachegrind" "helgrind" "massif" "ms_print" "drd" "dhat" "dhat(cat)" "bbv" "bbv(cat)"
    "perf"
    "uftrace replay"
    "uftrace record"
    "uftrace dump"
    "uftrace report"
    "uftrace info"
    "uftrace graph"
    "uftrace tui"
  
    # we can't use libasan as compiler flag. (-fsanitize=address) and as linker flag (-L/usr/lib32 -lasan),
    # since zagros is statically linked and this flag needs libc compiled with zagros.
    
    # we can't use uftrace, because of many reasons:
    #   1. it just works with dynamically linked binaries. zagros is statically linked, so uftrace can't work with it.
    #   2. it needs to pass -pg to compiler option, and this requires mcount from clib. but we're building our kernel without libc!
    #   3. even with -finstrument-functions, our binary is still static. so uftrace can't work!
  
    # alternative: manual tracing using this macro for each function:
    #   #define TRACE_ENTER() do { asm volatile("syscall" : : "a"(1), "D"(1), "S"("Entering " __func__ "\n"), "d"(strlen("Entering " __func__ "\n")) : "rcx", "r11", "memory"); } while(0)  // Raw write syscall// Add TRACE_ENTER() at function starts; similar for exit.
  
    # binary analyse
    "ldd" "objdump" "strings" "nm" "readelf" "addr2line" "size" "file" "xxd"
  
    # util
    "scc" "search" "search/replace")
  selected=$(printf '%s\n' "${commands[@]}" | fzf --header="project:")
  
  case $selected in
    "create patch")
      clear
      read -r -p "Enter name of your patch [$(date '+%Y_%m_%d_%H_%M')]: " filename
      if [ -z "$filename" ]; then
        filename="$(date '+%Y_%m_%d_%H_%M')"
      fi
      
      file="patches/${filename}.json"
      
      nvim "$file"
      
      if [ $? -eq 0 ] && [ -s "$file" ]; then
        applyPatch "$file"

        echo -e "Removing patch $file .."
        rm -f "$file"
      fi
      ;;
    "apply patch")
      file=$(find patches/ -type f -printf '%f\n' | fzf)
      [ -n "$file" ] && applyPatch "$file"
      ;;
    "android-check")
      echo ">>> not implemented: android-check"
      return
      ./scripts/build.sh android-check
      ;;
    "pair device")
      deviceIp=$(pickDeviceIp) || return
      read -r -p "Port: " portNumber
      read -r -p "Pair code: " pairCode
      adb pair "${deviceIp}:${portNumber}" "${pairCode}"
      ;;
    "build android(arm64_v8a)")
      #./scripts/build.sh --debug --android --arm64_v8a
      ./scripts/build.sh --android
      if [ $? -ne 0 ]; then
        # error
        playSound error
      else
        # success
        playSound success
      fi
      ;;
    "install on real device(arm64_v8a) with wifi")
      echo "Showing Devices"
      output=$(adb devices | awk 'NR > 1 && $2 == "device" {print $1}')
 
      # Sometimes you can't connect to your device, you need to check mdns first:
      #   adb mdns check
      #   adb mdns services
      #   adb connect <IP>:<PORT>
      #   adb pair <IP>:<PORT>
      #   adb devices
      #
      # If you see your device is offline:
      #  adb devices
      #   List of devices attached
      #   192.168.1.58:40347	offline
      # do these things:
      #   adb disconnect
      #   adb kill-server
      #   adb start-server
      #   adb mdns services
      #   adb pair <IP>:<PORT>

      if [ -n "$output" ]; then
        echo "Found an ADB device"
      else
        echo "No ADB device found"
        deviceIp=$(pickDeviceIp) || return
        echo "Enter the port"
        read portNumber

        echo "Enter the pair code"
        read pairCode

        echo "Pairing"
        adb pair "${deviceIp}:${portNumber}" "${pairCode}"
        output=$(adb devices | awk 'NR > 1 && $2 == "device" {print $1}')
      fi

      device=$(echo "$output" | head -n 1)
      echo "Device: $device"

      echo "Connecting to device"
      adb connect $device

      echo "Setting time-out for 1 day"
      adb -s $device shell settings put system screen_off_timeout 86400000

      # need to extract to separate function
      PROJECT_DIR="$PWD"
      DEBUG_DIR="$PROJECT_DIR/build/android"
      RELEASE_DIR="$PROJECT_DIR/build/android-release"
 
      APK_LIST=$(
        find "$DEBUG_DIR" "$RELEASE_DIR" \
          -type f \
          -name 'mewFM*.apk' \
          2>/dev/null
      )
      
      if [ -z "$APK_LIST" ]; then
        echo "No APK files found in:"
        echo "  $DEBUG_DIR"
        echo "  $RELEASE_DIR"
        return 1
      fi
      
      APK=$(printf '%s\n' "$APK_LIST" | fzf \
        --height=40% \
        --layout=reverse \
        --border \
        --prompt="Select APK: " \
        --header="Choose an APK to install")
      
      if [ -z "$APK" ]; then
        echo "Installation cancelled."
        return 0
      fi
      # need to extract to separate function
 
      echo "--> Installing:"
      echo "  $APK"
      adb -s $device install -r "$APK"
      ;;
    "clean android")
      echo ">>> cleaning build/android"
      rm -rf build/android
      if [ $? -ne 0 ]; then
        # error
        playSound error
      else
        # success
        playSound success
      fi
      ;;
    "build android(x86_64)")
      ./scripts/build.sh android x86
      if [ $? -ne 0 ]; then
        # error
        playSound error
      else
        # success
        playSound success
      fi
      ;;
    "launch qemu emulator (android x86-64)")
      qemu-system-x86_64 \
        -enable-kvm \
        -m 6144 \
        -smp cpus=4 \
        -nic user,hostfwd=tcp:127.0.0.1:5555-:5555 \
        -drive file=~/qemu/android.qcow2,format=qcow2 &
      echo -e "Please first run these commands in Android X86 Shell, before trying to connect adb to it:\nsu\nsetprop service.adb.tcp.port 5555\nstop adbd\nstart adbd"
      ;;
    "connect to emulator")
      # adb devices
      # adb connect 127.0.0.1:5555
      devices=$(adb devices | awk 'NR > 1 && $2 == "device" {print $1}')

      if [ -z "$devices" ]; then
        printf '\033[31m✗ Error:\033[0m No ADB devices found.\n' >&2
        return 1
      fi
      
      selected=$(printf '%s\n' "$devices" | fzf \
        --height=40% \
        --layout=reverse \
        --border \
        --prompt='ADB device > ')
      
      if [ -z "$selected" ]; then
        return 0
      fi
      
      printf '\033[36m→ Connecting to:\033[0m %s\n' "$selected"
      
      adb connect "$selected"
      ;;
    "install .apk to emulator")
      PROJECT_DIR="$PWD"
      DEBUG_DIR="$PROJECT_DIR/build/android"
      RELEASE_DIR="$PROJECT_DIR/build/android-release"
      
      APK_LIST=$(
        find "$DEBUG_DIR" "$RELEASE_DIR" \
          -type f \
          -name 'mewRM*.apk' \
          2>/dev/null
      )
      
      if [ -z "$APK_LIST" ]; then
        echo "No APK files found in:"
        echo "  $DEBUG_DIR"
        echo "  $RELEASE_DIR"
        return 1
      fi
      
      APK=$(printf '%s\n' "$APK_LIST" | fzf \
        --height=40% \
        --layout=reverse \
        --border \
        --prompt="Select APK: " \
        --header="Choose an APK to install")
      
      if [ -z "$APK" ]; then
        echo "Installation cancelled."
        return 0
      fi
      
      echo "--> Installing:"
      echo "  $APK"
      adb -s 127.0.0.1:5555 install -r "$APK"

      echo "--> Running mewRM..."
      adb -s 127.0.0.1:5555 shell am start -n com.mewrm/.MainActivity
      ;;
    "uninstall .apk from emulator")
      adb -s 127.0.0.1:5555 uninstall com.mewrm
      ;;
    "adb run")
      adb -s 127.0.0.1:5555 shell am start -n com.mewrm/.MainActivity
      ;;
    "adb log")
      adb -s 127.0.0.1:5555 logcat | grep -i mewRM
      ;;
    "adb close apk")
      adb -s 127.0.0.1:5555 shell am force-stop com.mewrm
      ;;
    "adb reboot android OS")
      adb -s 127.0.0.1:5555 reboot
      ;;
    "adb poweroff android OS")
      adb -s 127.0.0.1:5555 shell reboot --poweroff
      ;;

    #"build(android)")
    #  ./scripts/build.sh --android
    #  if [ $? -eq 1 ]; then
    #    # error
    #    mpg123 -f 3000 /home/linarcx/VoidConf/assets/error2.mp3 > /dev/null 2>&1 
    #  else
    #    # success
    #    mpg123 -f 3000 /home/linarcx/VoidConf/assets/success.mp3 > /dev/null 2>&1 
    #  fi
    #  ;;
    #"install(android)")
    #  adb install -r build/android/mewFM.apk
    #  ;;
    #"run(android)")
    #  adb shell am start -n com.mewfm/.MainActivity
    #  ;;
    #"clean(android)")
    #  rm -rf build/android
    #  echo ">>> cleaned build/android"
    #  ;;
    "build(debug)")
      ./scripts/build.sh --debug
      if [ $? -eq 1 ]; then
        # error
        mpg123 -f 3000 /home/linarcx/VoidConf/assets/error2.mp3 > /dev/null 2>&1 
      else
        # success
        mpg123 -f 3000 /home/linarcx/VoidConf/assets/success.mp3 > /dev/null 2>&1 
      fi
      ;;
    "run(debug)")
      clear
      cd build/debug
      ./mewFM 
      cd ../..
      scc src/
      ;;
    "gf2")
      gf2 -nx -ex "source breakpoints.gdb" build/debug/mewFM
      ;;
    "gf2 attach")
      # if you get this error: 
      # Attaching to process 18446 ❌️ ptrace: Operation not permitted.
      # just run:
      #   sudo sysctl kernel.yama.ptrace_scope=0
      gf2 -nx -ex "source breakpoints.gdb" -p "$(pidof mewFM)" >/dev/null 2>&1 & disown
      sleep 2
      wmctrl -r "gf2" -e 0,0,500,1820,550

      #gf2 -nx -ex "source breakpoints.gdb" -p $(pidof mewFM) &
      #until wmctrl -l | grep -q "gf2"; do
      #  sleep 0.1
      #done
      #wmctrl -r "gf2" -e 0,0,500,1820,550
      #wait
      #wait
      ;;
    "rr record")
      /usr/bin/rr record build/debug/mewFM
      ;;
    "gf2 --rr-replay")
      gf2 --rr-replay -x breakpoints.gdb
      ;;
    "clean(debug)")
      echo ">>> cleaning build/debug directory"
      ./scripts/build.sh --clean --debug
      if [ $? -eq 1 ]; then
        # error
        mpg123 -f 3000 /home/linarcx/VoidConf/assets/error2.mp3 > /dev/null 2>&1 
      else
        # success
        mpg123 -f 3000 /home/linarcx/VoidConf/assets/success.mp3 > /dev/null 2>&1 
      fi
      ;;
    "build(release)")
      ./scripts/build.sh --release 
      if [ $? -eq 1 ]; then
        # error
        mpg123 -f 3000 /home/linarcx/VoidConf/assets/error2.mp3 > /dev/null 2>&1 
      else
        # success
        mpg123 -f 3000 /home/linarcx/VoidConf/assets/success.mp3 > /dev/null 2>&1 
      fi
     ;;
    "run(release)")
      echo ">>> running mewFM (release)"
      cd build/release
      ./mewFM
      cd ../..
      scc src/
      ;;
    "clean(release)")
      ./scripts/build.sh --clean --release
      if [ $? -eq 1 ]; then
        # error
        mpg123 -f 3000 /home/linarcx/VoidConf/assets/error2.mp3 > /dev/null 2>&1 
      else
        # success
        mpg123 -f 3000 /home/linarcx/VoidConf/assets/success.mp3 > /dev/null 2>&1 
      fi
      ;;
    "start xephyr")
      #./start_xephyr.sh 
      Xephyr :1 -screen 1280x800 -ac &
      DISPLAY=:1 ./build/debug/mewFM &
      DISPLAY=:1 wezterm start &
      ;;
    "stop xephyr")
      ./scripts/stop_xephyr.sh
      ;;
    "doxygen(show)")
      ~/software/brave/brave-browser-1.86.142-linux-amd64/brave ./docs/doxygen/html/index.html
      ;;
    "cppcheck")
      rm report/*
      # --suppressions-list=suppress.txt 
      # Suppress a specific warning. The format of <spec> is: [error id]:[filename]:[line].
      # - The [filename] and [line] are optional.
      # - [error id] may be * to suppress all warnings (for a specified file or files).
      # - [filename] may contain the wildcard characters * or ?.
      cppcheck --addon=cppcheck/misra.json --addon=cppcheck/findcasts.json --addon=cppcheck/misc.json --addon=cppcheck/y2038.json --addon=cppcheck/threadsafety.json --inline-suppr --std=c11 --enable=all --error-exitcode=1 --platform=unix64 --report-type=misra-c-2012 -q --xml --xml-version=2 lib/util/*.c lib/*.c example/*.c -I lib/util/ > report/cppcheck.xml 2>&1
      cppcheck-htmlreport --file=report/cppcheck.xml --title="mewFM" --report-dir=report --source-dir=.
      ;;
    "cppcheck(show)")
      ~/software/brave/brave-browser-1.86.142-linux-amd64/brave ./report/index.html
      #xdg-open report/index.html
      ;;
    "frama-c")
      frama-c -json-compilation-database compile_commands.json example/* lib/* lib/util/*
      ;;
    "frama-c (eva)")
      frama-c -json-compilation-database compile_commands.json example/* lib/* lib/util/* -eva -eva-precision 11
      ;;
    "ivette (eva)")
      ivette -json-compilation-database compile_commands.json example/* lib/* lib/util/* -eva -eva-precision 11
      ;;
    "frama-c-gui (eva)")
      frama-c-gui -json-compilation-database compile_commands.json example/* lib/* lib/util/* -eva -eva-precision 11
      ;;
    "lcov")
      # https://wiki.cs.jmu.edu/student/gcov/start
      ./build/debug/mewFM
      lcov --capture --directory build/debug --output-file build/debug/coverage.info
      ;;
    "gcovr")
      gcovr build/debug
      ;;
    "genhtml")
      genhtml build/debug/coverage.info --output-directory build/debug/coverage_report
      ;;
    "show lcov report")
      ~/software/brave/brave-browser-1.86.142-linux-amd64/brave ./build/debug/coverage_report/index.html
      # xdg-open unit_test/build/coverage_report/index.html
      ;;
    "kcov(generate)")
      rm -r coverage/*
      kcov coverage/ build/debug/mewFM
      ;;
    "kcov(show)")
      ~/software/brave/brave-browser-1.86.142-linux-amd64/brave ./coverage/index.html
      # xdg-open coverage/index.html
      ;;
    "llvm-cov")
      ;;
    "valgrind(memcheck)")
      valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --xtree-leak=yes -s -v build/debug/mewFM
      ;;
    "callgrind")
      valgrind --tool=callgrind --dump-instr=yes --collect-jumps=yes -s -v build/debug/mewFM
      ;;
    "kcachegrind")
      ls callgrind.out.* cachegrind.out.* | fzf --header="kcachgrind: " | xargs kcachegrind
      ;;
    "cachegrind")
      valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes -s -v build/debug/mewFM
      ;;
    "helgrind")
      valgrind --tool=helgrind -s -v build/debug/mewFM
      ;;
    "massif")
      valgrind --tool=massif -s -v build/debug/mewFM
      ;;
    "ms_print")
      ls massif.out.* | fzf --header="ms_print: " | xargs ms_print
      ;;
    "drd")
      valgrind --tool=drd --trace-fork-join=yes --trace-mutex=yes --trace-semaphore=yes -s -v build/debug/mewFM
      ;;
    "dhat")
      valgrind --tool=dhat -s -v build/debug/mewFM
      ;;
    "dhat(cat)")
      ls dhat.out.* | fzf --header="dhat: " | xargs cat | less
      ;;
    "bbv")
      valgrind --tool=exp-bbv -s -v build/debug/mewFM
      ;;
    "bbv(cat)")
      ls bb.out.* | fzf --header="bbv: " | xargs cat | less
      ;;
    "perf")
      ls build/debug | fzf --header="perf: " | xargs perf stat -d
      ;;
    "uftrace record(test)")
      uftrace --srcline -a --symbols --time --no-libcall --symbols -F __clove_symint___UtilSuite* record build/debug/mewFM
      ;;
    "uftrace replay(tests)")
      uftrace replay
      ;;
    "uftrace dump(test)")
      uftrace dump --chrome > dump.json
      echo "load dump.json in chrome://tracing/"
      ;;
    "uftrace report(test)")
      uftrace report
      ;;
    "uftrace info(test)")
      uftrace info
      ;;
    "uftrace graph(test)")
      uftrace graph
      ;;
    "uftrace tui(test)")
      uftrace tui
      ;;
    "lttng")
      ;;
    "gprof")
      ;;
    "ldd")
      ls build/*.bin build/obj/* | fzf --header="objdump: " | xargs ldd | less -N
      ;;
    "objdump")
      ls build/*.bin build/obj/* | fzf --header="objdump: " | xargs objdump -x -d -p -f -a -t -r | less -N
      ;;
    "strings")
      ls build/debug/* | fzf --header="strings: " | xargs strings | less -N
      ;;
    "nm")
      ls build/debug/* | fzf --header="nm: " | xargs nm -l -n --synthetic | less -N
      ;;
    "readelf")
      ls build/*.bin build/obj/* | fzf --header="readelf: " | xargs readelf -a -h -g -t -s -d | less -N
      ;;
    "addr2line")
      file=$(ls build/*.bin build/obj/* | fzf --header="Select ELF file for addr2line")
      echo "Enter the memory address:"; read -r addr;
      addr2line -f -a -e $file $addr
      ;;
    "size")
      ls build/*.bin build/obj/* | fzf --header="size: " | xargs size --common -t -d -A | less -N
      ;;
    "file")
      ls build/*.* build/obj/* | fzf --header="file type: " | xargs file
      ;;
    "xxd")
      ls build/*.bin build/*.iso build/obj/* unit_test/build/*.o | fzf --header="xxd: " | xargs xxd | less
      ;;
    "scc")
      scc -p -a -u -i c,h,md,cpp,c++,hpp,txt,json,s,ld
      ;;
    "search")
      read -p "keyword: " p_keyword; rg "$p_keyword" ;;
    "search/replace")
      read -p "to_search: " to_search
      read -p "to_replace: " to_replace
      ambr "$to_search" "$to_replace" ;;
    "elfedit")
      ;;
    "strip")
      ;;
    "objcopy")
      ;;
    "ranlib")
      ;;
    "gcc-ar")
      ;;
    "gcc-nm")
      ;;
    "gcc-ranlib")
      ;;
    "gcov")
      ;;
    "gcov-dump")
      ;;
    "gcov-tool")
      ;;
    "gprof")
      ;;
    "gprofng")
      ;;
    "gprofng-archive")
      ;;
    "gprofng-collect-app")
      ;;
    "gprofng-display-html")
      ;;
    "gprofng-display-src")
      ;;
    "gprofng-display-text")
     ;;
    *) ;;
  esac
}

if [ $# -eq 0 ]; then
  menu
fi

"$@"
