// parts/02_camera.inc - camera state, rotate / scroll input, the isometric camera. Part of main.cpp (one translation unit, included in order): it uses everything defined in the parts before it.
// ---------------------------------------------------------------------------
// Camera state
// ---------------------------------------------------------------------------
struct CamState {
    double yaw = 45.0, yawTarget = 45.0;      // degrees
    bool persp = false;
    double tanHalf = 0.36;        // perspective mode + tan(fov/2)
    double T[3] = {0, 0, 0};      // ground target
    double eye[3] = {0, 0, 0};
    double fwd[3] = {0, 0, 0}, right[3] = {0, 0, 0}, up[3] = {0, 0, 0};
    double halfH = 1, aspect = 1;
    float focus[2] = {0, 0};
    float camDir[2] = {0, 1};
    bool active = false;      // iso camera applied this frame
    int framesSinceApply = 1000;
    LARGE_INTEGER lastT{};
    // rotate-scroll bookkeeping
    bool haveLast = false;
    float lastCx = 0, lastCy = 0, lastScale = 0;
    // raw game values for diagnostics
    double gameEye[3] = {0, 0, 0}, gameCenter[3] = {0, 0, 0}, gameUp[3] = {0, 0, 0};
    int samplesLogged = 0;
} gCam;

static GLfloat gViewInv[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
// 1.9.122 (refactor): where a modelview's origin (the drawn model's own origin) lies on the map - this sum was written out by hand everywhere
static inline double MVWorldX(const GLdouble* m) { return gViewInv[0] * m[12] + gViewInv[4] * m[13] + gViewInv[8] * m[14] + gViewInv[12]; }
static inline double MVWorldY(const GLdouble* m) { return gViewInv[1] * m[12] + gViewInv[5] * m[13] + gViewInv[9] * m[14] + gViewInv[13]; }

static bool gObjPrisoner = false;
static uint32_t gPrisTint = 0;
static uint32_t gPrisSkin = 0, gPrisHair = 0;
struct EquipNote { unsigned serial; bool cuffs, baton, tazer, gun; const void* def; };
static EquipNote gEquip = {0, false, false, false, false, nullptr};
static uint8_t* gPrisObj = nullptr;      // the prisoner object paiso_note found for this draw (rsi or another register), or null      // what the person being drawn carries (paiso_note); drawn as small 3D items
static float gNight = 0.0f, gNightTarget = 0.0f;      // 0 day .. 1 full night, from the game's lightmap (07_draw NightTick); shaders isoNight
static int gVpW = 1920;      // the main viewport's width this frame (ApplyIsoCamera; CullOffscreen's tiny test)
static GLuint gBulbTex = 0;      // the lamp-pool map (07_draw BulbBuild), unit 14
static bool gBulbReady = false;      // the map belongs to the loaded world
static float gCellsDark = 0.0f;      // 0..1: cell rooms' lights off (Sleep regime), eased; shaders isoCellsDark
static const uint8_t* gPrisBio = nullptr;      // the Bio of the prisoner being drawn (diagnostics: names in the log)
static float gPrisScale = 1.0f;      // this prisoner's BodyScale (Bio +0x70): 2D draws prisoners bigger or smaller, so do the 3D bodies
static bool gDoorSplitOK = false; static float gDoorPos[2] = {0, 0}, gDoorOpen = 0;      // the double door being drawn (paiso_note): its frame position and how far open
static int gPrisHead =
    -1;      // gPrisHead: Bio HeadType of the prisoner being drawn (-1 = unknown)      // this prisoner's skin colour (Bio SkinColour) and hair colour (from its head sprite); 0x01RRGGBB, 0 = leave the model's own
#include "heads.h"
static unsigned gPrisStat[6] = {
    0};      // prisoner notes, coloured, object not found, colour table unreadable, tinted draws, draws on a program without the tint uniform (per 5 s)
static int gPrisLog = 0, gPrisBad = 0, gPrisVar = 0;      // log budgets, reset when a save loads
static GLdouble gCamMV[16], gCamP[16];
static int gFloorDrawsFrame = 0, gWallPassesFrame = 0;
static std::vector<uint16_t> gCellA, gCellB;
static std::vector<uint32_t> gCellStamp;
static int gCellW = 0, gCellH = 0;
static bool gCellDirty = false;
static GLuint gCellTexA = 0, gCellTexB = 0;              // per-cell 2D-art mapping textures (floor and wall colours)
static GLuint gTilesetTex = 0;                           // GL id of the game's 4608x4608 tileset (all terrain draws sample it)
static bool gFloorSeen = false, gFloorDone = false;      // the 3D camera's matrices, saved each frame (used to project speech bubbles)
static int gDlcLoaded = 0;
extern "C" void* paiso_tbl[16384];
static HWND gWnd = nullptr;
// Is this viewport the game window's main view? Usually it equals the client area, but a window set bigger than the screen (3840 x 2160 windowed on a
// 4K monitor) is squeezed by Windows (client 3822 x 2113) while the game still renders 3840 x 2160. Off-screen passes are smaller or square.
static bool IsMainViewport(int w, int h) {
    RECT rc = {0, 0, 0, 0};
    if (!gWnd || !GetClientRect(gWnd, &rc)) return true;
    int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
    if (abs(w - cw) <= 2 && abs(h - ch) <= 2) return true;
    if (cw <= 0 || ch <= 0 || h <= 0) return false;
    // 1.9.129 (user: walls reverted - the game's own camera was used): with the game's Supersampling option on, the main view is drawn at
    // twice the window size (7680 x 4320 on a 4K screen) and scaled down; up to 2.6 x the window, same shape, is the main view
    return w >= cw - 2 && h >= ch - 2 && w <= cw * 26 / 10 && h <= ch * 26 / 10 && fabs((double)w / h - (double)cw / ch) < 0.06;
}
static WNDPROC gOrigWndProc = nullptr;
static bool gMenuOpen = false;
static bool gImguiReady = false;
static bool gWeForced3D = false;
static char gToast[128] = "";
static double gToastUntil = 0;

static double NowSec() {
    static LARGE_INTEGER f = [] {
        LARGE_INTEGER x;
        QueryPerformanceFrequency(&x);
        return x;
    }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}
static void Toast(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    vsnprintf(gToast, sizeof gToast, fmt, a);
    va_end(a);
    gToastUntil = NowSec() + 1.6;
}

// Layer test: hide each texture the game draws on unit 0 in turn, to identify the 2D wall strips / steam.
static std::set<int> gSeenTex;
static std::vector<int> gLayerList;
static bool gLayerTest = false;
static int gLayerIdx = -1;
static double gLayerNext = 0;
static int gHideBeforeTest = 0;
static void LayerTestTick() {
    if (!gLayerTest) return;
    double n = NowSec();
    if (n < gLayerNext) return;
    gLayerNext = n + 4.0;
    if (gLayerList.empty()) {
        if (gSeenTex.empty()) {
            gLayerNext = n + 0.5;
            return;
        }      // nothing collected yet: wait for some draws
        gLayerList.assign(gSeenTex.begin(), gSeenTex.end());
    }
    gLayerIdx++;
    if (gLayerIdx >= (int)gLayerList.size()) {
        gLayerTest = false;
        gCfg.hideTexA = gHideBeforeTest;
        Toast("Layer test finished");
        return;
    }
    gCfg.hideTexA = gLayerList[gLayerIdx];
    Toast("Hiding texture %d  (%d of %d)", gCfg.hideTexA, gLayerIdx + 1, (int)gLayerList.size());
    gToastUntil = n + 3.8;
}
static const char* WallModeName(int m) { return m == 0 ? "Walls Up" : m == 1 ? "Walls Cutaway" : "Walls Down"; }
// The four wall views the on-screen button (and the walls key) step through (user 8 Nov; the fade is gone, it did not work well):
// Walls Up, Cutaway Far (walls in the near 75 % of the screen at half height, the rest full), Cutaway Close (the near half of the
// screen at half height, the far half full), Walls Down (every wall cut to downStub). The cut height is stub (0.5).
static int WallViewIndex() {
    if (gCfg.wallMode == 0) return 0;
    if (gCfg.wallMode == 2) return 4;
    if (gCfg.cutScreen >= 1.5f) return 3;
    return gCfg.cutScreen >= 0.25f ? 1 : 2;
}
static const int kWallViews = 5;      // Up, Cutaway Far, Cutaway Close, Cutaway All (every wall at half height), Down
static const char* WallViewName(int v) {
    static const char* n[] = {"Walls Up", "Cutaway Far", "Cutaway Close", "Cutaway All", "Walls Down"};
    return n[v < 0 || v >= kWallViews ? 0 : v];
}
static void SetWallView(int v) {
    static const int md[] = {0, 1, 1, 1, 2};
    if (v < 0 || v >= kWallViews) v = 0;
    gCfg.wallMode = md[v];
    gCfg.cutStyle = 0;
    if (v == 1) gCfg.cutScreen = 0.5f;      // clip-space y: the lower 75 % of the screen
    if (v == 2) gCfg.cutScreen = 0.0f;      // the lower half
    if (v == 3) gCfg.cutScreen = 2.0f;      // the whole screen
}

static void Normalize(double* v) {
    double l = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 1e-12) {
        v[0] /= l;
        v[1] /= l;
        v[2] /= l;
    }
}
static void Cross(const double* a, const double* b, double* o) {
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}

static bool Is3DActive() {
    uint8_t* app = App();
    return app && app[pa::kApp_Pseudo3dFlag] != 0;
}

// Re-map pan deltas (keyboard scroll, edge scroll, drag) into the rotated/tilted view.
// Ground offset (from the view centre) of the point under the mouse, using last frame's camera.
static bool CursorGroundOffset(double& ox, double& oy) {
    if (!gWnd || gCam.halfH <= 0) return false;
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(gWnd, &pt);
    RECT rc;
    GetClientRect(gWnd, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) return false;
    double nx = 2.0 * pt.x / rc.right - 1.0, ny = 1.0 - 2.0 * pt.y / rc.bottom;
    const double *r = gCam.right, *u = gCam.up, *f = gCam.fwd;
    if (fabs(f[2]) < 1e-6) return false;
    double H = gCam.halfH, A = gCam.aspect;
    if (gCam.persp) {
        double dx = f[0] + nx * gCam.tanHalf * A * r[0] + ny * gCam.tanHalf * u[0];
        double dy = f[1] + nx * gCam.tanHalf * A * r[1] + ny * gCam.tanHalf * u[1];
        double dz = f[2] + nx * gCam.tanHalf * A * r[2] + ny * gCam.tanHalf * u[2];
        if (fabs(dz) < 1e-6) return false;
        double t = -gCam.eye[2] / dz;
        ox = gCam.eye[0] + t * dx - gCam.T[0];
        oy = gCam.eye[1] + t * dy - gCam.T[1];
        return true;
    }
    double px = nx * H * A * r[0] + ny * H * u[0], py = nx * H * A * r[1] + ny * H * u[1];
    double pz = nx * H * A * r[2] + ny * H * u[2];
    double t = -pz / f[2];
    ox = px + t * f[0];
    oy = py + t * f[1];
    return true;
}
static bool gDragging = false;
static double gDragAnchor[2];
static double gKeyPanUntil = 0, gKeyDir[2] = {0, 0};      // movement-key panning: until when the game's glide is steered, and which way

static void RotateScrollInput() {
    uint8_t* app = App();
    if (!app) {
        gCam.haveLast = false;
        return;
    }
    float* cx = (float*)(app + pa::kApp_ViewRect + pa::kView_CenterX);
    float* cy = (float*)(app + pa::kApp_ViewRect + pa::kView_CenterY);
    float sc = *(float*)(app + pa::kApp_ViewRect + pa::kView_Scale);
    // Back from 2D (F9) or any gap without the 3D camera: the remembered centre is stale and would turn the first move into a jump.
    if (gCam.framesSinceApply > 2) {
        gCam.haveLast = false;
        gKeyPanUntil = 0;
        gDragging = false;
    }

    // Grab-drag: keep the floor point that was under the cursor at press time under the cursor.
    bool held = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    bool menuMouse = gMenuOpen && gImguiReady && ImGui::GetIO().WantCaptureMouse;
    bool fg = gWnd && GetForegroundWindow() == gWnd;
    double ox, oy;

    // Middle mouse: free rotate (drag left/right = turn, up/down = tilt). The view centre is held
    // still so the game's own middle-button pan does not slide the map while turning.
    static bool rotDrag = false;
    static POINT rotLast;
    static float rotCx = 0, rotCy = 0;
    if ((GetAsyncKeyState(VK_MBUTTON) & 0x8000) && fg && !menuMouse && gCfg.rotateScroll) {
        POINT p;
        GetCursorPos(&p);
        if (rotDrag) {
            gCam.yaw += (p.x - rotLast.x) * 0.3;
            gCam.yawTarget = gCam.yaw;
            double np = gCfg.pitchDeg + (p.y - rotLast.y) * 0.2;
            gCfg.pitchDeg = (float)(np < 10.0 ? 10.0 : (np > 89.0 ? 89.0 : np));      // lower limit enforced in ApplyIsoCamera
            *cx = rotCx;
            *cy = rotCy;
        } else {
            rotCx = *cx;
            rotCy = *cy;
        }
        rotDrag = true;
        rotLast = p;
        gCam.lastCx = *cx;
        gCam.lastCy = *cy;
        gCam.lastScale = sc;
        gCam.haveLast = true;
        return;
    }
    rotDrag = false;
    if (held && fg && !menuMouse && gCfg.rotateScroll && CursorGroundOffset(ox, oy)) {
        if (!gDragging) {
            gDragging = true;
            double bx = gCam.haveLast ? gCam.lastCx : *cx, by = gCam.haveLast ? gCam.lastCy : *cy;
            gDragAnchor[0] = bx + ox;
            gDragAnchor[1] = by + oy;
        }
        *cx = (float)(gDragAnchor[0] - ox);
        *cy = (float)(gDragAnchor[1] - oy);
        gCam.lastCx = *cx;
        gCam.lastCy = *cy;
        gCam.lastScale = sc;
        gCam.haveLast = true;
        return;
    }
    gDragging = false;
    // The game eases its zoom over many frames; a tiny scale change is still key-panning (map it), a big one is a zoom jump.
    bool zoomJump = gCam.haveLast && gCam.lastScale > 0 && fabs(sc - gCam.lastScale) / gCam.lastScale > 0.02;
    if (!gCfg.rotateScroll || !gCam.haveLast || zoomJump) {
        gCam.lastCx = *cx;
        gCam.lastCy = *cy;
        gCam.lastScale = sc;
        gCam.haveLast = true;
        return;
    }
    gCam.lastScale = sc;
    float dx = *cx - gCam.lastCx, dy = *cy - gCam.lastCy;
    if (dx == 0 && dy == 0) return;
    // Large jumps (minimap click, "go to" buttons) are absolute: leave them alone.
    {
        double viewW0 = sc * 1920.0;
        if (fabs(dx) > viewW0 * 0.25 || fabs(dy) > viewW0 * 0.25) {
            gCam.lastCx = *cx;
            gCam.lastCy = *cy;
            gKeyPanUntil = 0;
            return;
        }
    }
    // Movement keys (1.9.71): the direction comes from the keys held and this camera's facing, never from the game's own move (which
    // eases on after the keys are let go, and whose direction changed after F9 toggles). Only the game's speed is kept.
    {
        int kx = 0, ky = 0;
        if (fg) {
            if ((GetAsyncKeyState('D') | GetAsyncKeyState(VK_RIGHT)) & 0x8000) kx++;
            if ((GetAsyncKeyState('A') | GetAsyncKeyState(VK_LEFT)) & 0x8000) kx--;
            if ((GetAsyncKeyState('S') | GetAsyncKeyState(VK_DOWN)) & 0x8000) ky++;
            if ((GetAsyncKeyState('W') | GetAsyncKeyState(VK_UP)) & 0x8000) ky--;
        }
        double nowS = NowSec();
        if (kx || ky) {
            double l = sqrt((double)(kx * kx + ky * ky));
            gKeyDir[0] = kx / l;
            gKeyDir[1] = ky / l;
            gKeyPanUntil = nowS + 0.8;      // the game keeps gliding a little after release: steer that the same way
        }
        if (kx || ky || nowS < gKeyPanUntil) {
            double sp = sin(gCfg.pitchDeg * kPi / 180.0);
            if (sp < 0.2) sp = 0.2;
            double rx = gCam.right[0], ry = gCam.right[1], rl = sqrt(rx * rx + ry * ry);
            if (rl < 1e-6) {
                rx = 1;
                ry = 0;
            } else {
                rx /= rl;
                ry /= rl;
            }
            double wx = gKeyDir[0] * rx + (gKeyDir[1] / sp) * gCam.camDir[0];
            double wy = gKeyDir[0] * ry + (gKeyDir[1] / sp) * gCam.camDir[1];
            double mag = sqrt((double)dx * dx + (double)dy * dy) * gCfg.panSpeed;
            *cx = gCam.lastCx + (float)(wx * mag);
            *cy = gCam.lastCy + (float)(wy * mag);
            gCam.lastCx = *cx;
            gCam.lastCy = *cy;
            return;
        }
    }
    {      // only remap real panning (edge scroll here; keys above). Follow-cam, minimap and go-to moves are the game's own: leave them alone
        bool pan = false;
        if (gWnd) {
            POINT pt;
            RECT rc;
            if (GetCursorPos(&pt) && ScreenToClient(gWnd, &pt) && GetClientRect(gWnd, &rc) && fg &&
                (pt.x <= 2 || pt.y <= 2 || pt.x >= rc.right - 3 || pt.y >= rc.bottom - 3))
                pan = true;
        }
        if (!pan) {
            gCam.lastCx = *cx;
            gCam.lastCy = *cy;
            return;
        }
    }
    // Large jumps (minimap click, "go to" buttons) are absolute: leave them alone.
    double viewW = sc * 1920.0;
    if (fabs(dx) > viewW * 0.25 || fabs(dy) > viewW * 0.25) {
        gCam.lastCx = *cx;
        gCam.lastCy = *cy;
        return;
    }
    // screen-right / screen-down of the previous frame's camera, projected on the ground
    double sp = sin(gCfg.pitchDeg * kPi / 180.0);
    if (sp < 0.2) sp = 0.2;
    double rx = gCam.right[0], ry = gCam.right[1], rl = sqrt(rx * rx + ry * ry);
    if (rl < 1e-6) {
        rx = 1;
        ry = 0;
    } else {
        rx /= rl;
        ry /= rl;
    }
    double nx = (dx * rx + (dy / sp) * gCam.camDir[0]) * gCfg.panSpeed;
    double ny = (dx * ry + (dy / sp) * gCam.camDir[1]) * gCfg.panSpeed;
    *cx = gCam.lastCx + (float)nx;
    *cy = gCam.lastCy + (float)ny;
    gCam.lastCx = *cx;
    gCam.lastCy = *cy;
}

// The iso camera only makes sense while a prison is loaded; on the main menu (and loading screens) the game's own camera is used.
static bool gMenuWorld = false;
static bool WorldLoaded() {
    if (gMenuWorld && !gCfg.isoOnMenu) return false;      // the main menu runs a fly-over of a premade prison
    uint8_t* app = App();
    if (!app) return false;
    uint8_t* world = *(uint8_t**)(app + 0x198);
    if (!world || !Readable(world, 0x48f0)) return false;
    int w = *(int*)(world + 0x60), h = *(int*)(world + 0x64);
    uint8_t* cells = *(uint8_t**)(world + 0x48e8);
    return w > 8 && h > 8 && w <= 4096 && h <= 4096 && cells && Readable(cells, 0x100);
}
static void RefreshAllIsoUniforms();
static GLuint gCurFbo = 0;      // set by the glBindFramebuffer hooks
static void ApplyIsoCamera(double ex, double ey, double ez) {
    LayerTestTick();
    // timing / rotation animation
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    static LARGE_INTEGER freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f;
    }();
    double dt = gCam.lastT.QuadPart ? (double)(now.QuadPart - gCam.lastT.QuadPart) / freq.QuadPart : 0.016;
    gCam.lastT = now;
    if (dt > 0.25) dt = 0.25;
    double k = 1.0 - exp(-gCfg.rotSpeed * dt);
    gCam.yaw += (gCam.yawTarget - gCam.yaw) * k;
    if (fabs(gCam.yawTarget - gCam.yaw) < 0.01) gCam.yaw = gCam.yawTarget;

    RotateScrollInput();

    // target = current view centre on the ground
    double tx = ex, ty = ey;
    if (uint8_t* app = App()) {
        tx = *(float*)(app + pa::kApp_ViewRect + pa::kView_CenterX);
        ty = *(float*)(app + pa::kApp_ViewRect + pa::kView_CenterY);
    }
    double alt = fabs(ez);
    if (alt < 1e-3) alt = 1.0;

    GLint vp[4] = {0, 0, 1, 1};
    glGetIntegerv(GL_VIEWPORT, vp);
    double aspect = vp[3] > 0 ? (double)vp[2] / vp[3] : 1.0;
    if (vp[2] > 200) gVpW = vp[2];
    double halfH = alt * tan(30.0 * kPi / 180.0) * gCfg.zoom;

    // never tilt so low that the sky (nothing is drawn there) shows at the top of the screen
    double fovHalf = gCfg.fovDeg * 0.5 * kPi / 180.0;
    double minP = gCfg.minPitchDeg;
    if (gCfg.perspective && minP < gCfg.fovDeg * 0.5 + 6.0) minP = gCfg.fovDeg * 0.5 + 6.0;
    if (gCfg.pitchDeg < minP) gCfg.pitchDeg = (float)minP;
    if (gCfg.pitchDeg > 89.0f) gCfg.pitchDeg = 89.0f;

    double p = gCfg.pitchDeg * kPi / 180.0, y = gCam.yaw * kPi / 180.0;
    double ys = gCfg.flipY ? -1.0 : 1.0, us = gCfg.upSign;
    // perspective: place the eye so the ground at the view centre keeps the same on-screen scale as ortho
    double D = gCfg.perspective ? halfH / tan(fovHalf) : alt * 4.0 + 500.0;
    double dir[3] = {sin(y) * cos(p), ys * cos(y) * cos(p), us * sin(p)};      // target -> eye
    double eye[3] = {tx + D * dir[0], ty + D * dir[1], D * dir[2]};
    double upv[3] = {0, 0, us};
    gCam.persp = gCfg.perspective;
    gCam.tanHalf = tan(fovHalf);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (gCfg.perspective) {
        double zn = D * 0.04;
        if (zn < 0.5) zn = 0.5;
        double top = zn * tan(fovHalf);
        glFrustum(-top * aspect, top * aspect, -top, top, zn, D * 3.0 + 2000.0);
    } else
        glOrtho(-halfH * aspect, halfH * aspect, -halfH, halfH, 1.0, D * 2.0 + 1000.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    R.LookAt(eye[0], eye[1], eye[2], tx, ty, 0.0, upv[0], upv[1], upv[2]);
    glGetDoublev(GL_MODELVIEW_MATRIX, gCamMV);
    glGetDoublev(GL_PROJECTION_MATRIX, gCamP);
    gProjGen++;      // we changed the projection ourselves (not through the game's glMatrixMode)
    gFloorSeen = false;
    gFloorDone = false;
    {      // rigid inverse of the view matrix (column-major)
        GLdouble m[16];
        glGetDoublev(GL_MODELVIEW_MATRIX, m);
        double Rt[9] = {m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10]};      // rows of R^T
        double t[3] = {m[12], m[13], m[14]};
        // inv = [R^T | -R^T t]; column-major: inv[c*4+r]
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) gViewInv[c * 4 + r] = (GLfloat)m[r * 4 + c];
        for (int r = 0; r < 3; r++) gViewInv[12 + r] = (GLfloat) - (m[r * 4 + 0] * t[0] + m[r * 4 + 1] * t[1] + m[r * 4 + 2] * t[2]);
        gViewInv[3] = gViewInv[7] = gViewInv[11] = 0;
        gViewInv[15] = 1;
        (void)Rt;
    }

    // basis for focus picking
    double f[3] = {-dir[0], -dir[1], -dir[2]};
    double r[3];
    Cross(f, upv, r);
    Normalize(r);
    double u[3];
    Cross(r, f, u);
    Normalize(u);
    memcpy(gCam.fwd, f, sizeof f);
    memcpy(gCam.right, r, sizeof r);
    memcpy(gCam.up, u, sizeof u);
    gCam.T[0] = tx;
    gCam.T[1] = ty;
    gCam.T[2] = 0;
    memcpy(gCam.eye, eye, sizeof eye);
    gCam.halfH = halfH;
    gCam.aspect = aspect;

    double hx = dir[0], hy = dir[1], hl = sqrt(hx * hx + hy * hy);
    if (hl > 1e-6) {
        gCam.camDir[0] = (float)(hx / hl);
        gCam.camDir[1] = (float)(hy / hl);
    }

    // cutaway focus
    double nx = 0, ny = 0;
    if (gCfg.focusMode == 0 && gWnd) {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(gWnd, &pt);
        RECT rc;
        GetClientRect(gWnd, &rc);
        if (rc.right > 0 && rc.bottom > 0) {
            nx = 2.0 * pt.x / rc.right - 1.0;
            ny = 1.0 - 2.0 * pt.y / rc.bottom;
            if (nx < -1) nx = -1;
            if (nx > 1) nx = 1;
            if (ny < -1) ny = -1;
            if (ny > 1) ny = 1;
        }
    }
    double P[3];
    if (gCam.persp) {
        double d[3];
        for (int i = 0; i < 3; i++) d[i] = f[i] + nx * gCam.tanHalf * aspect * r[i] + ny * gCam.tanHalf * u[i];
        if (fabs(d[2]) > 1e-6) {
            double t = -eye[2] / d[2];
            gCam.focus[0] = (float)(eye[0] + t * d[0]);
            gCam.focus[1] = (float)(eye[1] + t * d[1]);
        } else {
            gCam.focus[0] = (float)tx;
            gCam.focus[1] = (float)ty;
        }
    } else {
        for (int i = 0; i < 3; i++) P[i] = gCam.T[i] + nx * halfH * aspect * r[i] + ny * halfH * u[i];
        if (fabs(f[2]) > 1e-6) {
            double t = -P[2] / f[2];
            gCam.focus[0] = (float)(P[0] + t * f[0]);
            gCam.focus[1] = (float)(P[1] + t * f[1]);
        } else {
            gCam.focus[0] = (float)tx;
            gCam.focus[1] = (float)ty;
        }
    }

    gCam.active = true;
    gCam.framesSinceApply = 0;
    RefreshAllIsoUniforms();
}
