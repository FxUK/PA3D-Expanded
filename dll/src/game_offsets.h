// Prison Architect v1.02 r11056 (Steam build 1838), 64-bit
// All values are RVAs (offsets from the module base) found by disassembly.
#pragma once
#include <cstdint>

namespace pa {

// PE header TimeDateStamp of the build these offsets were taken from.
constexpr uint32_t kExeTimestamp = 0x63f8b2ba;

// Global pointer to the main app/world-view object.
constexpr uintptr_t kRva_gApp = 0xd57900;

// Return addresses (RVA of the instruction after the call) of the GLU calls.
constexpr uintptr_t kRva_Ret_Camera_gluLookAt   = 0x17c3ef; // Camera::Apply
constexpr uintptr_t kRva_Ret_Persp_gluPerspective = 0x13f49a; // RendererOpenGL::SetPerspective

// Byte signatures used to verify the build before touching memory.
struct Sig { uintptr_t rva; const char* bytes; int len; };
static const unsigned char kSigCameraCall[] = {0xFF,0x15,0x71,0x8D,0x8C,0x00};             // call [gluLookAt]
static const unsigned char kSigRender3D[]   = {0x80,0xBF,0x1D,0x02,0x00,0x00,0x00};        // cmp byte [rdi+0x21d],0
static const unsigned char kSigApp3D[]      = {0x41,0x80,0xBF,0xCD,0x08,0x00,0x00,0x00};   // cmp byte [r15+0x8cd],0
constexpr uintptr_t kRva_SigCameraCall = 0x17c3e9;
constexpr uintptr_t kRva_SigRender3D   = 0x45540d;
constexpr uintptr_t kRva_SigApp3D      = 0x185e76;

// Offsets inside the app object.
constexpr uintptr_t kApp_Pseudo3dFlag = 0x8cd;   // MapRenderer(app+0x6b0)+0x21d, "Psuedo3d" debug render flag
constexpr uintptr_t kApp_ViewRect     = 0xc68;   // view struct
constexpr uintptr_t kView_CenterX     = 0x24;    // float, world units (tiles)
constexpr uintptr_t kView_CenterY     = 0x28;    // float
constexpr uintptr_t kView_Scale       = 0x2c;    // float, world units per screen pixel
constexpr uintptr_t kApp_Camera3D     = 0x1c90;  // Camera: pos[3] @0, front[3] @0xc, up[3] @0x18,
                                                 //         GLint* viewport @0x30, GLdouble* mv @0x38, proj @0x40
} // namespace pa
