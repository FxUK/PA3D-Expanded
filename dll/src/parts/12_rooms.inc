// parts/12_rooms.inc - room texture, the Render3DWalls hook, inline hooks. Part of main.cpp (one translation unit, included in order): it uses everything defined in the parts before it.
// ---------------------------------------------------------------------------
// (gRoomTex is declared with gRoomW / gRoomBuf in 06_objects.inc: the own floor (part 11) samples it too)

static double gRoomNext = 0;

static bool UpdateRoomTexture() {
    uint8_t* app = App();
    if (!app) return false;
    uint8_t* world = *(uint8_t**)(app + 0x198);
    if (!Readable(world, 0x48f0)) return false;
    int w = *(int*)(world + 0x60), h = *(int*)(world + 0x64);
    uint8_t* cells = *(uint8_t**)(world + 0x48e8);
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || !Readable(cells, (size_t)w * h * 0xe0)) return false;
    double now = NowSec();
    if (gRoomTex && w == gRoomW && h == gRoomH && now < gRoomNext) return true;
    gRoomNext = now + 0.5;
    // 2 bytes per tile: L = indoors, A = inside a room (cell+0x50 room id != -1; corridors have none)
    gRoomBuf.resize((size_t)w * h * 2);
    static std::vector<signed char> memo;      // per room index: -1 unknown, 0 / 1 = cell?
    memo.assign(4096, -1);
    auto cellMemo = [&](int r) -> bool { if (r < 0) return false; if (r >= (int)memo.size()) return RoomIsCell(r); if (memo[r] < 0) memo[r] = RoomIsCell(r) ? 1 : 0; return memo[r] == 1; };
    int inRoom = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const uint8_t* c = cells + ((size_t)y * w + x) * 0xe0;
            bool ind = c[0x30] != 0;
            bool room = *(const int32_t*)(c + 0x50) >= 0;
            int rid = *(const int32_t*)(c + 0x54);      // the room's index (RoomIsCell)
            inRoom += room;
            gRoomBuf[((size_t)y * w + x) * 2 + 0] = ind ? 255 : 0;
            gRoomBuf[((size_t)y * w + x) * 2 + 1] = room ? (cellMemo(rid) ? 255 : 200) : 0;      // 255 = a cell (lights off at Sleep), 200 = any other room
        }
    {      // a big sudden change may be the game half-way through rebuilding its rooms: only accept it if a second read 0.15 s later agrees
        static int accepted = -1;
        static bool pending = false;
        if (gRoomTex && accepted >= 0 && abs(inRoom - accepted) > std::max(20, accepted / 50) && !pending) {
            pending = true;
            gRoomNext = now + 0.15;
            static int hl = 0;
            if (hl < 100) {
                hl++;
                Log("ROOMMAP big change held for a second read (%d -> %d)", accepted, inRoom);
            }
            return true;
        }
        pending = false;
        accepted = inRoom;
    }
    GLint prevUnpack = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevUnpack);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (!gRoomTex) glGenTextures(1, &gRoomTex);
    oBindTexture(GL_TEXTURE_2D, gRoomTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, w, h, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, gRoomBuf.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, prevUnpack);
    if (w != gRoomW || h != gRoomH) Log("Room map from cell grid: %dx%d (%d tiles inside rooms)", w, h, inRoom);
    {
        static int lastIn = -1, lastInd = -1, lines = 0;
        int ind = 0;
        for (size_t q = 0; q < gRoomBuf.size(); q += 2)
            ind += gRoomBuf[q] != 0;      // flicker diagnostic: does the room / indoors map change on a timer?
        if ((inRoom != lastIn || ind != lastInd) && lines < 200) {
            lines++;
            Log("ROOMMAP inRoom %d (was %d) indoors %d (was %d)", inRoom, lastIn, ind, lastInd);
        }
        lastIn = inRoom;
        lastInd = ind;
    }
    gRoomW = w;
    gRoomH = h;
    return true;
}

// 1.9.130 wall batch: everything a recorded wall pass depends on - the cell materials (walls built / demolished), doors and shop fronts
// seen, the learned wall materials and the wall settings. When it changes, the batch is rebuilt.
// 1.9.131 (1.9.130 log: rebuilt every 2-4 s): split in two. Hard = the cell materials and the wall settings: rebuilt at once. Soft = door,
// shop-front tiles and learned wall materials, which grow as the camera meets doors (and an opening door slides into new tiles): at most
// every 15 s.
static unsigned long long WbSigSoft() {
    unsigned long long h = 1469598103934665603ull;
    auto mix = [&](unsigned long long v) { h = (h ^ v) * 1099511628211ull; };
    unsigned long long ds = 0, dx = 0;
    for (auto& kv : gDoorTiles) { ds += (unsigned long long)(unsigned)kv.first * 2654435761ull; dx ^= (unsigned long long)(unsigned)kv.first * 40503ull; }
    mix(gDoorTiles.size()); mix(ds); mix(dx);
    unsigned long long ns = 0;
    for (int k : gNoWallEver) ns += (unsigned long long)(unsigned)k * 2654435761ull;
    mix(gNoWallEver.size()); mix(ns);
    for (int i = 0; i < 512; i++) if (gWallMat[i]) mix((unsigned long long)i + 7777);
    return h;
}
static unsigned long long WbSig() {
    unsigned long long h = 1469598103934665603ull;
    auto mix = [&](unsigned long long v) { h = (h ^ v) * 1099511628211ull; };
    if (uint8_t* app = App()) {
        uint8_t* world = *(uint8_t**)(app + 0x198);
        if (Readable(world, 0x48f0)) {
            int w = *(int*)(world + 0x60), hh = *(int*)(world + 0x64);
            uint8_t* cells = *(uint8_t**)(world + 0x48e8);
            if (w > 0 && hh > 0 && w <= 4096 && hh <= 4096 && Readable(cells, (size_t)w * hh * 0xe0)) {
                mix((unsigned)w); mix((unsigned)hh);
                for (size_t i = 0, n = (size_t)w * hh; i < n; i++) mix((unsigned)*(const int32_t*)(cells + i * 0xe0 + 0x18));
            }
        }
    }
    float cf[3] = {gCfg.wallLift, gCfg.wallThick, (float)gCfg.upSign};
    for (float f : cf) { unsigned u; memcpy(&u, &f, 4); mix(u); }
    mix((gCfg.wallAutoGround ? 1 : 0) | (gCfg.redrawMissingWalls ? 2 : 0));
    mix((unsigned long long)gGfxReloads);
    return h;
}
// 0 = the game's own wall pass, 1 = the game's pass, recorded, 2 = the batch instead of the game's pass
static int WbDecide() {
    static int seenReloads = 0;
    if (!gCfg.wallBatch || !gCam.active || gMenuWorld || !WorldLoaded() || gGfxReload || seenReloads != gGfxReloads) {
        seenReloads = gGfxReloads;
        if (gWb.state) { WbFree(); gWb.state = 0; }
        return 0;
    }
    if (gWallFrames < 60) return 0;
    {      // only the main view's pass (the game also draws the world into another frame buffer with its own camera)
        GLint vp[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_VIEWPORT, vp);
        if (!IsMainViewport(vp[2], vp[3]) || gCurFbo != gCamFbo) return 0;      // gCamFbo: the buffer the 3D camera's draws go to (06 outline)
        if (gWb.state == 2 && gCurFbo != gWb.fbo) return 0;
    }
    static int lastCheck = -1000;
    if ((gWb.state == 2 || gWb.state == 5) && gWallFrames - lastCheck >= 30) {
        lastCheck = gWallFrames;
        const unsigned long long s = WbSig(), ss = WbSigSoft();
        const bool hard = s != (gWb.state == 2 ? gWb.sig : gWb.failSig);
        const bool soft = ss != (gWb.state == 2 ? gWb.sigSoft : gWb.failSigSoft) && NowSec() - gWb.builtAt >= 15.0;
        if (hard || soft) {
            if (gWb.state == 2) {
                static int rl = 0;
                if (rl < 20) { rl++; Log("Wall batch: %s - rebuilding", hard ? "walls or wall settings changed" : "doors / shop fronts seen since the last build"); }
            }
            WbFree();
            gWb.state = 0;
        }
    }
    switch (gWb.state) {
        case 0: gWb.state = 1; return 0;      // one ordinary pass first: it refreshes the list of wall cells the fillers are worked out from
        case 1: return 1;
        case 2: return 2;
        default: return 0;
    }
}
// Inline hook on Render3DWalls (RVA 0x181780) so shaders know when walls are drawn.
typedef void (*PFN_Render3DWalls)(void*);
static PFN_Render3DWalls oRender3DWalls = nullptr;
static void hkRender3DWalls(void* self) {
    const int wbAct = WbDecide();
    gWbWide = wbAct == 1;
    if (pActiveTexture && oBindTexture) {
        GLint prevTex = 0;
        pActiveTexture(kIsoUnit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
        if (UpdateRoomTexture()) gOutdoorsTex = gRoomTex;
        oBindTexture(GL_TEXTURE_2D, gOutdoorsTex ? gOutdoorsTex : (GLuint)prevTex);
        pActiveTexture(gActiveUnit);
    }
    gRendererSelf = self;
    BulbFrame();
    gWallFramesSeen++;
    gWallPassesFrame++;
    if (gCfg.ownFloor && !gFloorDone && gCam.active) {
        gFloorDone = true;
        DrawOwnFloor();
    }
    DumpMaterialHistogram();
    WallFrameBegin();
    {
        static int nl = 0;
        if (gCam.active && WorldLoaded() && nl < 12 && (gFboDiagLeft > 0 || (nl < 2 && gWallFramesSeen > 400))) {
            nl++;
            GlSnap s;
            TakeSnap(s);
            LogSnap(gFboDiagLeft > 0 ? "WALLPASS-extra-frame" : "WALLPASS-normal-frame", s);
        }
    }
    if (gCfg.fixExtraPass) RestoreCamMV("wall pass start");
    GLint pw9 = 0, pw10 = 0, pw11 = 0, pwAct = 0;
    bool wallArt = gCfg.wallArt && gCellTexA && gCellTexB && gTilesetTex && pActiveTexture;
    if (wallArt) {
        glGetIntegerv(GL_ACTIVE_TEXTURE, &pwAct);
        pActiveTexture(GL_TEXTURE0 + 9);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &pw9);
        oBindTexture(GL_TEXTURE_2D, gCellTexA);
        pActiveTexture(GL_TEXTURE0 + 10);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &pw10);
        oBindTexture(GL_TEXTURE_2D, gCellTexB);
        pActiveTexture(GL_TEXTURE0 + 11);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &pw11);
        oBindTexture(GL_TEXTURE_2D, gTilesetTex);
        pActiveTexture((GLenum)pwAct);
    }
    gInWallPass = true;
    RefreshAllIsoUniforms();
    {
        bool drawn = false;
        if (wbAct == 2) {
            const double t0 = gPerfOn ? PerfNow() : 0.0;
            drawn = WbDraw();
            if (gPerfOn) gPerf.t[4] += PerfNow() - t0;
            if (!drawn) { WbFree(); gWb.state = 5; gWb.failSig = gWb.sig; }
        }
        if (!drawn) {
            if (wbAct == 1) WbBegin();
            gWbCapture = wbAct == 1;
            oRender3DWalls(self);
            gWbCapture = false;
            if (wbAct == 1) {
                const unsigned long long s = WbSig(), ss = WbSigSoft();
                gWb.builtAt = NowSec();
                if (WbEnd()) { gWb.state = 2; gWb.sig = s; gWb.sigSoft = ss; }
                else { gWb.state = 5; gWb.failSig = s; gWb.failSigSoft = ss; }
            }
        }
    }
    gWbWide = false;
    gInWallPass = false;
    RefreshAllIsoUniforms();
    if (wallArt) {
        pActiveTexture(GL_TEXTURE0 + 9);
        oBindTexture(GL_TEXTURE_2D, (GLuint)pw9);
        pActiveTexture(GL_TEXTURE0 + 10);
        oBindTexture(GL_TEXTURE_2D, (GLuint)pw10);
        pActiveTexture(GL_TEXTURE0 + 11);
        oBindTexture(GL_TEXTURE_2D, (GLuint)pw11);
        pActiveTexture((GLenum)pwAct);
    }
}
static void* InlineHook14(uintptr_t rva, const unsigned char* sig, void* hook) {
    if (!CheckSig(rva, sig, 14)) return nullptr;
    uint8_t* target = (uint8_t*)(gExe + rva);
    uint8_t* tr = (uint8_t*)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tr) return nullptr;
    memcpy(tr, sig, 14);
    tr[14] = 0xFF;
    tr[15] = 0x25;
    *(uint32_t*)(tr + 16) = 0;
    *(uint64_t*)(tr + 20) = (uint64_t)(target + 14);
    DWORD old;
    VirtualProtect(target, 14, PAGE_EXECUTE_READWRITE, &old);
    target[0] = 0xFF;
    target[1] = 0x25;
    *(uint32_t*)(target + 2) = 0;
    *(uint64_t*)(target + 6) = (uint64_t)hook;
    VirtualProtect(target, 14, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, 14);
    return tr;
}
