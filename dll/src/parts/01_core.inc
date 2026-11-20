// parts/01_core.inc - globals, logging, the real GLU32 forwarding, memory and patch helpers. Part of main.cpp (one translation unit, included in order): it uses everything defined in the parts before it.
// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static HMODULE gSelf = nullptr;
static uintptr_t gExe = 0;
static bool gBuildOK = false;
static std::string gDir, gIni, gLogPath;
static IsoConfig gCfg;
static FILE* gLog = nullptr;
static std::mutex gLogMx;

// 1.9.122 (user: remove unnecessary logging): diagnostic lines - those starting with an ALL-CAPS tag (EQUIPQ, DOORSHIFT, PRISSTAT...) - and
// the game's per-model load chatter are written only with the diagLines setting on. Kept always: the start-up / status sentences, PERF
// (frame rate), EXCEPTION / ERROR / WARN, and the game's own messages.
static bool LogIsDiag(const char* s) {
    static const char* kNoise[] = {"GAME: Parsed sprite", "GAME: Failed to find matching sprite", "GAME: Saving Preferences", "GAME: Loading user sprite"};
    for (const char* n : kNoise)
        if (strncmp(s, n, strlen(n)) == 0) return true;
    int n = 0;
    while ((s[n] >= 'A' && s[n] <= 'Z') || (s[n] >= '0' && s[n] <= '9') || s[n] == '_') n++;
    if (n < 3 || (s[n] != ' ' && s[n] != ':' && s[n] != 0)) return false;      // a sentence, not a tag
    static const char* kKeep[] = {"PERF", "EXCEPTION", "ERROR", "WARN", "GAME", "PA3AID"};
    for (const char* k : kKeep)
        if ((int)strlen(k) == n && strncmp(s, k, n) == 0) return false;
    return true;
}
static void Log(const char* fmt, ...) {
    char buf[4096];
    va_list a;
    va_start(a, fmt);
    vsnprintf(buf, sizeof buf, fmt, a);
    va_end(a);
    if (!gCfg.diagLines && LogIsDiag(buf)) return;
    std::lock_guard<std::mutex> lk(gLogMx);
    if (!gLog) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(gLog, "[%02d:%02d:%02d.%03d] %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, buf);
    fflush(gLog);
}

// ---------------------------------------------------------------------------
// Real GLU32
// ---------------------------------------------------------------------------
typedef void(APIENTRY* PFN_gluPerspective)(GLdouble, GLdouble, GLdouble, GLdouble);
typedef void(APIENTRY* PFN_gluLookAt)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
typedef int(APIENTRY* PFN_gluUnProject)(GLdouble, GLdouble, GLdouble, const GLdouble*, const GLdouble*, const GLint*, GLdouble*, GLdouble*,
                                        GLdouble*);
typedef void(APIENTRY* PFN_gluOrtho2D)(GLdouble, GLdouble, GLdouble, GLdouble);
typedef int(APIENTRY* PFN_gluBuild2DMipmaps)(GLenum, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
typedef const GLubyte*(APIENTRY* PFN_gluErrorString)(GLenum);

static struct {
    HMODULE mod = nullptr;
    PFN_gluPerspective Perspective = nullptr;
    PFN_gluLookAt LookAt = nullptr;
    PFN_gluUnProject UnProject = nullptr;
    PFN_gluOrtho2D Ortho2D = nullptr;
    PFN_gluBuild2DMipmaps Build2DMipmaps = nullptr;
    PFN_gluErrorString ErrorString = nullptr;
} R;

static void LoadRealGlu() {
    if (R.mod) return;
    char sys[MAX_PATH];
    GetSystemDirectoryA(sys, MAX_PATH);
    std::string p = std::string(sys) + "\\glu32.dll";
    R.mod = LoadLibraryA(p.c_str());
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&LoadRealGlu, &self);
    if (R.mod && R.mod == self) { R.mod = nullptr; }
    if (!R.mod) {
        Log("FATAL: could not load %s", p.c_str());
        return;
    }
    R.Perspective = (PFN_gluPerspective)GetProcAddress(R.mod, "gluPerspective");
    R.LookAt = (PFN_gluLookAt)GetProcAddress(R.mod, "gluLookAt");
    R.UnProject = (PFN_gluUnProject)GetProcAddress(R.mod, "gluUnProject");
    R.Ortho2D = (PFN_gluOrtho2D)GetProcAddress(R.mod, "gluOrtho2D");
    R.Build2DMipmaps = (PFN_gluBuild2DMipmaps)GetProcAddress(R.mod, "gluBuild2DMipmaps");
    R.ErrorString = (PFN_gluErrorString)GetProcAddress(R.mod, "gluErrorString");
}

// Generic pass-through for the GLU functions we don't intercept (see glu_forward.S).
// opengl32.dll itself imports from glu32.dll, so every GLU export must exist.
#include "glu_forward_names.h"
static void* gFwd[sizeof(kFwdNames) / sizeof(kFwdNames[0])];
static intptr_t ReturnZero() { return 0; }
extern "C" void* paiso_resolve_fwd(int idx) {
    if (idx < 0 || idx >= (int)(sizeof(kFwdNames) / sizeof(kFwdNames[0]))) return (void*)&ReturnZero;
    if (!gFwd[idx]) {
        LoadRealGlu();
        void* p = R.mod ? (void*)GetProcAddress(R.mod, kFwdNames[idx]) : nullptr;
        gFwd[idx] = p ? p : (void*)&ReturnZero;
    }
    return gFwd[idx];
}

// ---------------------------------------------------------------------------
// Memory helpers
// ---------------------------------------------------------------------------
static uintptr_t gRdB = 0, gRdE = 0;      // last region that VirtualQuery confirmed readable; reset every frame
static uintptr_t gRdCacheB[32], gRdCacheE[32];
static int gRdCacheN = 0, gRdCacheNext = 0;      // more readable regions remembered (cleared each frame)
static bool Readable(const void* p, size_t n) {
    uintptr_t a = (uintptr_t)p;
    if (a >= gRdB && a + n <= gRdE && gRdE) return true;
    for (int i = 0; i < gRdCacheN; i++)
        if (a >= gRdCacheB[i] && a + n <= gRdCacheE[i]) {
            gRdB = gRdCacheB[i];
            gRdE = gRdCacheE[i];
            return true;
        }
    MEMORY_BASIC_INFORMATION mbi;
    if (!p || !VirtualQuery(p, &mbi, sizeof mbi)) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    bool ok = (uintptr_t)p + n <= (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    if (ok) {
        gRdB = (uintptr_t)mbi.BaseAddress;
        gRdE = gRdB + mbi.RegionSize;
        gRdCacheB[gRdCacheNext] = gRdB;
        gRdCacheE[gRdCacheNext] = gRdE;
        gRdCacheNext = (gRdCacheNext + 1) & 31;
        if (gRdCacheN < 32) gRdCacheN++;
    }
    return ok;
}
static unsigned gDrawTick = 1;      // bumped at the start of every draw call (arrays, elements, begin): per-draw caches key on it
// 1.9.125 (frame rate): the modelview is read back only after a matrix call may have changed it. The game's own matrix calls (glLoadIdentity,
// glMultMatrixf, glTranslatef, glScalef, glPopMatrix - all it imports) are hooked and bump gMvGen, ours bump it through the macros below, and
// the real GLU's (gluLookAt / gluPerspective / gluOrtho2D) in our exports. Before, every draw did a glGetDoublev: ~28k driver round trips a
// frame on a big prison. Every 1024th cache hit is checked against the driver; one mismatch turns the cache off for good (logged).
static unsigned gMvGen = 1;
static bool gMvHooked = false;
static int gMvMiss = 0;
static inline void GetMV(GLdouble* out) {
    static GLdouble c[16];
    static unsigned t = 0, g = 0, n = 0;
    if (!gMvHooked ? t != gDrawTick : g != gMvGen) {
        glGetDoublev(GL_MODELVIEW_MATRIX, c);
        g = gMvGen;
    } else if (t != gDrawTick && (++n & 1023) == 0) {
        GLdouble r[16];
        glGetDoublev(GL_MODELVIEW_MATRIX, r);
        if (memcmp(r, c, sizeof c)) {
            memcpy(c, r, sizeof c);
            gMvHooked = false;
            gMvMiss = 1;
        }
    }
    t = gDrawTick;
    memcpy(out, c, sizeof c);
}
#define glLoadIdentity() (gMvGen++, glLoadIdentity())
#define glLoadMatrixd(m) (gMvGen++, glLoadMatrixd(m))
#define glLoadMatrixf(m) (gMvGen++, glLoadMatrixf(m))
#define glMultMatrixd(m) (gMvGen++, glMultMatrixd(m))
#define glMultMatrixf(m) (gMvGen++, glMultMatrixf(m))
#define glTranslated(x, y, z) (gMvGen++, glTranslated(x, y, z))
#define glTranslatef(x, y, z) (gMvGen++, glTranslatef(x, y, z))
#define glScaled(x, y, z) (gMvGen++, glScaled(x, y, z))
#define glScalef(x, y, z) (gMvGen++, glScalef(x, y, z))
#define glRotated(a, x, y, z) (gMvGen++, glRotated(a, x, y, z))
#define glRotatef(a, x, y, z) (gMvGen++, glRotatef(a, x, y, z))
#define glPopMatrix() (gMvGen++, glPopMatrix())
#define glOrtho(l, r, b, t, n, f) (gMvGen++, glOrtho(l, r, b, t, n, f))
// 1.9.86: the projection is read back only after it may have changed (glMatrixMode hook / our own camera bump gProjGen), not on every
// draw - a glGetDoublev per draw was thousands of driver round trips a frame on a big prison
static unsigned gProjGen = 1;
static GLenum gMatMode = GL_MODELVIEW;
static bool gProjHooked = false;
static inline void GetPR(GLdouble* out) {
    static GLdouble c[16];
    static unsigned t = 0, g = 0;
    if (gProjHooked ? (g != gProjGen || gMatMode == GL_PROJECTION) : (t != gDrawTick)) {
        glGetDoublev(GL_PROJECTION_MATRIX, c);
        t = gDrawTick;
        g = gProjGen;
    }
    memcpy(out, c, sizeof c);
}
// every glMatrixMode of our own also marks the projection as possibly changed (they do not go through the game's hooked import)
#define glMatrixMode(m) (gProjGen++, glMatrixMode(m))
struct ArrState {
    void* vp;
    GLint sz, ty, st, vbo;
    unsigned serial;
};
static unsigned gDrawSerial = 1;
// The vertex array state, tracked from the game's own calls so a draw needs no glGet round trips (falls back to asking GL if the hooks are missing)
static bool gVPHooked = false;
static const void* gVPPtr = nullptr;
static GLint gVPSize = 4, gVPType = 0x1406, gVPStride = 0, gArrayVbo = 0;
static const ArrState& GetArr() {
    static ArrState s = {nullptr, 0, 0, 0, 0, 0};
    if (s.serial == gDrawSerial) return s;
    if (gVPHooked) {
        s.serial = gDrawSerial;
        s.vp = (void*)gVPPtr;
        s.sz = gVPSize;
        s.ty = gVPType;
        s.st = gVPStride;
        s.vbo = gArrayVbo;
        return s;
    }
    s.serial = gDrawSerial;
    s.vp = nullptr;
    s.sz = s.ty = s.st = s.vbo = 0;
    glGetPointerv(0x808E, &s.vp);
    glGetIntegerv(0x807A, &s.sz);
    glGetIntegerv(0x807B, &s.ty);
    glGetIntegerv(0x807C, &s.st);
    glGetIntegerv(0x8894, &s.vbo);
    return s;
}
static bool CheckSig(uintptr_t rva, const unsigned char* b, size_t n) {
    return Readable((void*)(gExe + rva), n) && memcmp((void*)(gExe + rva), b, n) == 0;
}
static uint8_t* App() {
    if (!gBuildOK) return nullptr;
    uintptr_t* pp = (uintptr_t*)(gExe + pa::kRva_gApp);
    if (!Readable(pp, 8)) return nullptr;
    uint8_t* app = (uint8_t*)*pp;
    if (!Readable(app, 0x2000)) return nullptr;
    return app;
}

static bool PatchIAT(HMODULE mod, const char* dll, const char* fn, void* hook, void** orig) {
    auto base = (uint8_t*)mod;
    auto dos = (IMAGE_DOS_HEADER*)base;
    auto nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return false;
    for (auto imp = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress); imp->Name; ++imp) {
        if (_stricmp((char*)(base + imp->Name), dll) != 0) continue;
        auto names = (IMAGE_THUNK_DATA*)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        auto iat = (IMAGE_THUNK_DATA*)(base + imp->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            auto ibn = (IMAGE_IMPORT_BY_NAME*)(base + names->u1.AddressOfData);
            if (strcmp((char*)ibn->Name, fn) != 0) continue;
            DWORD old;
            VirtualProtect(&iat->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            if (orig) *orig = (void*)iat->u1.Function;
            iat->u1.Function = (ULONG_PTR)hook;
            VirtualProtect(&iat->u1.Function, sizeof(void*), old, &old);
            return true;
        }
    }
    return false;
}
