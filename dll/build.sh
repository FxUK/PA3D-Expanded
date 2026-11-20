#!/bin/sh
# Cross-compiles PAIso (proxy glu32.dll) with llvm-mingw.
set -e
TC=${TC:-/root/tools/llvm-mingw-20250114-ucrt-ubuntu-20.04-x86_64/bin}
CXX="$TC/x86_64-w64-mingw32-g++"
I=third_party_imgui
mkdir -p build
$CXX -O2 -std=c++17 -shared -static -static-libgcc -static-libstdc++ \
  -DWIN32_LEAN_AND_MEAN -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS \
  -I$I -I$I/backends -Isrc \
  src/main.cpp src/config.cpp src/glu_forward.S src/dlc_models.S \
  $I/imgui.cpp $I/imgui_draw.cpp $I/imgui_tables.cpp $I/imgui_widgets.cpp \
  $I/backends/imgui_impl_win32.cpp $I/backends/imgui_impl_opengl2.cpp \
  src/glu32.def -o build/glu32.dll \
  -lopengl32 -lgdi32 -ldwmapi -limm32 -luser32 -Wl,--enable-stdcall-fixup -s
echo built build/glu32.dll
