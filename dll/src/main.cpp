// PA3aiD - isometric camera mod for Prison Architect v1.02 r11056 (64-bit)
//
// Loads as a proxy GLU32.dll from the game folder. It forwards the six GLU
// functions the game imports to the real system GLU32 and intercepts:
//   * gluLookAt from Camera::Apply      -> isometric orthographic camera + Q/E yaw
//   * wglGetProcAddress (exe IAT)       -> wraps glShaderSource/glAttachShader/glUseProgram
//                                          so the 3D-mode shaders are swapped for ours
//   * SwapBuffers (exe IAT)             -> Dear ImGui mod menu + toasts
//   * the game window's WndProc         -> hotkeys and menu input
// Mouse picking keeps working because the game un-projects the mouse through the
// matrices captured right after our gluLookAt.

#include <windows.h>
#include <GL/gl.h>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <array>
#include <vector>
#include <map>
#include <tuple>
#include <mutex>

#include "game_offsets.h"
#include "config.h"
#include "shaders.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl2.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

#ifndef GL_CURRENT_PROGRAM
#define GL_CURRENT_PROGRAM 0x8B8D
#endif
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE 0x84E0
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif

static const char* kVersion = "1.9.132";
static const double kPi = 3.14159265358979323846;

#include "parts/01_core.inc"
#include "parts/02_camera.inc"
#include "parts/03_glu_exports.inc"
#include "parts/04_gl_hooks.inc"
#include "parts/05_walls.inc"
#include "parts/06_objects.inc"
#include "parts/07_draw.inc"
#include "parts/08_input_menu.inc"
#include "parts/09_frame.inc"
#include "parts/10_crash_diag.inc"
#include "parts/11_own_floor.inc"
#include "parts/12_rooms.inc"
#include "parts/13_models_people.inc"
// ---------------------------------------------------------------------------
// Startup
// ---------------------------------------------------------------------------
static void Startup() {
    char path[MAX_PATH]; GetModuleFileNameA(nullptr, path, MAX_PATH);
    gDir = path; gDir = gDir.substr(0, gDir.find_last_of("\\/") + 1);
    CreateDirectoryA((gDir + "PA3D Expanded").c_str(), nullptr);
    gIni = gDir + "PA3D Expanded\\PA3D Expanded.ini";
    gLogPath = gDir + "PA3D Expanded\\PA3D Expanded.log";
    { WIN32_FILE_ATTRIBUTE_DATA fa; if (GetFileAttributesExA(gLogPath.c_str(), GetFileExInfoStandard, &fa) && (fa.nFileSizeHigh || fa.nFileSizeLow > 20u * 1024 * 1024))
        MoveFileExA(gLogPath.c_str(), (gLogPath + ".old").c_str(), MOVEFILE_REPLACE_EXISTING); }      // keep one previous log, never a 50 MB+ file
    gLog = fopen(gLogPath.c_str(), "a");
    Log("==================================================");
    Log("PA3D Expanded %s starting. PID %lu. Game: %s", kVersion, GetCurrentProcessId(), path);
    Log("Command line: %s", GetCommandLineA());

    if (GetFileAttributesA(gIni.c_str()) == INVALID_FILE_ATTRIBUTES) gCfg.Save(gIni);
    gCfg.Load(gIni);
    if (GetPrivateProfileIntA("PA3D Expanded", "cfgVersion", 1, gIni.c_str()) < 2) {
        IsoConfig def; gCfg.grit = def.grit; gCfg.style = def.style;
        gCfg.Save(gIni); WritePrivateProfileStringA("PA3D Expanded", "cfgVersion", "2", gIni.c_str());
        Log("Config migrated to v2 (cartoon defaults)");
    }
    gCam.yaw = gCam.yawTarget = gCfg.startYawDeg;

    gExe = (uintptr_t)GetModuleHandleA(nullptr);
    auto nt = (IMAGE_NT_HEADERS*)(gExe + ((IMAGE_DOS_HEADER*)gExe)->e_lfanew);
    uint32_t ts = nt->FileHeader.TimeDateStamp;
    bool s1 = CheckSig(pa::kRva_SigCameraCall, pa::kSigCameraCall, sizeof pa::kSigCameraCall);
    bool s2 = CheckSig(pa::kRva_SigRender3D, pa::kSigRender3D, sizeof pa::kSigRender3D);
    bool s3 = CheckSig(pa::kRva_SigApp3D, pa::kSigApp3D, sizeof pa::kSigApp3D);
    gBuildOK = s1 && s2 && s3;
    Log("Exe timestamp %08x (expected %08x), signatures camera=%d render3d=%d app3d=%d -> %s",
        ts, pa::kExeTimestamp, s1, s2, s3, gBuildOK ? "SUPPORTED" : "UNSUPPORTED (camera hooks off)");

    HMODULE exe = (HMODULE)gExe;
    bool a = PatchIAT(exe, "OPENGL32.dll", "wglGetProcAddress", (void*)hkWglGetProcAddress, (void**)&oWglGetProcAddress);
    bool b = PatchIAT(exe, "GDI32.dll", "SwapBuffers", (void*)hkSwapBuffers, (void**)&oSwapBuffers);
    Log("IAT hooks: wglGetProcAddress=%d SwapBuffers=%d", a, b);
    bool t1 = PatchIAT(exe, "OPENGL32.dll", "glBindTexture", (void*)hkBindTexture, (void**)&oBindTexture);
    bool t2 = PatchIAT(exe, "OPENGL32.dll", "glDrawArrays", (void*)hkDrawArrays, (void**)&oDrawArrays);
    bool t3 = PatchIAT(exe, "OPENGL32.dll", "glDrawElements", (void*)hkDrawElements, (void**)&oDrawElements);
    bool t4 = PatchIAT(exe, "OPENGL32.dll", "glBegin", (void*)hkBegin, (void**)&oBegin);
    bool t5 = PatchIAT(exe, "OPENGL32.dll", "glTexImage2D", (void*)hkTexImage2D, (void**)&oTexImage2D);
    gVPHooked = PatchIAT(exe, "OPENGL32.dll", "glVertexPointer", (void*)hkVertexPointer, (void**)&oVertexPointer); Log("IAT hooks: glVertexPointer=%d (vertex array state tracked without glGet)", (int)gVPHooked);
    { bool a1 = PatchIAT(exe, "OPENGL32.dll", "glTexSubImage2D", (void*)hkTexSubImage2D, (void**)&oTexSubImage2D); bool a2 = PatchIAT(exe, "OPENGL32.dll", "glCopyTexSubImage2D", (void*)hkCopyTexSubImage2D, (void**)&oCopyTexSubImage2D); Log("IAT hooks: glTexSubImage2D=%d glCopyTexSubImage2D=%d", (int)a1, (int)a2); }
    PatchIAT(exe, "OPENGL32.dll", "glEnd", (void*)hkEnd, (void**)&oEnd);
    gProjHooked = PatchIAT(exe, "OPENGL32.dll", "glMatrixMode", (void*)hkMatrixMode, (void**)&oMatrixMode);
    Log("IAT hooks: glMatrixMode=%d (projection read back only after it may have changed)", (int)gProjHooked);
    {
        bool m1 = PatchIAT(exe, "OPENGL32.dll", "glLoadIdentity", (void*)hkLoadIdentity, (void**)&oLoadIdentity);
        bool m2 = PatchIAT(exe, "OPENGL32.dll", "glPopMatrix", (void*)hkPopMatrix, (void**)&oPopMatrix);
        bool m3 = PatchIAT(exe, "OPENGL32.dll", "glMultMatrixf", (void*)hkMultMatrixf, (void**)&oMultMatrixf);
        bool m4 = PatchIAT(exe, "OPENGL32.dll", "glTranslatef", (void*)hkTranslatef, (void**)&oTranslatef);
        bool m5 = PatchIAT(exe, "OPENGL32.dll", "glScalef", (void*)hkScalef, (void**)&oScalef);
        gMvHooked = m1 && m2 && m3 && m4 && m5 && gCfg.mvCache;
        Log("IAT hooks: matrix calls %d%d%d%d%d (modelview read back only after it may have changed: %s)", (int)m1, (int)m2, (int)m3, (int)m4, (int)m5,
            gMvHooked ? "on" : "off");
    }
    Log("Texture tracking hooks: bind=%d drawArrays=%d drawElements=%d begin=%d texImage2D=%d", t1, t2, t3, t4, t5);
    bool c = PatchIAT(exe, "USER32.dll", "MessageBoxA", (void*)hkMessageBoxA, (void**)&oMessageBoxA);
    bool d = PatchIAT(exe, "KERNEL32.dll", "ExitProcess", (void*)hkExitProcess, (void**)&oExitProcess);
    bool e = PatchIAT(exe, "KERNEL32.dll", "OutputDebugStringA", (void*)hkODSA, (void**)&oODSA);
    bool f = PatchIAT(exe, "KERNEL32.dll", "OutputDebugStringW", (void*)hkODSW, (void**)&oODSW);
    bool g = gBuildOK && InstallDebugOutHook();
    static const unsigned char wallSig[14] = {0x48,0x8B,0xC4,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    if (gBuildOK) oRender3DWalls = (PFN_Render3DWalls)InlineHook14(0x181780, wallSig, (void*)&hkRender3DWalls);
    Log("Render3DWalls hook: %d", oRender3DWalls != nullptr);
    // DLC model patch disabled: crashes in the entity draw path and the game's name
    // lookup only finds ~1 DLC sprite anyway. Kept in source for later work.
    if (gBuildOK && gCfg.extraModels) InstallDlcModels();
    Log("Game debug output capture: A=%d W=%d inline=%d", e, f, g);
    AddVectoredExceptionHandler(1, DiagVEH);
    Log("Diag hooks: MessageBoxA=%d ExitProcess=%d VEH=on", c, d);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        gSelf = inst;
        DisableThreadLibraryCalls(inst);
        Startup();
    } else if (reason == DLL_PROCESS_DETACH) {
        if (gLog) { Log("Shutdown (DLL detach)"); fclose(gLog); gLog = nullptr; }
    }
    return TRUE;
}
