PA3aiD DLL tools (clang++ / clang-format from the LLVM-MinGW toolchain in C:\My Software\GA4 Extension Tool Lib)

glslcheck.exe - compiles PA3aiD's shaders on this PC's GL driver (hidden window) and prints the logs. Exit code 0 = compiled and linked.
  glslcheck.exe                      -> kIsoVS / kIsoFS from ..\src\shaders.h (BUILT IN: rebuild it after editing shaders.h, or it checks the old shaders)
  python -I extract.py ..\src\parts\11_own_floor.inc .   then   glslcheck.exe kFloorVS.glsl kFloorFS.glsl   -> the own-floor shader
  rebuild:  clang++ -O1 -static -I..\src glslcheck.cpp -o glslcheck.exe -lopengl32 -lgdi32 -luser32

pamem.py - reads the RUNNING game's memory (read-only) to find object layouts without a build / play cycle. Game running, a prison loaded:
  python -I pamem.py prisoners 10     prisoner objects (vtable exe+0xB09478): category, naked, where the Bio is, names
  python -I pamem.py bio 6            Bio objects (vtable exe+0xAC0508): names, head type, body type, scale, skin, raw bytes
  python -I pamem.py find-vt RVA 20   objects whose vtable is exe+RVA (hex)
  python -I pamem.py dump ADDR LEN    hex dump;   python -I pamem.py refs ADDR   who points at ADDR

pehash.py - hashes a DLL's .text / .rdata / .data (ignores the timestamp). A refactor that should change nothing must print IDENTICAL:
  python -I pehash.py ..\verify\before.dll ..\build\glu32.dll       (the build is deterministic: same source -> same sections)

..\.clang-format - the source style; format the parts with:
  clang-format -i --style=file --assume-filename=x.cpp src\parts\*.inc      (then pehash: formatting must not change the build)

Game memory found with pamem (6 Nov 2026), app = *(exe+0xD57900), world = *(app+0x198):
  world+0x80 TimeIndex (double, minutes) | world+0x5E8 int[24] schedule per prisoner category (1 = Sleep) | world+0x44A0 Room* by index
  room +0x34 index, +0x38 RoomType (1 Cell, 5 Dormitory, 7 Canteen, 8 Kitchen), +0x40 name | cell grid (world+0x48E8, 0xE0 per tile) +0x50 room uid, +0x54 room index
  prisoner (vtable exe+0xB09478) +0xA34 category, +0xA4C naked, +0xCB0 Bio (inline: +0x18 first name, +0x68 HeadType = head sprite index, +0x74 SkinColour RGBA)
  door +0x260/+0x264 OpenDir, +0x274 Open (0..1), +0x23C CloseTimer
  pamem.py regime / rooms: the live regime schedule and room types (what the cell lights use)
