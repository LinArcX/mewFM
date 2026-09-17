#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

CXXFLAGS="-std=c++17 -O2 -Isrc -Ithird_party/nanovg -Ithird_party/oui-blendish -DGL_GLEXT_PROTOTYPES"
CFLAGS="-O2 -Ithird_party/nanovg -Ithird_party/oui-blendish -DGL_GLEXT_PROTOTYPES"

echo ">>> compiling implementation TU (C)"
gcc $CFLAGS -fgnu89-inline -c ../third_party/impl.c -o ../build/impl.o
gcc $CFLAGS -c ../third_party/nanovg/nanovg.c -o ../build/nanovg.o

echo ">>> compiling sources (C++)"
g++ $CXXFLAGS -c ../src/main.cpp -o ../build/main.o
g++ $CXXFLAGS -c ../src/FileManager.cpp -o ../build/FileManager.o

echo ">>> linking"
g++ ../build/impl.o ../build/nanovg.o ../build/main.o ../build/FileManager.o \
  $(pkg-config --cflags --libs glfw3 gl) \
  -lm -o ../build/rah

echo ">>> done: build/rah"
