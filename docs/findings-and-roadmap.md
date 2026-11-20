# PAIso – reverse-engineering findings & roadmap

Target: Prison Architect v1.02 r11056 (Steam build 1838), `Prison Architect64.exe`, PE timestamp `0x63f8b2ba`, image base `0x140000000`.

## Status (v0.5.4) – working in-game
- Isometric ortho camera, Q/E 45° rotation, grab-drag panning (middle/right button), F10 ImGui menu, F8 wall modes, F9 toggle.
- Cutaway: decided per wall piece at the piece's model origin (its tile centre). A piece drops if the tile one step behind it (away from camera) is indoors AND the tile one step in front is not inside a room. Room map = LUMINANCE_ALPHA texture (L = indoors, A = in a room) built from the cell grid every 0.5 s, bound to texture unit 7 during Render3DWalls.
- Cartoon look: cel shading + rim light on models (3dmode shader replaced), painted-block walls (face normal from screen derivatives).
- Model pack: cookie-cutter / table / tree / character-bust archetypes + extended objects-maps.png. Never model sprite #0 (Accountant) or #1 (Actor). Small items ~0.05 tiles tall.
- Launch flow: Steam starts the exe → exit(0) → Paradox launcher relaunches with `--launched-from-launcher`.

## Engine facts
- Legacy fixed-function OpenGL + GLSL 1.10 via OPENGL32.dll/GLU32.dll; extensions via wglGetProcAddress; floors in immediate mode.
- opengl32.dll imports from glu32.dll → proxy must export all 52 GLU functions.
- Data archives are solid RAR4.
- Hidden 3D renderer = debug flag "Psuedo3d" = app+0x8cd.
- Loose `data\shapes\*.obj` + `objects-maps.png` are loaded, matched to sprites by name. OBJ: 100 units/tile, Y up, centred, sprite top → −Z, sprite w/h cells × 50 units, UV v flipped.
- objects.spritebank = 4096² composite, 32 px cells: x<64,y<64 objects.png; y≥64 people.png; x≥64 special-entities.png.
- Model table has 871 slots (base bank only): `cmp edi,0x366` at 0x1806c2. DLC sprites can't get models; the remap patch crashed at 0x182a46 (disabled).
- Walls: Render3DWalls with 3dmode shader; model space Y up, per-piece modelview; world position via inverse view matrix uniform.
- LightMap `outdoors` sampler only bound during weather.

## Key addresses (RVA)
| What | RVA |
|---|---|
| g_app pointer | 0xd57900 |
| Camera::Apply (gluLookAt) / ret | 0x17c2f0 / 0x17c3ef |
| Camera::ScreenToWorldRay | 0x17c450 |
| Renderer vtable (+0x28 Ortho2D, +0x30 Perspective) | 0xab6360 |
| Render3DWalls / Render3DObjects | 0x181780 / 0x181f60 |
| Load3DAssets | 0x180560 |
| Game debug logger | 0xf26b0 |
| Cell class ctor (Mat +0x18, Ind +0x30, Room.i +0x50) | 0x7b1410 |

App: +0x198 World* · +0x8cd 3D flag · +0xc68 view (+0x24/+0x28 centre, +0x2c units/px) · +0x1c90 3D camera.
World: +0x60 width, +0x64 height, +0x48e8 cells* — stride 0xe0, index y*w+x, Indoors bool +0x30, room id int32 +0x50 (−1 none).

## Roadmap
1. DLC objects in 3D: draw them ourselves (world object list + positions).
2. Hand-built models: doors, prisoner/staff restyle, vehicles.
3. Culling at screen corners when rotated.
4. Character animation.
