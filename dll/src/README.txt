PA3aiD DLL source map (build: ..\build.sh; one translation unit - main.cpp includes the parts in order, each part uses what the earlier ones define)

main.cpp                 includes, the parts, Startup, DllMain
parts\01_core.inc        globals, logging, the real GLU32 forwarding, memory and patch helpers (Readable, GetArr, App, PatchIAT)
parts\02_camera.inc      camera state, rotate / scroll input, the isometric camera (ApplyIsoCamera)
parts\03_glu_exports.inc the GLU functions the game imports
parts\04_gl_hooks.inc    shader interception (our iso shaders replace the game's), per-program uniform locations (Locs), texture hooks
parts\05_walls.inc       indoor map, cell / material dumps, wall pieces and wall art
parts\06_objects.inc     per-draw object work: hiding, lying people, bubbles, doors, wall snap, wall fittings, overlays, culling, small sprites
parts\07_draw.inc        prisoner head overlay, glDrawArrays (the main per-draw pipeline) / immediate mode / FBO / buffer hooks, wglGetProcAddress
parts\08_input_menu.inc  hotkeys, window procedure, the ImGui mod menu
parts\09_frame.inc       per-frame work in SwapBuffers, the extra-model reload, PERF / PRISSTAT lines
parts\10_crash_diag.inc  diagnostics: exceptions, message boxes, process exit, debug output
parts\11_own_floor.inc   the 3D floor drawn from the game's own 2D floor art (CaptureTerrain, DrawOwnFloor)
parts\12_rooms.inc       room texture, the Render3DWalls hook, inline hooks
parts\13_models_people.inc extra model slots (sprites beyond 871), sprite ids, paiso_note (what object is being drawn: prisoner colours,
                         Bio, skin, hair, head), the DLC model install
shaders.h                the iso vertex / fragment shaders (uniform tint, skin / hair, head overlay, walls, cutaway)
dlc_models.S             assembly stubs in the game's model-table code paths (call paiso_note / paiso_def_to_id)
config.h / .cpp          settings (PA3aiD.ini); diagLog = one-off diagnostic dumps, verboseLog = the heavy draw survey
game_offsets.h, *_ids.h  game addresses, sprite ids, entity names
heads.h                  hair colour per prisoner head sprite (made by model-generator\pipeline\heads.py)
retired\                 code taken out (kept for reference);  backups\  older copies of files
