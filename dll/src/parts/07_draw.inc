// parts/07_draw.inc - prisoner head overlay, glDrawArrays / immediate mode / FBO / buffer hooks, wglGetProcAddress. Part of main.cpp (one translation unit, included in order): it uses everything defined in the parts before it.
struct HeadGeo {
    int verts;
    float top, cx, cy, cz, r, halfX;      // halfX: the OBJ's half width (the game scales models differently across and up)
};
static const HeadGeo kHeadGeo[] = {{2286, 110.6f, -1.0f, 88.0f, 29.0f, 23.0f, 49.51f},
                                   {1692, 116.2f, 0.0f, 87.0f, 1.0f, 22.0f, 45.01f},
                                   {2988, 111.6f, 0.0f, 89.0f, 4.0f, 23.0f, 21.94f},
                                   {2130, 111.2f, 0.5f, 88.0f, -8.5f, 23.0f, 32.68f}};
static GLuint gHeadTex = 0;
static int gHeadTexState = 0;
static GLint gHeadPrevTex = 0;
static bool gHeadBound = false;
static bool HeadOverlaySet(GLint uHeads, GLint uGeo, GLint uCell, GLint first, GLsizei count) {
    const HeadGeo* hg = nullptr;
    for (const auto& h : kHeadGeo)
        if (h.verts == (int)count) hg = &h;
    if (!hg) return false;
    if (gHeadTexState == 0) {      // load the head atlas once
        gHeadTexState = -1;
        FILE* fp = fopen((gDir + "PA3D Expanded\\heads.rgba").c_str(), "rb");
        if (fp) {
            const int side = kHeadGrid * 32; std::vector<uint8_t> px((size_t)side * side * 4);
            size_t n = fread(px.data(), 1, px.size(), fp);
            fclose(fp);
            if (n == px.size()) {
                GLint act = 0, prev = 0;
                glGetIntegerv(GL_ACTIVE_TEXTURE, &act);
                pActiveTexture(GL_TEXTURE0 + 13);
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev);
                glGenTextures(1, &gHeadTex);
                oBindTexture(GL_TEXTURE_2D, gHeadTex);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, side, side, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                oBindTexture(GL_TEXTURE_2D, (GLuint)prev);
                pActiveTexture((GLenum)act);
                gHeadTexState = 1;
                Log("Head overlay: head sprites loaded (texture %u)", gHeadTex);
            }
        }
        if (gHeadTexState != 1) Log("Head overlay: PA3D Expanded\\heads.rgba missing or the wrong size - prisoners keep the body's own head");
    }
    if (gHeadTexState != 1) return false;
    // the model's real height in the vertex data gives the scale from OBJ units (the game may scale models when it loads them)
    static std::unordered_map<unsigned long long, float> topCache;
    const ArrState& AS = GetArr();
    if (!AS.vp || AS.vbo || AS.ty != 0x1406 || AS.sz < 3) return false;
    int stride = AS.st ? AS.st : AS.sz * 4;
    unsigned long long key = (unsigned long long)(uintptr_t)AS.vp * 31ull + (unsigned long long)first * 7ull + (unsigned long long)count;
    // measure the body in its real vertex data, once: the game loads models scaled (~1/220) and may flip front and back,
    // so the head's centre and size come from the top of the model itself, and the face side from comparing with the OBJ
    struct BodyM { float top, sv, sh; };      // model top; vertical and horizontal scale from OBJ units (the game scales them differently)
    static std::unordered_map<unsigned long long, BodyM> bodyCache;
    BodyM bm;
    auto it = bodyCache.find(key);
    if (it != bodyCache.end())
        bm = it->second;
    else {
        if (!Readable((uint8_t*)AS.vp + (size_t)first * stride, (size_t)count * stride)) return false;
        auto P = [&](int i) { return (const float*)((uint8_t*)AS.vp + (size_t)(first + i) * stride); };
        float top = -1e9f, half = 0.0f;
        for (int i = 0; i < count; i++) { top = std::max(top, P(i)[1]); half = std::max(half, fabsf(P(i)[0])); }
        bm.top = top; bm.sv = top / hg->top; bm.sh = half / hg->halfX;
        if (bodyCache.size() > 4000) bodyCache.clear();
        bodyCache[key] = bm;
        static int ml = 0;
        if (ml < 6) { ml++; Log("HEADOVL body %d verts: top %.3f, half width %.3f -> scale up %.5f, across %.5f; head at (%.3f, %.3f, %.3f) r %.3f / %.3f", (int)count, top, half, bm.sv, bm.sh, hg->cx * bm.sh, hg->cy * bm.sv, hg->cz * bm.sh, hg->r * bm.sh, hg->r * bm.sv); }
    }
    if (bm.top <= 0.0f || bm.sv <= 0.0f || bm.sh <= 0.0f) return false;
    int cell = (gPrisHead >= 0 && gPrisHead < (int)(sizeof kHeadCell / sizeof kHeadCell[0])) ? kHeadCell[gPrisHead] : -1;      // HeadType = the head sprite's index in objects.spritebank
    if (cell < 0) return false;
    GLint act = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &act);
    pActiveTexture(GL_TEXTURE0 + 13);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &gHeadPrevTex);
    oBindTexture(GL_TEXTURE_2D, gHeadTex);
    pActiveTexture((GLenum)act);
    pUniform1i(uHeads, 13);
    // the face looks along +z (measured in game, 1.9.44 log); xyz = head centre, w = radius across, cell.w = radius up (negative: bald)
    pUniform4f(uGeo, hg->cx * bm.sh, hg->cy * bm.sv, hg->cz * bm.sh, hg->r * bm.sh);
    float rv = hg->r * bm.sv;
    if (uCell >= 0) pUniform4f(uCell, (cell % kHeadGrid) / (float)kHeadGrid, (cell / kHeadGrid) / (float)kHeadGrid, 1.0f / kHeadGrid, kHeadHair[cell] ? rv : -rv);
    return true;
}
// A prisoner's own uniform colour / skin / hair / head stay set on the program until reset. Some draws are finished by other routines
// (lying people, cut fittings, wall snap) that return before the normal reset, and the next draws inherited them (brown discs on the
// walls, a yellow door): reset after the draw, at the start of the next draw, and before the program changes.
static GLuint gTintProg = 0;
static void ResetPrisonerUniforms() {
    if (gTintProg) {
        auto lt = gLocs.find(gTintProg);
        if (lt != gLocs.end() && gCurProg == gTintProg) {
            if (lt->second.utint >= 0) pUniform4f(lt->second.utint, 0, 0, 0, 0);
            if (lt->second.uskin >= 0) pUniform4f(lt->second.uskin, 0, 0, 0, 0);
            if (lt->second.uhair >= 0) pUniform4f(lt->second.uhair, 0, 0, 0, 0);
            if (lt->second.uheadgeo >= 0) pUniform4f(lt->second.uheadgeo, 0, 0, 0, 0);
        }
        gTintProg = 0;
    }
    if (gHeadBound) {
        gHeadBound = false;
        GLint act = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &act);
        pActiveTexture(GL_TEXTURE0 + 13);
        oBindTexture(GL_TEXTURE_2D, (GLuint)gHeadPrevTex);
        pActiveTexture((GLenum)act);
    }
}// Head swap: every stock prisoner body drawn with the stock head that matches the prisoner's own 2D head (bald, crest, lots of hair,
// short), from PA3aiD\heads3d\body<V>_head<k>.obj (model-generator\pipeline\headswap.py: the 4 bodies x the 4 heads). The game picks the
// body by body type; for the draw we point the vertex / normal / texcoord arrays at the combined mesh instead, then put them back.
struct SwapMesh { std::vector<float> pos, nrm, tex; int n = 0; bool scaled = false; float sx = 0, sy = 0; int topI = -1, halfI = -1; };
static std::map<int, SwapMesh> gSwap;      // key: body vertex count * 8 + head
struct BodyBox { float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f}; };
static std::map<int, BodyBox> gBodyBox;      // key: body vertex count
static int gSwapState = 0;
static std::string BioName(const uint8_t* s);      // 13_models_people
// GL error trace (1.9.60 broke drawing with GL_INVALID_OPERATION on every draw): who raised it
static void GLCHK(const char* where) {
    if (!gCfg.glErrTrace) return;      // glGetError on every draw costs frame time: only when tracing
    GLenum e = glGetError();
    if (!e) return;
    static std::map<std::string, int> n;
    int& k = n[where];
    if (k < 4) Log("GLERR 0x%x after %s (note %d serving %d prisoner %d)", (unsigned)e, where, (int)(gDrawSerial - gNoteSerial), (int)gObjServing, (int)gObjPrisoner);
    k++;
    while (glGetError()) {}
}

static int BodyOwnHead(int verts) { return verts == 2286 ? 0 : verts == 1692 ? 1 : verts == 2988 ? 2 : verts == 2130 ? 3 : -1; }
static void LoadHeadSwap() {
    gSwapState = -1;
    const int bodies[4] = {2286, 1692, 2988, 2130};
    int loaded = 0;
    for (int b : bodies)
        for (int h = 0; h < 4; h++) {
            char fn[96]; snprintf(fn, sizeof fn, "PA3D Expanded\\heads3d\\body%d_head%d.obj", b, h);
            FILE* fp = fopen((gDir + fn).c_str(), "r");
            if (!fp) continue;
            std::vector<float> V, T, N; SwapMesh m; char line[256];
            while (fgets(line, sizeof line, fp)) {
                float a, b2, c;
                if (line[0] == 'v' && line[1] == ' ' && sscanf(line + 2, "%f %f %f", &a, &b2, &c) == 3) { V.push_back(a); V.push_back(b2); V.push_back(c); }
                else if (line[0] == 'v' && line[1] == 't' && sscanf(line + 3, "%f %f", &a, &b2) == 2) { T.push_back(a); T.push_back(b2); }
                else if (line[0] == 'v' && line[1] == 'n' && sscanf(line + 3, "%f %f %f", &a, &b2, &c) == 3) { N.push_back(a); N.push_back(b2); N.push_back(c); }
                else if (line[0] == 'f') {
                    int vi[3], ti[3], ni[3];
                    if (sscanf(line + 2, "%d/%d/%d %d/%d/%d %d/%d/%d", &vi[0], &ti[0], &ni[0], &vi[1], &ti[1], &ni[1], &vi[2], &ti[2], &ni[2]) != 9) continue;
                    for (int k = 0; k < 3; k++) {
                        int v = vi[k] - 1, t = ti[k] - 1, nn = ni[k] - 1;
                        if (v < 0 || 3 * v + 2 >= (int)V.size() || t < 0 || 2 * t + 1 >= (int)T.size() || nn < 0 || 3 * nn + 2 >= (int)N.size()) continue;
                        for (int q = 0; q < 3; q++) { m.pos.push_back(V[3 * v + q]); m.nrm.push_back(N[3 * nn + q]); }
                        m.tex.push_back(T[2 * t]); m.tex.push_back(T[2 * t + 1]);
                    }
                }
            }
            fclose(fp);
            m.n = (int)m.pos.size() / 3;
            if (m.n > 0) { gSwap[b * 8 + h] = std::move(m); loaded++; }
        }
    for (int b : bodies) {      // each body's own extent in OBJ units, from its file with its own head (= the stock model)
        auto own = gSwap.find(b * 8 + BodyOwnHead(b));
        if (own == gSwap.end()) continue;
        BodyBox bx;
        for (int i = 0; i < own->second.n; i++)
            for (int a = 0; a < 3; a++) { float v = own->second.pos[3 * i + a]; bx.lo[a] = std::min(bx.lo[a], v); bx.hi[a] = std::max(bx.hi[a], v); }
        gBodyBox[b] = bx;
    }
    if (loaded) gSwapState = 1;
    Log("HEADSWAP %d body / head models loaded from PA3D Expanded\\heads3d", loaded);
}
// Things on furniture: the game draws food trays, meals and bowls on tables and sofas as flat 2D pictures at floor height (they have no
// 3D model), so in 3D they lay on the floor under the table. Each table / desk records its top height on the tiles it covers (sofas and
// benches their seat), and a small thing drawn on such a tile is lifted to that height.
static unsigned gLiftNote = 0, gRotNote = 0, gSurfNote = 0;      // the note whose (one) draw was lifted / turned / recorded
// gSurfCur / gSurfPrev (tile -> surface height) are declared in 06_objects: the loose items stand on them too
static int gSurfFrame = -1;
static void SurfFrameTick() {
    if (gSurfFrame == gWallFramesSeenExt) return;
    gSurfFrame = gWallFramesSeenExt;
    gSurfPrev.swap(gSurfCur);
    gSurfCur.clear();
    gSurfBoxPrev.swap(gSurfBoxCur);
    gSurfBoxCur.clear();
}
// 1.9.122 (refactor): one cached model-space bounding box for the current client vertex array range, shared by every user (surface tops,
// shadows, equipment, detector width, stack size) - each had its own copy of this loop and cache. nullptr: a vertex buffer or unreadable.
struct ArrBox { float lo[3], hi[3]; };
static const ArrBox* ArrBounds(GLint first, GLsizei count) {
    const ArrState& AS = GetArr();
    if (!AS.vp || AS.vbo || AS.ty != 0x1406 || AS.sz < 3) return nullptr;
    const int st = AS.st ? AS.st : AS.sz * 4;
    static std::unordered_map<uint64_t, ArrBox> boxes;
    const uint64_t key = ((uint64_t)(uintptr_t)AS.vp + (uint64_t)first * st) * 31u ^ (uint64_t)count;
    auto bi = boxes.find(key);
    if (bi != boxes.end()) return &bi->second;
    if (!Readable((uint8_t*)AS.vp + (size_t)first * st, (size_t)count * st)) return nullptr;
    ArrBox b = {{1e9f, 1e9f, 1e9f}, {-1e9f, -1e9f, -1e9f}};
    for (int i = 0; i < count; i++) {
        const float* P = (const float*)((uint8_t*)AS.vp + (size_t)(first + i) * st);
        for (int a = 0; a < 3; a++) { b.lo[a] = std::min(b.lo[a], P[a]); b.hi[a] = std::max(b.hi[a], P[a]); }
    }
    if (boxes.size() > 8192) boxes.clear();
    return &boxes.emplace(key, b).first->second;
}
static void SurfaceRecord(GLint first, GLsizei count) {
    const ArrBox* bp = ArrBounds(first, count);
    if (!bp) return;
    const ArrBox& b = *bp;
    GLdouble mv[16];
    GetMV(mv);
    double wlo[3] = {1e9, 1e9, 1e9}, whi[3] = {-1e9, -1e9, -1e9};
    for (int k = 0; k < 8; k++) {
        double p[3] = {(k & 1) ? b.hi[0] : b.lo[0], (k & 2) ? b.hi[1] : b.lo[1], (k & 4) ? b.hi[2] : b.lo[2]}, e[3], w[3];
        for (int r = 0; r < 3; r++) e[r] = mv[r] * p[0] + mv[4 + r] * p[1] + mv[8 + r] * p[2] + mv[12 + r];
        for (int r = 0; r < 3; r++) w[r] = gViewInv[r] * e[0] + gViewInv[4 + r] * e[1] + gViewInv[8 + r] * e[2] + gViewInv[12 + r];
        w[2] *= gCfg.upSign;
        for (int a = 0; a < 3; a++) { wlo[a] = std::min(wlo[a], w[a]); whi[a] = std::max(whi[a], w[a]); }
    }
    if (whi[0] - wlo[0] > 6 || whi[1] - wlo[1] > 6 || whi[2] - wlo[2] > 2.5 || whi[2] < 0.1) return;      // not furniture-sized
    if (fabs(wlo[0] + whi[0]) < 0.1 && fabs(wlo[1] + whi[1]) < 0.1) return;      // at the origin: first frames, before the 3D camera is set
    float top = (float)(gObjSurface == 2 ? wlo[2] + (whi[2] - wlo[2]) * 0.45 : (gObjSurface == 3 ? wlo[2] + (whi[2] - wlo[2]) * 0.82 : whi[2]));      // 3: the freezer's well floor (kitchen.py: 74 of 90)
    SurfFrameTick();
    for (int ty = (int)floor(wlo[1] + 0.05); ty <= (int)floor(whi[1] - 0.05); ty++)
        for (int tx = (int)floor(wlo[0] + 0.05); tx <= (int)floor(whi[0] - 0.05); tx++) {
            float& h = gSurfCur[ty * 65536 + tx];
            h = std::max(h, top);
            SurfBox sb = {(float)wlo[0], (float)whi[0], (float)wlo[1], (float)whi[1]};
            gSurfBoxCur[ty * 65536 + tx] = sb;
        }
    // the tiles the furniture was moved off (it stands half a tile toward a wall, edges off the tile grid): what lies there is on it too.
    // Only along an axis where it is off the grid, so things on the floor beside an ordinary table are not pulled onto it.
    double offX = fabs(wlo[0] - floor(wlo[0] + 0.5)) > 0.2 ? 0.45 : -0.05, offY = fabs(wlo[1] - floor(wlo[1] + 0.5)) > 0.2 ? 0.45 : -0.05;
    if (offX > 0 || offY > 0)
    for (int ty = (int)floor(wlo[1] - offY); ty <= (int)floor(whi[1] + offY); ty++)
        for (int tx = (int)floor(wlo[0] - offX); tx <= (int)floor(whi[0] + offX); tx++) {
            int k = ty * 65536 + tx;
            if (gSurfBoxCur.count(k)) continue;
            SurfBox sb = {(float)wlo[0], (float)whi[0], (float)wlo[1], (float)whi[1]};
            gSurfBoxCur[k] = sb;
            gSurfCur[k] = std::max(gSurfCur[k], top);
        }
    static int sl = 0;
    if (sl < 10) { sl++; Log("SURFACE %s at (%.2f..%.2f, %.2f..%.2f): height %.2f", gObjSurface == 1 ? "table top" : "seat", wlo[0], whi[0], wlo[1], whi[1], top); }
}
// how far to lift this small thing (0 = leave it): its origin against the surface recorded on its tile this frame or the last
static double SurfaceLift() {
    GLdouble mv[16];
    GetMV(mv);
    double ox = MVWorldX(mv),
           oy = MVWorldY(mv),
           oh = gCfg.upSign * (gViewInv[2] * mv[12] + gViewInv[6] * mv[13] + gViewInv[10] * mv[14] + gViewInv[14]);
    if (fabs(ox) < 0.05 && fabs(oy) < 0.05) return 0;      // first frames, before the 3D camera is set
    int k = (int)floor(oy) * 65536 + (int)floor(ox);
    float top = -1;
    auto a = gSurfCur.find(k);
    if (a != gSurfCur.end()) top = a->second;
    auto p = gSurfPrev.find(k);
    if (p != gSurfPrev.end()) top = std::max(top, p->second);
    if (top <= 0 || oh >= top - 0.02) return 0;
    static int ll = 0;
    if (ll < 10) { ll++; Log("SURFACE lift at (%.2f, %.2f): from %.2f onto %.2f", ox, oy, oh, top); }
    return top + 0.01 - oh;
}
// 1.9.125 (user: the psychologist stands inside the sofa): the game has no sitting pose - someone using a sofa or bench stands on its seat
// tile, upright, and the 3D seat cuts through their legs. A person who has stood still on a seat tile for a moment (20 frames) is drawn
// lowered by seatDrop, so the body sits on the cushion and the seat hides the legs. Walkers crossing the tile never get that still.
static double SeatDrop() {
    GLdouble mv[16];
    GetMV(mv);
    const double Uz = gViewInv[2] * mv[4] + gViewInv[6] * mv[5] + gViewInv[10] * mv[6];
    if (fabs(Uz) < 0.5) return 0;      // lying down: SnapLying's
    const double ox = MVWorldX(mv), oy = MVWorldY(mv);
    if (fabs(ox) < 0.05 && fabs(oy) < 0.05) return 0;
    const int k = (int)floor(oy) * 65536 + (int)floor(ox);
    float top = -1;
    auto a = gSurfCur.find(k);
    if (a != gSurfCur.end()) top = a->second;
    auto p = gSurfPrev.find(k);
    if (p != gSurfPrev.end()) top = std::max(top, p->second);
    if (top < 0.12f || top > 0.55f) return 0;      // seats only (table and desk tops are ~0.8)
    struct Still { int frame, n; };
    static std::unordered_map<long long, Still> still;
    if (still.size() > 8192) still.clear();
    Still& s = still[(long long)llround(oy * 16.0) * 1000003LL + llround(ox * 16.0)];
    if (s.frame != gWallFramesSeenExt) {
        s.n = (s.n > 0 && gWallFramesSeenExt - s.frame <= 3) ? s.n + 1 : 1;
        s.frame = gWallFramesSeenExt;
    }
    if (s.n < 20) return 0;
    static int sl = 0;
    if (sl < 6) { sl++; Log("SEATED person at (%.2f, %.2f) on a seat %.2f high: lowered %.2f", ox, oy, top, gCfg.seatDrop); }
    return gCfg.seatDrop;
}
static bool HeadSwapDraw(GLenum mode, GLint first, GLsizei count) {
    if (!gCfg.headSwap || !gObjPrisoner || gPrisHead < 0 || mode != GL_TRIANGLES || (unsigned)(gDrawSerial - gNoteSerial) > 16) return false;
    int own = BodyOwnHead((int)count);
    if (own < 0) return false;
    int cell = gPrisHead < (int)(sizeof kHeadCell / sizeof kHeadCell[0]) ? kHeadCell[gPrisHead] : -1;
    if (cell < 0) return false;
    int want = kHeadStyle[cell];
    if (want == own) return false;      // the body already wears this head
    if (gSwapState == 0) LoadHeadSwap();
    if (gSwapState != 1) return false;
    auto it = gSwap.find((int)count * 8 + want);
    if (it == gSwap.end()) return false;
    SwapMesh& sm = it->second;
    const ArrState& AS = GetArr();
    if (!AS.vp || AS.vbo || AS.ty != 0x1406 || AS.sz < 3) return false;
    int stride = AS.st ? AS.st : AS.sz * 4;
    if (!Readable((uint8_t*)AS.vp + (size_t)first * stride, (size_t)count * stride)) return false;
    if (!sm.scaled) {      // OBJ units -> the game's model space: each axis on its own, the body's whole extent as the game draws it
        auto bb = gBodyBox.find((int)count);
        if (bb == gBodyBox.end()) return false;
        float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
        for (int i = 0; i < count; i++) {
            const float* P = (const float*)((uint8_t*)AS.vp + (size_t)(first + i) * stride);
            for (int a = 0; a < 3; a++) { lo[a] = std::min(lo[a], P[a]); hi[a] = std::max(hi[a], P[a]); }
        }
        float k[3], o[3];
        for (int a = 0; a < 3; a++) {
            float span = bb->second.hi[a] - bb->second.lo[a];
            if (span <= 1e-6f || hi[a] - lo[a] <= 1e-9f) return false;
            k[a] = (hi[a] - lo[a]) / span; o[a] = lo[a] - k[a] * bb->second.lo[a];
        }
        for (int i = 0; i < sm.n; i++) {
            for (int a = 0; a < 3; a++) sm.pos[3 * i + a] = sm.pos[3 * i + a] * k[a] + o[a];
            float nx = sm.nrm[3 * i] / k[0], ny = sm.nrm[3 * i + 1] / k[1], nz = sm.nrm[3 * i + 2] / k[2], L = sqrtf(nx * nx + ny * ny + nz * nz);
            if (L > 1e-6f) { sm.nrm[3 * i] = nx / L; sm.nrm[3 * i + 1] = ny / L; sm.nrm[3 * i + 2] = nz / L; }
        }
        sm.scaled = true;
        static int sl = 0;
        if (sl < 16) { sl++; Log("HEADSWAP body %d with head %d: %d vertices, scale x %.5f y %.5f z %.5f, offset %.4f %.4f %.4f", (int)count, want, sm.n, k[0], k[1], k[2], o[0], o[1], o[2]); }
    }
    // swap the arrays in, draw, put the game's back
    void *vp = nullptr, *np = nullptr, *tp = nullptr;
    GLint vs = 0, vt = 0, vst = 0, nt = 0, nst = 0, ts = 0, tt = 0, tst = 0;
    glGetPointerv(GL_VERTEX_ARRAY_POINTER, &vp); glGetIntegerv(0x807A, &vs); glGetIntegerv(0x807B, &vt); glGetIntegerv(0x807C, &vst);
    glGetPointerv(GL_NORMAL_ARRAY_POINTER, &np); glGetIntegerv(0x807E, &nt); glGetIntegerv(0x807F, &nst);
    glGetPointerv(GL_TEXTURE_COORD_ARRAY_POINTER, &tp); glGetIntegerv(0x8088, &ts); glGetIntegerv(0x8089, &tt); glGetIntegerv(0x808A, &tst);
    glVertexPointer(3, GL_FLOAT, 0, sm.pos.data());
    glNormalPointer(GL_FLOAT, 0, sm.nrm.data());
    glTexCoordPointer(2, GL_FLOAT, 0, sm.tex.data());
    oDrawArrays(GL_TRIANGLES, 0, sm.n);
    glVertexPointer(vs, vt, vst, vp);
    glNormalPointer(nt, nst, np);
    glTexCoordPointer(ts, tt, tst, tp);
    return true;
}// Equipment the 2D view draws as overlay sprites (cuffs, baton, taser, gun): small solid items on the person's body, in its model space,
// sized from the body's own bounds. Drawn once per noted person, right after the body, with the fixed-function pipeline (plain colours).
static unsigned gEquipDone = 0;
static void EquipBox(float x0, float x1, float y0, float y1, float z0, float z1, float r, float g, float b) {
    const float sh[6] = {1.0f, 0.55f, 0.85f, 0.85f, 0.72f, 0.72f};      // top, bottom, +z, -z, +x, -x
    const float V[6][4][3] = {{{x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1}}, {{x0, y0, z0}, {x0, y0, z1}, {x1, y0, z1}, {x1, y0, z0}},
                              {{x0, y0, z1}, {x0, y1, z1}, {x1, y1, z1}, {x1, y0, z1}}, {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0}},
                              {{x1, y0, z0}, {x1, y0, z1}, {x1, y1, z1}, {x1, y1, z0}}, {{x0, y0, z0}, {x0, y1, z0}, {x0, y1, z1}, {x0, y0, z1}}};
    for (int k = 0; k < 6; k++) {
        glColor3f(r * sh[k], g * sh[k], b * sh[k]);
        for (int v = 0; v < 4; v++) glVertex3f(V[k][v][0], V[k][v][1], V[k][v][2]);
    }
}
static unsigned gEquipSkip[6] = {0};      // diagnostic: why equipment was not drawn (other note, wall pass / no camera, not triangles / small, note too old, vertex buffer)
// 1.9.67: people's extras are queued in WORLD space and drawn after the own floor with the loose items (DrawQueuedEquip). Drawn in model
// space right after the body (DrawEquipmentOld) they never showed (user 8 Nov: no cuffs, no weapons) although EQUIPSTAT counted them.
// Every person gets round hands (2D draws them: at the sides, together in front when cuffed) and a soft shadow on the floor.
struct EquipQ { float cx, cy, rx, ry, fx, fy, R, D, h0, h1; uint32_t skin; bool cuffs, baton, tazer, gun; };
static std::vector<EquipQ> gEquipCur, gEquipPrev;
static int gEquipFrame = -1;
static unsigned gEquipNoteDone = 0;
static void DrawEquipment(GLenum mode, GLint first, GLsizei count) {
    // keyed on the last PERSON note, not the last note: a guard's baton / a prisoner's cuffs are noted as objects between the person and its body
    if (!(gCfg.showEquipment || gCfg.showHands || gCfg.personShadows) || !gPersonNote || gInWallPass || !gCam.active || mode != GL_TRIANGLES || count < 400) return;
    if ((unsigned)(gDrawSerial - gPersonNote) > 24 || gEquipNoteDone == gPersonNote) return;
    if (!gObjPersonExt && (unsigned)(gDrawSerial - gNoteSerial) <= 16 && (gObjSurface || gObjLift || gObjServing || gObjDoor || gObjSnapType || gObjBench || gObjStructural)) return;      // a later note is a real object: its draw, not the body (1.9.87: gates got hands)
    // 1.9.103 (user: bushes have hands): a later note that is not this person's equipment means the draw is that object's, whatever it is
    // (a baton / taser carried by the person is noted where the person stands: it is let through)
    {      // 1.9.110 (user: a wooden stool has hands): body models are learned by vertex count from draws straight after a person's own note;
           // after another object's note, only a draw of a known body count is the person's body
        static std::unordered_set<int> bodyCounts;
        if (!gPersonHands) return;      // 1.9.111: visitors, lawyers, civilians... get none
        if (gObjPersonExt || gNoteSerial == gPersonNote) { if (bodyCounts.size() < 512) bodyCounts.insert((int)count); }
        else if (!bodyCounts.count((int)count)) return;
    }
    // after another object's note: 1.9.111 (user: hands again, flickering) - that note must be known to stand on the person's spot (a baton,
    // a taser carried); unknown positions no longer let it through
    if (!gObjPersonExt && gNoteSerial != gPersonNote && (unsigned)(gDrawSerial - gNoteSerial) <= 16 &&
        !(gNotePosOk && gPersonPosOk && fabs(gNotePos[0] - gPersonPos[0]) < 0.3 && fabs(gNotePos[1] - gPersonPos[1]) < 0.3)) return;
    if (gPersonPosOk) {      // 1.9.87: the body is drawn where the person stands - gates, coat stands... drawn soon after a person got its hands
        GLdouble mv0[16];
        GetMV(mv0);
        double ox = MVWorldX(mv0);
        double oy = MVWorldY(mv0);
        if (fabs(ox - gPersonPos[0]) > 0.8 || fabs(oy - gPersonPos[1]) > 0.8) return;
    }
    const ArrBox* bbp = ArrBounds(first, count);
    if (!bbp) { gEquipSkip[4]++; return; }
    struct BB { float mn[3], mx[3]; } bb;
    for (int k = 0; k < 3; k++) { bb.mn[k] = bbp->lo[k]; bb.mx[k] = bbp->hi[k]; }
    gEquipNoteDone = gPersonNote;
    // model -> world (x, y on the floor, h = height): world = gViewInv * MV * p
    GLdouble mv[16];
    GetMV(mv);
    double M[16];
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++) M[c * 4 + r] = gViewInv[r] * mv[c * 4] + gViewInv[4 + r] * mv[c * 4 + 1] + gViewInv[8 + r] * mv[c * 4 + 2] + gViewInv[12 + r] * mv[c * 4 + 3];
    double lo[3] = {1e9, 1e9, 1e9}, hi[3] = {-1e9, -1e9, -1e9};
    for (int k = 0; k < 8; k++) {
        double p[3] = {(k & 1) ? bb.mx[0] : bb.mn[0], (k & 2) ? bb.mx[1] : bb.mn[1], (k & 4) ? bb.mx[2] : bb.mn[2]}, w[3];
        for (int r = 0; r < 3; r++) w[r] = M[r] * p[0] + M[4 + r] * p[1] + M[8 + r] * p[2] + M[12 + r];
        w[2] *= gCfg.upSign;
        for (int a = 0; a < 3; a++) { lo[a] = std::min(lo[a], w[a]); hi[a] = std::max(hi[a], w[a]); }
    }
    if (hi[2] - lo[2] < 0.1 || hi[2] - lo[2] > 3.0 || (fabs(lo[0] + hi[0]) < 0.1 && fabs(lo[1] + hi[1]) < 0.1)) return;      // not a standing body / first frames
    // the body's sideways axis = model x on the floor; its front = whichever other model axis lies flattest (model +z, or +y if z is up)
    double rx = M[0], ry = M[1], rl = sqrt(rx * rx + ry * ry);
    double zx = M[8], zy = M[9], yx = M[4], yy = M[5];
    bool zFront = zx * zx + zy * zy >= yx * yx + yy * yy;
    double fx = zFront ? zx : yx, fy = zFront ? zy : yy, fl = sqrt(fx * fx + fy * fy);
    if (rl < 1e-6 || fl < 1e-6) return;
    EquipQ q;
    q.cx = (float)((lo[0] + hi[0]) * 0.5); q.cy = (float)((lo[1] + hi[1]) * 0.5);
    q.rx = (float)(rx / rl); q.ry = (float)(ry / rl); q.fx = (float)(fx / fl); q.fy = (float)(fy / fl);
    q.R = (float)(rl * (bb.mx[0] - bb.mn[0]) * 0.5);
    q.D = (float)(fl * (zFront ? (bb.mx[2] - bb.mn[2]) : (bb.mx[1] - bb.mn[1])) * 0.5);
    q.h0 = (float)lo[2]; q.h1 = (float)hi[2];
    if (q.R > 0.55f || q.D > 0.55f || q.h1 - q.h0 > 2.6f || q.h1 - q.h0 < 0.4f) return;      // 1.9.91: not person-sized (a boxing ring with its boxer at the centre got giant hands)
    q.skin = gPersonPrisoner && gPersonSkin ? (gPersonSkin & 0xffffff) : 0xd8a878;
    bool eq = gCfg.showEquipment && gEquip.serial && (unsigned)(gDrawSerial - gEquip.serial) <= 24;
    q.cuffs = eq && gEquip.cuffs; q.baton = eq && gEquip.baton; q.tazer = eq && gEquip.tazer; q.gun = eq && gEquip.gun;
    if (eq) { gEquipDone = gEquip.serial; gEquipSkip[0]++; }
    if (gEquipFrame != gSwapCount) {      /* 1.9.115: per real frame (buffer swap), not per wall pass */ gEquipPrev.swap(gEquipCur); gEquipCur.clear(); gEquipFrame = gSwapCount; }
    if (gEquipCur.size() < 4000) gEquipCur.push_back(q);
    static int dl = 0;
    if (dl < 12 && (q.cuffs || q.baton || q.gun || dl < 3)) { dl++; Log("EQUIPQ person at (%.2f, %.2f) height %.2f..%.2f half width %.2f depth %.2f front (%.2f, %.2f)%s: cuffs %d baton %d taser %d gun %d", q.cx, q.cy, q.h0, q.h1, q.R, q.D, q.fx, q.fy, zFront ? "" : " (model y)", q.cuffs, q.baton, q.tazer, q.gun); }
}
// Soft contact shadows under furniture and other floor objects (user 8 Nov: improve shadows, low cost): each object's footprint from its
// world bounds, once per note, queued and drawn as a dark rectangle with a fading rim, slightly offset away from the light.
struct ShadowQ { float x0, y0, x1, y1, h0, a; };
static std::vector<ShadowQ> gShadCur, gShadPrev;
static int gShadFrame = -1;
static unsigned gShadNote = 0;
static void ObjShadowRecord(GLint first, GLsizei count) {
    if (gShadNote == gNoteSerial) return;
    gShadNote = gNoteSerial;
    const ArrBox* bp = ArrBounds(first, count);
    if (!bp) return;
    const ArrBox& b = *bp;
    GLdouble mv[16];
    GetMV(mv);
    double lo[3] = {1e9, 1e9, 1e9}, hi[3] = {-1e9, -1e9, -1e9};
    for (int k = 0; k < 8; k++) {
        double p[3] = {(k & 1) ? b.hi[0] : b.lo[0], (k & 2) ? b.hi[1] : b.lo[1], (k & 4) ? b.hi[2] : b.lo[2]}, e[3], w[3];
        for (int r = 0; r < 3; r++) e[r] = mv[r] * p[0] + mv[4 + r] * p[1] + mv[8 + r] * p[2] + mv[12 + r];
        for (int r = 0; r < 3; r++) w[r] = gViewInv[r] * e[0] + gViewInv[4 + r] * e[1] + gViewInv[8 + r] * e[2] + gViewInv[12 + r];
        w[2] *= gCfg.upSign;
        for (int a = 0; a < 3; a++) { lo[a] = std::min(lo[a], w[a]); hi[a] = std::max(hi[a], w[a]); }
    }
    double sx = hi[0] - lo[0], sy = hi[1] - lo[1], sh = hi[2] - lo[2];
    if (sh < 0.12 || sx > 4.5 || sy > 4.5 || sx < 0.08 || sy < 0.08) return;      // flat (rugs, floor marks) or huge: no shadow
    if (lo[2] > 0.35 || lo[2] < -0.3) return;      // not standing on the floor (wall lights, windows, things on tables)
    if (fabs(lo[0] + hi[0]) < 0.1 && fabs(lo[1] + hi[1]) < 0.1) return;      // first frames, before the 3D camera is set
    ShadowQ q;
    double in = 0.04;      // tuck in a little: the rim fades out past the footprint
    q.x0 = (float)(lo[0] + in); q.x1 = (float)(hi[0] - in); q.y0 = (float)(lo[1] + in); q.y1 = (float)(hi[1] - in);
    q.h0 = (float)std::max(0.0, lo[2]);
    q.a = (float)std::min(0.30, 0.12 + sh * 0.12);      // taller things cast a darker shadow
    if (gShadFrame != gSwapCount) {      /* 1.9.115: per real frame (buffer swap), not per wall pass */ gShadPrev.swap(gShadCur); gShadCur.clear(); gShadFrame = gSwapCount; }
    if (gShadCur.size() < 12000) gShadCur.push_back(q);
}
// ---------------------------------------------------------------------------
// Lamps and night (1.9.72, user 8 Nov: "it's midnight but the yard is light; you can't see light cast from the floor or spot lights;
// indoors is sterile, like fluorescent panels, but it's many bulbs"). Every lamp the game draws is remembered by position; a light-pool
// map (4 texels a tile: R = indoor bulb pools, G = outdoor lamp pools, B = near an indoor bulb) is rebuilt when the set changes and
// sampled once per pixel by the floor, model and wall shaders. Pools stay inside the lamp's own room (same room id / indoors flag).
// How dark the night is comes from the game's own lightmap: the median light of outdoor tiles, read back every 2 s.
// ---------------------------------------------------------------------------
struct BulbL { float x, y; int kind; int seen; };
static std::unordered_map<long long, BulbL> gBulbs;
static bool gBulbDirty = false;      // a full rebuild is needed (lamps gone, another save)
static std::vector<BulbL> gBulbNew;      // lamps found since the last update: added to the map without a rebuild
static std::vector<BulbL> gBulbGone;      // lamps gone since the last update: subtracted from the map without a rebuild (1.9.80: a full
                                          // rebuild of ~1100 lamps took 9 ms and ran about once a second - a dropped frame every second)
static int gBulbW = 0, gBulbH = 0, gBulbWorldW = 0, gBulbWorldH = 0;
static double gBulbNext = 0;
static unsigned gLightNote = 0;
static void BulbRecord() {
    if (gLightNote == gNoteSerial) return;
    gLightNote = gNoteSerial;
    GLdouble mv[16];
    GetMV(mv);
    double ox = MVWorldX(mv);
    double oy = MVWorldY(mv);
    if (fabs(ox) < 0.5 && fabs(oy) < 0.5) return;      // first frames, before the 3D camera is set
    // 1.9.74: the 1.9.72 log showed ~1100 "lamps" that kept vanishing and coming back (the map rebuilt twice a second, 5 ms each): draws of
    // OTHER objects after a lamp's note were taken as lamps. Ceiling lights, outdoor lamps and floodlights stand at a tile centre.
    if (gObjLight != 4 && (fabs(ox - floor(ox) - 0.5) > 0.2 || fabs(oy - floor(oy) - 0.5) > 0.2)) return;
    long long key = ((long long)(int)floor(oy * 2.0) << 32) ^ (long long)(unsigned)(int)floor(ox * 2.0);
    auto it = gBulbs.find(key);
    if (it != gBulbs.end()) { it->second.seen = gSwapCount; return; }
    if (gBulbs.size() >= 4000) return;
    BulbL b = {(float)ox, (float)oy, gObjLight, gSwapCount};
    gBulbs[key] = b;
    gBulbNew.push_back(b);      // splatted on its own (1.9.75): no full rebuild for each lamp found
    static int bl = 0;
    if (bl < 12) { bl++; Log("BULB kind %d at (%.2f, %.2f), %d known", gObjLight, ox, oy, (int)gBulbs.size()); }
}
// Lamps not drawn for about a minute while their spot is in the middle of the screen were removed (or sold): forget them. Once a second.
// (1.9.74's 3 s near the screen edge dropped hundreds of real lamps at a time - the prison has ~1100 - and found them again: constant rebuilds.)
static void BulbPrune() {
    static double next = 0;
    double now = NowSec();
    if (now < next || gBulbs.empty()) return;
    next = now + 1.0;
    int gone = 0;
    for (auto it = gBulbs.begin(); it != gBulbs.end();) {
        const BulbL& b = it->second;
        double p[4] = {b.x, b.y, 0.0, 1.0}, e[4], c[4];
        for (int r = 0; r < 4; r++) e[r] = gCamMV[r] * p[0] + gCamMV[4 + r] * p[1] + gCamMV[8 + r] * p[2] + gCamMV[12 + r] * p[3];
        for (int r = 0; r < 4; r++) c[r] = gCamP[r] * e[0] + gCamP[4 + r] * e[1] + gCamP[8 + r] * e[2] + gCamP[12 + r] * e[3];
        bool onScreen = c[3] > 0 && fabs(c[0] / c[3]) < 0.5 && fabs(c[1] / c[3]) < 0.5;
        if (onScreen && gSwapCount - b.seen > 7200) { gBulbGone.push_back(b); it = gBulbs.erase(it); gone++; }
        else ++it;
    }
    static int gl = 0;
    if (gone && gl < 20) { gl++; Log("BULB %d lamps gone, %d known", gone, (int)gBulbs.size()); }      // taken off the map on their own (1.9.80), no rebuild
}
static void BulbBuild() {
    uint8_t* app = App();
    if (!app || !pActiveTexture) return;
    uint8_t* world = *(uint8_t**)(app + 0x198);
    if (!Readable(world, 0x48f0)) return;
    int w = *(int*)(world + 0x60), h = *(int*)(world + 0x64);
    const uint8_t* cells = *(uint8_t**)(world + 0x48e8);
    if (w <= 8 || h <= 8 || w > 4096 || h > 4096 || !Readable(cells, (size_t)w * h * 0xe0)) return;
    double now = NowSec();
    bool sizeChanged = w != gBulbWorldW || h != gBulbWorldH;
    if (sizeChanged) { gBulbs.clear(); gBulbNew.clear(); gBulbGone.clear(); gBulbDirty = true; gBulbReady = false; }      // another save
    else gBulbReady = gBulbTex != 0;
    if ((!gBulbDirty && gBulbNew.empty() && gBulbGone.empty()) || now < gBulbNext) return;
    gBulbNext = now + 0.25;
    bool full = gBulbDirty || !gBulbTex;
    gBulbDirty = false;
    int s = 4;
    while (s > 1 && (w * s > 4096 || h * s > 4096)) s--;
    int W = w * s, H = h * s;
    static std::vector<uint8_t> buf;
    static std::vector<float> fr, fg, fb;
    if (full || fr.size() != (size_t)W * H) { full = true; fr.assign((size_t)W * H, 0.0f); fg.assign((size_t)W * H, 0.0f); fb.assign((size_t)W * H, 0.0f); }
    int rx0 = W, ry0 = H, rx1 = -1, ry1 = -1;      // the part of the map that changed
    auto cellAt = [&](int x, int y) -> const uint8_t* { return cells + ((size_t)y * w + x) * 0xe0; };
    std::vector<BulbL> todo;
    std::vector<float> sign;      // +1 add a lamp, -1 take one off (the map is a plain sum, clamped when uploaded)
    if (full) { for (auto& kv : gBulbs) { todo.push_back(kv.second); sign.push_back(1.0f); } }
    else {
        for (const BulbL& b : gBulbNew) { todo.push_back(b); sign.push_back(1.0f); }
        for (const BulbL& b : gBulbGone) { todo.push_back(b); sign.push_back(-1.0f); }
    }
    gBulbNew.clear();
    gBulbGone.clear();
    for (size_t bi = 0; bi < todo.size(); bi++) {
        const BulbL& b = todo[bi];
        const float sg = sign[bi];
        int tx = (int)floor(b.x), ty = (int)floor(b.y);
        if (tx < 0 || ty < 0 || tx >= w || ty >= h) continue;
        const uint8_t* lc = cellAt(tx, ty);
        if (IsWallMat(*(const int32_t*)(lc + 0x18))) {      // a wall light sits in the wall tile: light the open side
            static const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (auto& d : d4) {
                int nx = tx + d[0], ny = ty + d[1];
                if (nx >= 0 && ny >= 0 && nx < w && ny < h && !IsWallMat(*(const int32_t*)(cellAt(nx, ny) + 0x18))) { lc = cellAt(nx, ny); break; }
            }
        }
        bool ind = lc[0x30] != 0;
        int room = *(const int32_t*)(lc + 0x50);
        // 1.9.76 (user: "radius too hard, should feather further"): half as wide again, with a long linear-squared tail instead of a plateau
        float r = 1.5f * (b.kind == 1 ? 3.1f : (b.kind == 2 ? 5.5f : (b.kind == 3 ? 9.0f : 3.9f)));
        float strength = b.kind == 3 ? 0.9f : 0.75f;
        bool indoorPool = ind;
        float rc = indoorPool ? r * 1.4f : r;
        int x0 = std::max(0, (int)floor((b.x - rc) * s)), x1 = std::min(W - 1, (int)ceil((b.x + rc) * s));
        int y0 = std::max(0, (int)floor((b.y - rc) * s)), y1 = std::min(H - 1, (int)ceil((b.y + rc) * s));
        rx0 = std::min(rx0, x0); ry0 = std::min(ry0, y0); rx1 = std::max(rx1, x1); ry1 = std::max(ry1, y1);
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                const uint8_t* c = cellAt(x / s, y / s);
                bool wall = IsWallMat(*(const int32_t*)(c + 0x18));
                if (!wall && ((c[0x30] != 0) != ind || (ind && *(const int32_t*)(c + 0x50) != room))) continue;      // stays in its own room
                float dx = (x + 0.5f) / s - b.x, dy = (y + 0.5f) / s - b.y, d = sqrtf(dx * dx + dy * dy);
                size_t i = (size_t)y * W + x;
                if (d < r) {
                    float q = 1.0f - d / r;
                    q = q * q * strength;
                    // kept as a sum of -ln(1 - q), shown as 1 - exp(-sum): overlapping pools add up, never past full, and a lamp
                    // can be taken off again exactly by subtracting its own share
                    float& t = indoorPool ? fr[i] : fg[i];
                    t += sg * -logf(1.0f - std::min(q, 0.95f));
                }
                if (indoorPool && d < rc) {
                    float cv = d < r ? 1.0f : 1.0f - (d - r) / (rc - r);
                    fb[i] += sg * cv;      // near a bulb: a sum, clamped to 1 when shown
                }
            }
    }
    if (full) { rx0 = 0; ry0 = 0; rx1 = W - 1; ry1 = H - 1; }
    if (rx1 < rx0 || ry1 < ry0) return;
    int RW = rx1 - rx0 + 1, RH = ry1 - ry0 + 1;
    buf.resize((size_t)RW * RH * 4);
    for (int y = 0; y < RH; y++)
        for (int x = 0; x < RW; x++) {
            size_t i = (size_t)(ry0 + y) * W + rx0 + x, o = ((size_t)y * RW + x) * 4;
            float vr = fr[i] > 1e-4f ? 1.0f - expf(-fr[i]) : 0.0f, vg = fg[i] > 1e-4f ? 1.0f - expf(-fg[i]) : 0.0f;
            float vb = std::min(1.0f, std::max(0.0f, fb[i]));
            buf[o + 0] = (uint8_t)(vr * 255.0f + 0.5f);
            buf[o + 1] = (uint8_t)(vg * 255.0f + 0.5f);
            buf[o + 2] = (uint8_t)(vb * 255.0f + 0.5f);
            buf[o + 3] = 255;
        }
    GLint act = 0, prev = 0, pa = 4;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &act);
    pActiveTexture(GL_TEXTURE0 + 14);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev);
    if (!gBulbTex) glGenTextures(1, &gBulbTex);
    glBindTexture(GL_TEXTURE_2D, gBulbTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &pa);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (full) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
    else glTexSubImage2D(GL_TEXTURE_2D, 0, rx0, ry0, RW, RH, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, pa);
    glBindTexture(GL_TEXTURE_2D, (GLuint)prev);
    pActiveTexture((GLenum)act);
    gBulbW = W; gBulbH = H; gBulbWorldW = w; gBulbWorldH = h; gBulbReady = true;
    static int ll = 0;
    if (ll < 30 && (full || ll < 10)) { ll++; Log("BULBMAP %s: %d lamps (%d done), %dx%d (%d a tile), %.1f ms", full ? "rebuilt" : "added", (int)gBulbs.size(), (int)todo.size(), W, H, s, (NowSec() - now) * 1000.0); }
}
// How dark it is: the median light of outdoor tiles in the game's lightmap (on unit lmUnit), read back every 2 s, eased.
static void NightTick(GLint lmUnit) {
    static double next = 0, lastT = 0;
    double now = NowSec();
    double dt = lastT > 0 ? now - lastT : 0.0;
    lastT = now;
    gNight += (gNightTarget - gNight) * (float)std::min(1.0, dt * 0.7);
    // 1.9.80: read the lightmap back through a pixel buffer: the copy is queued one call and collected half a second later, so the
    // game never waits for the graphics card (a plain glGetTexImage stalled the frame every 2 s). Plain read every 5 s if unavailable.
    typedef void(APIENTRY * PFN_GenBuffers)(GLsizei, GLuint*);
    typedef void(APIENTRY * PFN_BindBuffer)(GLenum, GLuint);
    typedef void(APIENTRY * PFN_BufferData)(GLenum, ptrdiff_t, const void*, GLenum);
    typedef void*(APIENTRY * PFN_MapBuffer)(GLenum, GLenum);
    typedef GLboolean(APIENTRY * PFN_UnmapBuffer)(GLenum);
    static PFN_GenBuffers genB = nullptr; static PFN_BindBuffer bindB = nullptr; static PFN_BufferData dataB = nullptr;
    static PFN_MapBuffer mapB = nullptr; static PFN_UnmapBuffer unmapB = nullptr;
    static bool looked = false;
    if (!looked && oWglGetProcAddress) {
        looked = true;
        genB = (PFN_GenBuffers)oWglGetProcAddress("glGenBuffers"); bindB = (PFN_BindBuffer)oWglGetProcAddress("glBindBuffer");
        dataB = (PFN_BufferData)oWglGetProcAddress("glBufferData"); mapB = (PFN_MapBuffer)oWglGetProcAddress("glMapBuffer");
        unmapB = (PFN_UnmapBuffer)oWglGetProcAddress("glUnmapBuffer");
    }
    const bool pbo = genB && bindB && dataB && mapB && unmapB;
    static GLuint pb = 0;
    static bool pending = false;
    static GLint pTW = 0, pTH = 0;
    if (lmUnit < 0 || now < next || !pActiveTexture) return;
    uint8_t* app = App();
    if (!app) return;
    uint8_t* world = *(uint8_t**)(app + 0x198);
    if (!Readable(world, 0x48f0)) return;
    int w = *(int*)(world + 0x60), h = *(int*)(world + 0x64);
    const uint8_t* cells = *(uint8_t**)(world + 0x48e8);
    if (w <= 8 || h <= 8 || w > 4096 || h > 4096 || !Readable(cells, (size_t)w * h * 0xe0)) return;
    static std::vector<uint8_t> px;
    GLint tw = 0, th = 0;
    bool have = false;
    if (pbo && pending) {      // collect the copy queued last time
        pending = false;
        next = now + 1.5;
        bindB(0x88EB /* GL_PIXEL_PACK_BUFFER */, pb);
        const uint8_t* src = (const uint8_t*)mapB(0x88EB, 0x88B8 /* GL_READ_ONLY */);
        if (src) { px.assign(src, src + (size_t)pTW * pTH * 4); tw = pTW; th = pTH; have = true; }
        unmapB(0x88EB);
        bindB(0x88EB, 0);
    } else {
        GLint act = 0, tex = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &act);
        pActiveTexture(GL_TEXTURE0 + lmUnit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
        if (tex) {
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tw);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &th);
        }
        if (tex && tw > 0 && th > 0 && (long long)tw * th <= 4096LL * 4096LL) {
            GLint pk = 4;
            glGetIntegerv(GL_PACK_ALIGNMENT, &pk);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            if (pbo) {      // queue the copy; read it next time
                if (!pb) genB(1, &pb);
                bindB(0x88EB, pb);
                dataB(0x88EB, (ptrdiff_t)tw * th * 4, nullptr, 0x88E1 /* GL_STREAM_READ */);
                glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)0);
                bindB(0x88EB, 0);
                pending = true; pTW = tw; pTH = th;
                next = now + 0.5;
            } else {
                px.resize((size_t)tw * th * 4);
                glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                have = true;
                next = now + 5.0;
            }
            glPixelStorei(GL_PACK_ALIGNMENT, pk);
        } else next = now + 2.0;
        pActiveTexture((GLenum)act);
    }
    if (have) {
        int hist[256] = {0}, n = 0;
        int step = std::max(1, (int)sqrt((double)w * h / 20000.0));
        for (int y = 0; y < h; y += step)
            for (int x = 0; x < w; x += step) {
                const uint8_t* c = cells + ((size_t)y * w + x) * 0xe0;
                if (c[0x30] != 0 || IsWallMat(*(const int32_t*)(c + 0x18))) continue;      // outdoors only
                int u = std::min(tw - 1, (int)((x + 0.5) / w * tw)), v = std::min(th - 1, (int)((y + 0.5) / h * th));
                const uint8_t* q = &px[((size_t)v * tw + u) * 4];
                hist[(q[0] + q[1] + q[2]) / 3]++;
                n++;
            }
        if (n > 20) {
            int acc = 0, med = 0;
            for (; med < 256; med++) { acc += hist[med]; if (acc * 2 >= n) break; }
            float m = med / 255.0f;
            float t = (m - 0.30f) / (0.75f - 0.30f);
            t = t < 0 ? 0 : (t > 1 ? 1 : t);
            float nt = 1.0f - t * t * (3.0f - 2.0f * t);
            static float lastLogged = -1.0f;
            if (fabs(nt - lastLogged) > 0.08f) { lastLogged = nt; Log("NIGHT outdoor median light %.2f -> night %.2f (lightmap %dx%d, %d samples)", m, nt, (int)tw, (int)th, n); }
            gNightTarget = nt;
            static bool first = true;
            if (first) { first = false; gNight = nt; }      // no fade-in on load
        }
    }
}
// Each frame, before the 3D draws: keep the lamp map current and on unit 14 for every shader.
static void BulbFrame() {
    if (!gCfg.bulbPools || !gCam.active) return;
    BulbPrune();
    BulbBuild();
    if (gBulbTex && pActiveTexture) {
        GLint act = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &act);
        pActiveTexture(GL_TEXTURE0 + 14);
        glBindTexture(GL_TEXTURE_2D, gBulbTex);
        pActiveTexture((GLenum)act);
    }
}
// The hidden small sprites (06_objects QueueSpriteTri) as thin extrusions: the sprite's own art in 5 stacked alpha-tested layers, the lower ones
// darker (reads as the sides), standing on the floor or on the table / seat under it. Drawn after the own floor, with depth testing.
static void DrawQueuedSprites() {
    const std::vector<SpriteTri>& Q = (gSprFrame == gWallFramesSeenExt && !gSprCur.empty()) ? gSprCur : (gSprFrame == gWallFramesSeenExt ? gSprPrev : gSprCur);
    if (Q.empty() || !gCfg.spriteFallback || !pActiveTexture) return;
    PFN_UseProgram use = oUseProgram ? oUseProgram : oUseProgramObjectARB;
    if (!use) return;
    GLint prevProg = 0, act = 0, prevTex = 0, mm = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &act);
    glGetIntegerv(GL_MATRIX_MODE, &mm);
    use(0);
    pActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixd(gCamMV);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);
    glDisable(GL_BLEND);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    const float us = gCfg.upSign, dark = 1.0f - 0.62f * gNight * gCfg.nightDark;
    const int layers = 5;
    const float thick = 0.10f;
    // 1.9.80: built into one vertex array per texture and drawn with one call each (glVertex per point cost ~0.5 ms a frame)
    typedef void(APIENTRY * PFN_BindBufferS)(GLenum, GLuint);
    static PFN_BindBufferS bindAB = oWglGetProcAddress ? (PFN_BindBufferS)oWglGetProcAddress("glBindBuffer") : nullptr;
    GLint prevAB = 0;
    glGetIntegerv(0x8894 /* GL_ARRAY_BUFFER_BINDING */, &prevAB);
    if (prevAB && bindAB) bindAB(0x8892 /* GL_ARRAY_BUFFER */, 0);
    const bool arrays = !prevAB || bindAB;
    struct SV { float u, v, r, g, b, a, x, y, z; };      // GL_T2F_C4F_N3F_V3F-free layout: interleaved by hand
    static std::vector<SV> verts;
    verts.clear();
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    auto flush = [&](GLuint tex) {
        if (verts.empty()) return;
        glBindTexture(GL_TEXTURE_2D, tex);
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_INDEX_ARRAY);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3, GL_FLOAT, sizeof(SV), &verts[0].x);
        glColorPointer(4, GL_FLOAT, sizeof(SV), &verts[0].r);
        glTexCoordPointer(2, GL_FLOAT, sizeof(SV), &verts[0].u);
        oDrawArrays(GL_TRIANGLES, 0, (GLsizei)verts.size());
        verts.clear();
    };
    GLuint bound = 0xFFFFFFFF;
    bool open = false;
    for (const SpriteTri& t : Q) {
        if (t.tex != bound) {
            if (arrays) { if (bound != 0xFFFFFFFF) flush(bound); }
            else {
                if (open) glEnd();
                glBindTexture(GL_TEXTURE_2D, t.tex);
                glBegin(GL_TRIANGLES);
                open = true;
            }
            bound = t.tex;
        }
        float mnx = std::min(t.p[0][0], std::min(t.p[1][0], t.p[2][0])), mxx = std::max(t.p[0][0], std::max(t.p[1][0], t.p[2][0]));
        float mny = std::min(t.p[0][1], std::min(t.p[1][1], t.p[2][1])), mxy = std::max(t.p[0][1], std::max(t.p[1][1], t.p[2][1]));
        float cx = (mnx + mxx) * 0.5f, cy = (mny + mxy) * 0.5f;      // the quad's centre (each of its two triangles spans the whole quad)
        float base = SurfHeightAt(cx, cy);
        float qx = cx, qy = cy;
        if (base > 0.0f) SurfClamp(qx, qy, std::min(0.45f, (mxx - mnx) * 0.5f), std::min(0.45f, (mxy - mny) * 0.5f));      // inside the furniture's outline
        float dx = qx - cx, dy = qy - cy;
        for (int l = 0; l < layers; l++) {
            float f = (float)l / (layers - 1), h = (base + 0.012f + thick * f) * us, s = (0.55f + 0.45f * f) * dark;
            if (arrays) {
                for (int v = 0; v < 3; v++) { SV q = {t.uv[v][0], t.uv[v][1], s, s, s, 1.0f, t.p[v][0] + dx, t.p[v][1] + dy, h}; verts.push_back(q); }
                continue;
            }
            glColor4f(s, s, s, 1.0f);
            for (int v = 0; v < 3; v++) { glTexCoord2f(t.uv[v][0], t.uv[v][1]); glVertex3f(t.p[v][0] + dx, t.p[v][1] + dy, h); }
        }
    }
    if (arrays && bound != 0xFFFFFFFF) flush(bound);
    if (open) glEnd();
    glPopClientAttrib();
    if (prevAB && bindAB) bindAB(0x8892, (GLuint)prevAB);
    glPopMatrix();
    glPopAttrib();
    glBindTexture(GL_TEXTURE_2D, (GLuint)prevTex);
    pActiveTexture((GLenum)act);
    glMatrixMode((GLenum)mm);
    use((GLuint)prevProg);
    static int dl = 0;
    if (dl < 3) { dl++; Log("SPRITES3D %d hidden sprite triangles drawn as extrusions", (int)Q.size()); }
}
static void DrawQueuedEquip() {
    // 1.9.111 (user: flickering shadows / hands): always the last COMPLETE frame's list - this frame's was taken whenever it had any entries, half-filled when the floor came mid-frame
    const std::vector<EquipQ>& L = (gEquipFrame == gSwapCount ? gEquipPrev : gEquipCur);
    const std::vector<ShadowQ>& S = (gShadFrame == gSwapCount ? gShadPrev : gShadCur);
    if (L.empty() && (S.empty() || !gCfg.objectShadows) && gPotCur.empty() && gPotPrev.empty()) return;
    GLint prevProg = 0, act = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &act);
    PFN_UseProgram use = oUseProgram ? oUseProgram : oUseProgramObjectARB;
    if (!use) return;
    use(0);
    pActiveTexture(GL_TEXTURE0);
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_POLYGON_BIT);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_CULL_FACE); glDisable(GL_ALPHA_TEST);
    glEnable(GL_DEPTH_TEST);
    // 1.9.127 (user: flickering shadow under bench / table): the shadows lay 0.004 tile above the floor - inside one depth-buffer step far
    // from the camera, so floor and shadow fought. They are now pulled towards the camera by a depth offset instead (any distance, any zoom)
    glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(-2.0f, -8.0f);
    const float up = (float)gCfg.upSign, dim = 1.0f - 0.55f * gCellsDark;
    // soft shadows first: a dark disc fading out, on the floor, no depth writes (people and items stand in them)
    // light from above, a little off to one side: shadows lean the other way (world +x, +y), a few hundredths of a tile
    const float offX = 0.06f, offY = 0.05f;
    if (gCfg.objectShadows && !S.empty()) {
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
        glBegin(GL_QUADS);
        for (const ShadowQ& q : S) {      // a dark core and a rim fading to nothing 0.14 tiles out
            float a = q.a * (0.55f + 0.45f * dim), h = up * (q.h0 + 0.004f), r = 0.14f;
            float x0 = q.x0 + offX, x1 = q.x1 + offX, y0 = q.y0 + offY, y1 = q.y1 + offY;
            glColor4f(0, 0, 0, a);
            glVertex3f(x0, y0, h); glVertex3f(x1, y0, h); glVertex3f(x1, y1, h); glVertex3f(x0, y1, h);
            const float in[4][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}, out[4][2] = {{x0 - r, y0 - r}, {x1 + r, y0 - r}, {x1 + r, y1 + r}, {x0 - r, y1 + r}};
            for (int e = 0; e < 4; e++) {
                int n = (e + 1) & 3;
                glColor4f(0, 0, 0, a); glVertex3f(in[e][0], in[e][1], h); glVertex3f(in[n][0], in[n][1], h);
                glColor4f(0, 0, 0, 0); glVertex3f(out[n][0], out[n][1], h); glVertex3f(out[e][0], out[e][1], h);
            }
        }
        glEnd();
        glDisable(GL_BLEND);
    }
    if (gCfg.personShadows) {      // two layers: a dark contact shadow at the feet and a wide soft one, leaning away from the light
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
        const int N = 16;
        for (const EquipQ& q : L) {
            float base = std::max(q.R, q.D), h = q.h0 + 0.006f;
            const float rads[2] = {base * 1.45f, base * 0.80f}, alph[2] = {0.20f * dim + 0.04f, 0.26f * dim + 0.06f}, off[2] = {1.0f, 0.4f};
            for (int L2 = 0; L2 < 2; L2++) {
                float cx = q.cx + offX * 2.0f * off[L2], cy = q.cy + offY * 2.0f * off[L2], rad = rads[L2];
                glBegin(GL_TRIANGLE_FAN);
                glColor4f(0, 0, 0, alph[L2]);
                glVertex3f(cx, cy, up * h);
                glColor4f(0, 0, 0, 0.0f);
                for (int i = 0; i <= N; i++) { float a = i * 6.2831853f / N; glVertex3f(cx + cosf(a) * rad, cy + sinf(a) * rad * 0.92f, up * h); }
                glEnd();
            }
        }
        glDisable(GL_BLEND);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_POLYGON_OFFSET_FILL);      // hands, cuffs and pots are solid models: no offset
    glDisable(GL_BLEND); glDisable(GL_FOG); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);      // 1.9.105 (user: hands see-through): solid, whatever the game left on
    // a point in the body frame: s = sideways (+ = model +x), f = forwards, h = height above the body's floor
    auto P = [&](const EquipQ& q, float s, float f, float h) { glVertex3f(q.cx + q.rx * s + q.fx * f, q.cy + q.ry * s + q.fy * f, up * (q.h0 + h)); };
    auto obox = [&](const EquipQ& q, float s0, float s1, float f0, float f1, float h0, float h1, float r, float g, float b) {
        const float sh[6] = {1.0f, 0.5f, 0.85f, 0.85f, 0.7f, 0.7f};
        const float C[6][4][3] = {{{s0, f0, h1}, {s1, f0, h1}, {s1, f1, h1}, {s0, f1, h1}}, {{s0, f0, h0}, {s0, f1, h0}, {s1, f1, h0}, {s1, f0, h0}},
                                  {{s0, f1, h0}, {s0, f1, h1}, {s1, f1, h1}, {s1, f1, h0}}, {{s0, f0, h0}, {s1, f0, h0}, {s1, f0, h1}, {s0, f0, h1}},
                                  {{s1, f0, h0}, {s1, f1, h0}, {s1, f1, h1}, {s1, f0, h1}}, {{s0, f0, h0}, {s0, f0, h1}, {s0, f1, h1}, {s0, f1, h0}}};
        for (int k = 0; k < 6; k++) { glColor3f(r * sh[k] * dim, g * sh[k] * dim, b * sh[k] * dim); for (int v = 0; v < 4; v++) P(q, C[k][v][0], C[k][v][1], C[k][v][2]); }
    };
    // a round hand: a small low-poly ball, lit from above
    auto ball = [&](const EquipQ& q, float s, float f, float h, float rad, uint32_t col) {
        float r = ((col >> 16) & 255) / 255.0f, g = ((col >> 8) & 255) / 255.0f, b = (col & 255) / 255.0f;
        const int SL = 8, ST = 5;
        for (int j = 0; j < ST; j++) {
            float t0 = 3.1415927f * j / ST - 1.5707963f, t1 = 3.1415927f * (j + 1) / ST - 1.5707963f;
            float shade = (0.62f + 0.38f * (sinf((t0 + t1) * 0.5f) * 0.5f + 0.5f)) * dim;
            glColor3f(r * shade, g * shade, b * shade);
            for (int i = 0; i < SL; i++) {
                float a0 = 6.2831853f * i / SL, a1 = 6.2831853f * (i + 1) / SL;
                P(q, s + rad * cosf(t0) * cosf(a0), f + rad * cosf(t0) * sinf(a0), h + rad * sinf(t0));
                P(q, s + rad * cosf(t0) * cosf(a1), f + rad * cosf(t0) * sinf(a1), h + rad * sinf(t0));
                P(q, s + rad * cosf(t1) * cosf(a1), f + rad * cosf(t1) * sinf(a1), h + rad * sinf(t1));
                P(q, s + rad * cosf(t1) * cosf(a0), f + rad * cosf(t1) * sinf(a0), h + rad * sinf(t1));
            }
        }
    };
    glBegin(GL_QUADS);
    for (const EquipQ& q : L) {
        float H = q.h1 - q.h0, R = q.R, D = q.D, hr = std::max(0.075f, R * 0.34f);      // 1.9.100: bigger (user: no hands / cuffs - they were ~0.05 tile, mostly in the body)
        if (q.cuffs) {      // 1.9.89 (user: improve handcuffs): hands together in front, a round steel cuff around each wrist and a linked chain
            float h = H * 0.36f, f = D * 1.1f + hr * 0.7f, sx = hr * 1.25f;      // clear of the belly
            if (gCfg.showHands) { ball(q, -sx, f + hr * 0.35f, h, hr, q.skin); ball(q, sx, f + hr * 0.35f, h, hr, q.skin); }
            // a ring around the wrist (its axis forwards): 12 segments x 4 sides, lit from above
            auto ring = [&](float sc, float fc, float hc, float rr, float rt, float cr, float cg, float cb) {
                const int SU = 12, SV = 4;
                for (int i = 0; i < SU; i++) {
                    float u0 = 6.2831853f * i / SU, u1 = 6.2831853f * (i + 1) / SU;
                    float sh = (0.62f + 0.38f * (sinf((u0 + u1) * 0.5f) * 0.5f + 0.5f)) * dim;
                    for (int j = 0; j < SV; j++) {
                        float v0 = 6.2831853f * j / SV, v1 = 6.2831853f * (j + 1) / SV;
                        float sj = sh * (0.85f + 0.15f * cosf((v0 + v1) * 0.5f));
                        glColor3f(cr * sj, cg * sj, cb * sj);
                        P(q, sc + (rr + rt * cosf(v0)) * cosf(u0), fc + rt * sinf(v0), hc + (rr + rt * cosf(v0)) * sinf(u0));
                        P(q, sc + (rr + rt * cosf(v0)) * cosf(u1), fc + rt * sinf(v0), hc + (rr + rt * cosf(v0)) * sinf(u1));
                        P(q, sc + (rr + rt * cosf(v1)) * cosf(u1), fc + rt * sinf(v1), hc + (rr + rt * cosf(v1)) * sinf(u1));
                        P(q, sc + (rr + rt * cosf(v1)) * cosf(u0), fc + rt * sinf(v1), hc + (rr + rt * cosf(v1)) * sinf(u0));
                    }
                }
            };
            float wf = f - hr * 0.55f, rr = hr * 0.72f, rt = hr * 0.2f;
            for (int s = -1; s <= 1; s += 2) {
                ring(s * sx, wf, h, rr, rt, 0.82f, 0.84f, 0.88f);                                                                    // the cuff
                obox(q, s * sx - rt * 0.6f, s * sx + rt * 0.6f, wf - rt, wf + rt, h - rr - rt * 2.2f, h - rr - rt * 0.6f, 0.30f, 0.31f, 0.34f);   // its lock box underneath
            }
            for (int k = 0; k < 3; k++) {      // three chain links between the cuffs, alternately flat and upright
                float a = -sx + rr * 0.9f + (2.0f * sx - rr * 1.8f) * (k + 0.5f) / 3.0f, lw = (2.0f * sx - rr * 1.8f) / 3.0f * 0.55f;
                if (k % 2 == 0) obox(q, a - lw, a + lw, wf - rt * 0.7f, wf + rt * 0.7f, h - rr * 0.95f - rt * 0.35f, h - rr * 0.95f + rt * 0.35f, 0.62f, 0.64f, 0.68f);
                else obox(q, a - lw, a + lw, wf - rt * 0.3f, wf + rt * 0.3f, h - rr * 0.95f - rt * 0.8f, h - rr * 0.95f + rt * 0.8f, 0.62f, 0.64f, 0.68f);
            }
        } else if (q.gun) {      // 1.9.104 (user: it's a shotgun): held across the body at port arms, stock low on the right, muzzle up past the left shoulder
            // 1.9.105 (user: gun is strange - the stepped blocks came apart): each part is one box along the gun's own slant
            const float fz = D * 1.05f + hr * 0.5f;      // just in front of the chest
            const float sR = R * 1.0f, hR = H * 0.34f, sL = -R * 1.25f, hL = H * 0.60f;      // stock end (right hip) -> muzzle (left shoulder)
            const float ls = sL - sR, lh = hL - hR, len = sqrtf(ls * ls + lh * lh), us = ls / len, uh = lh / len, ps = -uh, ph = us;      // along / across (in s-h)
            auto bar = [&](float t0, float t1, float off, float th, float dep, float r, float g, float b) {      // from t0 to t1 of the length, off across, half thickness th
                float c0s = sR + us * len * t0 + ps * off, c0h = hR + uh * len * t0 + ph * off, c1s = sR + us * len * t1 + ps * off, c1h = hR + uh * len * t1 + ph * off;
                const float cs[4] = {c0s - ps * th, c1s - ps * th, c1s + ps * th, c0s + ps * th}, ch[4] = {c0h - ph * th, c1h - ph * th, c1h + ph * th, c0h + ph * th};
                const float f0 = fz - dep, f1 = fz + dep;
                glColor3f(r * dim, g * dim, b * dim);      // front
                for (int i = 0; i < 4; i++) P(q, cs[i], f1, ch[i]);
                glColor3f(r * 0.6f * dim, g * 0.6f * dim, b * 0.6f * dim);      // back
                for (int i = 3; i >= 0; i--) P(q, cs[i], f0, ch[i]);
                for (int e = 0; e < 4; e++) {      // the four long / end sides
                    int n = (e + 1) & 3; float sh = (e == 2) ? 0.95f : (e == 0 ? 0.55f : 0.75f);
                    glColor3f(r * sh * dim, g * sh * dim, b * sh * dim);
                    P(q, cs[e], f0, ch[e]); P(q, cs[n], f0, ch[n]); P(q, cs[n], f1, ch[n]); P(q, cs[e], f1, ch[e]);
                }
            };
            const float g = hr * 0.30f;
            bar(0.00f, 0.30f, -g * 0.3f, g * 1.25f, g * 0.9f, 0.46f, 0.28f, 0.14f);      // wooden stock, deeper than the rest
            bar(0.28f, 0.42f, 0.0f, g * 0.95f, g * 0.85f, 0.20f, 0.20f, 0.22f);           // receiver
            bar(0.40f, 1.00f, g * 0.45f, g * 0.38f, g * 0.45f, 0.10f, 0.10f, 0.11f);       // barrel
            bar(0.40f, 0.94f, -g * 0.35f, g * 0.38f, g * 0.45f, 0.14f, 0.14f, 0.15f);      // magazine tube under it
            bar(0.55f, 0.75f, -g * 0.25f, g * 0.75f, g * 0.75f, 0.50f, 0.31f, 0.16f);      // wooden pump
            if (gCfg.showHands) {      // right hand on the grip, left on the pump
                ball(q, sR + us * len * 0.30f - ps * g * 0.6f, fz + g + hr * 0.2f, hR + uh * len * 0.30f - ph * g * 0.6f, hr, q.skin);
                ball(q, sR + us * len * 0.65f - ps * g * 0.6f, fz + g + hr * 0.2f, hR + uh * len * 0.65f - ph * g * 0.6f, hr, q.skin);
            }
        } else if (gCfg.showHands) {      // hands at the sides, a little forward
            ball(q, -R * 1.02f - hr * 0.3f, D * 0.3f, H * 0.32f, hr, q.skin);      // at the sides, just outside the body
            ball(q, R * 1.02f + hr * 0.3f, D * 0.3f, H * 0.32f, hr, q.skin);
        }
        // 1.9.104 (user: improve taser and baton): a round truncheon and a holstered taser on a belt, both behind the hands so they show
        auto cyl = [&](float sc, float fc, float h0, float h1, float rad, float r, float g, float b) {      // upright 8-sided rod, lit from one side
            for (int i = 0; i < 8; i++) {
                float a0 = 6.2831853f * i / 8, a1 = 6.2831853f * (i + 1) / 8, sh = (0.55f + 0.45f * (0.5f + 0.5f * cosf((a0 + a1) * 0.5f - 0.8f))) * dim;
                glColor3f(r * sh, g * sh, b * sh);
                P(q, sc + rad * cosf(a0), fc + rad * sinf(a0), h0); P(q, sc + rad * cosf(a1), fc + rad * sinf(a1), h0);
                P(q, sc + rad * cosf(a1), fc + rad * sinf(a1), h1); P(q, sc + rad * cosf(a0), fc + rad * sinf(a0), h1);
            }
            glColor3f(r * dim, g * dim, b * dim);      // flat caps as two crossing quads' worth: a square top is enough at this size
            P(q, sc - rad * 0.7f, fc - rad * 0.7f, h1); P(q, sc + rad * 0.7f, fc - rad * 0.7f, h1); P(q, sc + rad * 0.7f, fc + rad * 0.7f, h1); P(q, sc - rad * 0.7f, fc + rad * 0.7f, h1);
        };
        if (q.baton || q.tazer) {      // the belt: a dark band showing at the sides and front
            float bh0 = H * 0.30f, bh1 = H * 0.36f;
            obox(q, -R * 1.0f, R * 1.0f, D * 0.55f, D * 1.0f, bh0, bh1, 0.12f, 0.11f, 0.10f);
            obox(q, -hr * 0.35f, hr * 0.35f, D * 1.0f, D * 1.0f + hr * 0.12f, bh0 - hr * 0.05f, bh1 + hr * 0.05f, 0.75f, 0.70f, 0.45f);      // buckle
        }
        if (q.baton) {      // right hip, slung slightly back: black rod with a ribbed grip, a side handle and a pommel
            float sc = R * 1.0f + hr * 0.22f, fc = -D * 0.25f, rd = hr * 0.17f;
            cyl(sc, fc, H * 0.02f, H * 0.30f, rd, 0.09f, 0.09f, 0.10f);
            cyl(sc, fc, H * 0.30f, H * 0.46f, rd * 1.12f, 0.20f, 0.20f, 0.22f);      // grip
            cyl(sc, fc, H * 0.46f, H * 0.48f, rd * 1.45f, 0.09f, 0.09f, 0.10f);      // pommel
            obox(q, sc - rd * 0.6f, sc + rd * 0.6f, fc, fc + hr * 0.75f, H * 0.33f, H * 0.33f + rd * 1.2f, 0.09f, 0.09f, 0.10f);      // side handle
            obox(q, sc - rd * 1.3f, sc + rd * 1.3f, fc - rd * 1.3f, fc + rd * 1.3f, H * 0.28f, H * 0.31f, 0.35f, 0.35f, 0.37f);      // belt ring
        }
        if (q.tazer) {      // left hip: a black holster with the yellow-striped taser grip sticking out of the top, two steel prongs
            float s0 = -R * 1.0f - hr * 0.7f, s1 = -R * 0.96f, f0 = -D * 0.3f - hr * 0.45f, f1 = -D * 0.3f + hr * 0.45f;
            obox(q, s0, s1, f0, f1, H * 0.12f, H * 0.33f, 0.08f, 0.08f, 0.09f);                                           // holster
            obox(q, s0 + hr * 0.08f, s1 - hr * 0.08f, f0 + hr * 0.1f, f1 - hr * 0.1f, H * 0.33f, H * 0.45f, 0.95f, 0.78f, 0.12f);      // yellow grip
            for (int k = 0; k < 2; k++) {      // black stripes on the grip
                float hs = H * (0.355f + 0.04f * k);
                obox(q, s0 + hr * 0.06f, s1 - hr * 0.06f, f0 + hr * 0.08f, f1 - hr * 0.08f, hs, hs + H * 0.015f, 0.08f, 0.08f, 0.09f);
            }
            for (int k = -1; k <= 1; k += 2)      // prongs at the bottom
                obox(q, (s0 + s1) * 0.5f - hr * 0.05f, (s0 + s1) * 0.5f + hr * 0.05f, (f0 + f1) * 0.5f + k * hr * 0.2f - hr * 0.04f, (f0 + f1) * 0.5f + k * hr * 0.2f + hr * 0.04f,
                     H * 0.08f, H * 0.12f, 0.70f, 0.72f, 0.75f);
        }
    }
    glEnd();
    {      // 1.9.106: pans of food ("ingredients") as round steel pots with handles, the food filling the top
        const std::vector<PotQ>& PL = (gPotFrame == gSwapCount ? gPotPrev : gPotCur);
        const int SEG = 16;
        glBegin(GL_QUADS);
        for (const PotQ& p : PL) {
            const float R0 = 0.20f, Hh = 0.11f, h0 = p.h + 0.002f, h1 = h0 + Hh, rim = 0.018f;
            auto V = [&](float a, float r, float h) { glVertex3f(p.x + cosf(a) * r, p.y + sinf(a) * r, up * h); };
            for (int i = 0; i < SEG; i++) {
                float a0 = 6.2831853f * i / SEG, a1 = 6.2831853f * (i + 1) / SEG, sh = (0.55f + 0.45f * (0.5f + 0.5f * cosf((a0 + a1) * 0.5f - 0.7f))) * dim;
                glColor3f(0.42f * sh, 0.43f * sh, 0.46f * sh);      // the side, slightly tapered
                V(a0, R0 * 0.92f, h0); V(a1, R0 * 0.92f, h0); V(a1, R0, h1); V(a0, R0, h1);
                glColor3f(0.70f * dim, 0.71f * dim, 0.74f * dim);      // the rolled rim
                V(a0, R0, h1); V(a1, R0, h1); V(a1, R0 - rim, h1); V(a0, R0 - rim, h1);
                float fs = (0.85f + 0.15f * sinf(a0 * 3.0f)) * dim;      // the food, a little below the rim, lumpy in colour
                glColor3f(0.36f * fs, 0.58f * fs, 0.20f * fs);
                V(a0, R0 - rim, h1 - 0.015f); V(a1, R0 - rim, h1 - 0.015f); V(a1, 0.0f, h1 - 0.008f); V(a0, 0.0f, h1 - 0.008f);
            }
            for (int s = -1; s <= 1; s += 2) {      // two loop handles
                float cx = p.x + s * (R0 + 0.035f), hh = h1 - 0.02f;
                glColor3f(0.25f * dim, 0.25f * dim, 0.27f * dim);
                glVertex3f(cx - 0.035f, p.y - 0.05f, up * (hh + 0.012f)); glVertex3f(cx + 0.035f, p.y - 0.05f, up * (hh + 0.012f));
                glVertex3f(cx + 0.035f, p.y + 0.05f, up * (hh + 0.012f)); glVertex3f(cx - 0.035f, p.y + 0.05f, up * (hh + 0.012f));
            }
        }
        glEnd();
    }
    glPopAttrib();
    pActiveTexture((GLenum)act);
    use((GLuint)prevProg);
}
// glMatrixMode (the game's): leaving or entering projection mode means the projection may change - GetPR reads it again then
typedef void(APIENTRY* PFN_MatrixMode)(GLenum);
static PFN_MatrixMode oMatrixMode = nullptr;
static void APIENTRY hkMatrixMode(GLenum m) {
    if (m == GL_PROJECTION || gMatMode == GL_PROJECTION) gProjGen++;
    gMatMode = m;
    oMatrixMode(m);
}
// 1.9.125: the game's matrix calls mark the modelview as changed (GetMV reads it back only then)
typedef void(APIENTRY* PFN_Void)(void);
typedef void(APIENTRY* PFN_Mat16f)(const GLfloat*);
typedef void(APIENTRY* PFN_Vec3f)(GLfloat, GLfloat, GLfloat);
static PFN_Void oLoadIdentity = nullptr, oPopMatrix = nullptr;
static PFN_Mat16f oMultMatrixf = nullptr;
static PFN_Vec3f oTranslatef = nullptr, oScalef = nullptr;
static void APIENTRY hkLoadIdentity() { gMvGen++; oLoadIdentity(); }
static void APIENTRY hkPopMatrix() { gMvGen++; oPopMatrix(); }
static void APIENTRY hkMultMatrixf(const GLfloat* m) { gMvGen++; oMultMatrixf(m); }
static void APIENTRY hkTranslatef(GLfloat x, GLfloat y, GLfloat z) { gMvGen++; oTranslatef(x, y, z); }
static void APIENTRY hkScalef(GLfloat x, GLfloat y, GLfloat z) { gMvGen++; oScalef(x, y, z); }
// Early cull self-check (1.9.83): at the first draw after each note, does the position read at the note match where the object is
// really drawn (its model origin)? Per draw path. Turned on after 4000 checks with under 1 % misses; off again if misses pass 2 %.
static void EarlyCullCheck(GLsizei c) {
    {      // 1.9.85 diagnostic: path 1 never finished its search on the big save - why? every 15 s until both paths are searched
        static unsigned seen[3][4] = {{0}}; static double nextT = 0; static int lines = 0;
        int pp = gNotePath;
        seen[pp][0]++;
        if (gCalNote == gNoteSerial) seen[pp][1]++; else if ((unsigned)(gDrawSerial - gNoteSerial) > 1) seen[pp][2]++; else if (c < 12 || gInWallPass) seen[pp][3]++;
        double now = NowSec();
        if (nextT == 0) nextT = now + 15;
        if (now >= nextT && lines < 8 && (gPosSamples[1] < 3000 || gPosSamples[2] < 3000)) {
            nextT = now + 15; lines++;
            Log("EARLYCULL draws by path (none / B1 / C2): %u / %u / %u; B1: already checked %u, not the note's first draw %u, small or wall %u, samples %u; C2 samples %u",
                seen[0][0], seen[1][0], seen[2][0], seen[1][1], seen[1][2], seen[1][3], gPosSamples[1], gPosSamples[2]);
        }
    }
    if (!gNotePath || gCalNote == gNoteSerial || gInWallPass || !gCam.active || c < 12) return;
    if (!gNotePosOk && gPosSamples[gNotePath] >= 3000) return;
    if ((unsigned)(gDrawSerial - gNoteSerial) > 1) return;
    gCalNote = gNoteSerial;
    GLdouble mv[16];
    GetMV(mv);
    bool cam = true;
    for (int i = 0; i < 16; i++) if (fabs(mv[i] - gCamMV[i]) > 1e-6) { cam = false; break; }
    if (cam) return;      // world-space vertices: the origin is not the object
    double ox = MVWorldX(mv);
    double oy = MVWorldY(mv);
    if (fabs(ox) < 0.5 && fabs(oy) < 0.5) return;
    int p = gNotePath;
    if (gPosSamples[p] < 3000) {      // still searching: which register + offset holds a float pair equal to the drawn origin?
        gPosSamples[p]++;
        for (int r = 0; r < 15; r++) {
            uint8_t* q = (uint8_t*)gNoteRegs[r];
            if ((uintptr_t)q < 0x10000 || (uintptr_t)q >= 0x7FFFFFFF0000ull || ((uintptr_t)q & 3) || !Readable(q, 0x200)) continue;
            for (int k = 0; k < 127; k++) {
                float x = *(float*)(q + k * 4), y = *(float*)(q + k * 4 + 4);
                if (fabs(x - ox) < 0.6 && fabs(y - oy) < 0.6) gPosVotes[p][r][k]++;
            }
        }
        if (gPosSamples[p] == 3000) {
            unsigned best = 0, second = 0; int br = -1, bk = 0;
            for (int r = 0; r < 15; r++)
                for (int k = 0; k < 127; k++) {
                    unsigned v = gPosVotes[p][r][k];
                    if (v > best) { second = best; best = v; br = r; bk = k; } else if (v > second) second = v;
                }
            static const char* rn[] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"};
            if (br >= 0 && best >= 3000 * 0.6) { gPosReg[p] = br; gPosOff[p] = bk * 4; }
            Log("EARLYCULL path %d: position search over 3000 draws: best %s+0x%x matched %u (next best %u) -> %s", p, br >= 0 ? rn[br] : "-", bk * 4, best, second,
                gPosReg[p] >= 0 ? "checking it" : "none good enough: early cull stays off on this path");
        }
        return;
    }
    double d = hypot(ox - gNotePos[0], oy - gNotePos[1]);
    if (d < 1.0) gEarlyAgree[p]++; else gEarlyDisagree[p]++;
    if (gNoteES && gNotePosOk) {      // 1.9.107: this object type's own record (a type that ever misses is never culled early)
        if (d < 2.5) { if (gNoteES->ok < 60000) gNoteES->ok++; }      // 1.9.121 (fps 42 -> 15): within 2.5 tiles is right enough - the cull keeps 3.5 tiles of margin
        else if (gNoteES->bad < 60000) {
            gNoteES->bad++;
            static int bl = 0;
            if (bl < 20) { bl++; Log("EARLYCULL a type drawn %.1f tiles from its note's position: never culled early (%u good before)", d, gNoteES->ok); }
        }
    }
    unsigned n = gEarlyAgree[p] + gEarlyDisagree[p];
    if (n >= 4000) {
        double miss = (double)gEarlyDisagree[p] / n;
        bool was = gEarlyOn[p];
        if (!was && miss < 0.01) gEarlyOn[p] = true;
        else if (was && miss > 0.02) gEarlyOn[p] = false;
        static int ll = 0;
        if (ll < 20 && (was != gEarlyOn[p] || ll < 4)) { ll++; Log("EARLYCULL path %d: position at the note matched %u of %u draws (%.2f %% off) -> early cull %s; %u objects skipped so far", p, gEarlyAgree[p], n, miss * 100.0, gEarlyOn[p] ? "ON" : "off", gEarlyCulled); }
        gEarlyAgree[p] = gEarlyDisagree[p] = 0;
    }
}
static void APIENTRY hkDrawArrays(GLenum m, GLint f, GLsizei c) {
    // 1.9.123: the whole hook's time (with the game's own GL call) in sampled frames - PERF 'hook'
    struct HookTimer { double t0; ~HookTimer() { if (t0 > 0) { gPerf.t[8] += PerfNow() - t0; gPerf.draws++; } } } hookTimer{gPerfOn ? PerfNow() : 0.0};
    if (gTintProg || gHeadBound) ResetPrisonerUniforms();      // left set by a draw that returned early
    gDrawSerial++;
    gDrawTick++;
    EarlyCullCheck(c);
    if (gInWallPass && c >= 12 && gWallHFrame != gSwapCount) {      // 1.9.100: how tall a wall piece is in the world (doors are sized to it)
        gWallHFrame = gSwapCount;
        GLdouble mvw[16];
        GetMV(mvw);
        double s = sqrt(mvw[4] * mvw[4] + mvw[5] * mvw[5] + mvw[6] * mvw[6]);
        if (s > 1e-4 && s < 10.0) gWallWorldH = (float)(1.978 * s);
    }
    if (paiso_tree_note && !gInWallPass && (unsigned)(gDrawSerial - gNoteSerial) <= 16) {      // which draws a tree's note is followed by (finds the man if it is not 3003)
        static std::set<int> seen;
        if (seen.size() < 12 && seen.insert((int)c).second) Log("TREEDRAW a tree's note is followed by a draw of %d vertices (path %d)", (int)c, gNotePath);
    }
    if ((c == 3003 || c == 5634) && !gObjPersonExt && !gInWallPass && gCam.active) {      // 1.9.103: the accountant is 5634 (TREEDRAW: a tree's note, then 5634)      // 1.9.100: a person-sized model (1001 faces: the accountant and every
        static int tl = 0;                                                  // other 1001-face person) drawn for something that is NOT a person - the
        if (tl < 8) { tl++; Log("TREEMAN 3003-vertex person model drawn for a non-person note (tree note %d, path %d, %u draws after it): skipped",      // bare trees'
                                paiso_tree_note, gNotePath, gDrawSerial - gNoteSerial); }      // men were not drawn during the tree's note (1.9.92 missed them)
        return;
    }
    if (!(gInWallPass && gWbCapture) && PERF(6, CullOffscreen(c))) {      // 1.9.130: a recorded wall pass keeps every piece
        if (gInWallPass) {
            GLdouble mv[16];
            GetMV(mv);
            double ox = MVWorldX(mv),
                   oy = MVWorldY(mv);
            gDrawnCur.insert((int)floor(oy) * 65536 + (int)floor(ox));
        }
        return;
    }      // off screen (wall pieces still count as walls)
    if (gObjTV && !gInWallPass) {
        static std::set<int> seenA;
        if (seenA.size() < 12 && seenA.insert((int)c).second)
            Log("TVPATH arrays count %d serial gap %u prog %u", (int)c, gDrawSerial - gNoteSerial, gCurProg);
    }
    FlushOverlaysIfUi();
    if (gCfg.deferOverlays && gCfg.deferArrays && !gCurProg && !gInWallPass && gCam.active && !gDeferRec && m == GL_TRIANGLES && c > 0 && c <= 400 &&
        gDeferred.size() < 96 && glIsEnabled(GL_BLEND) && !glIsEnabled(GL_DEPTH_TEST) && (int)gBound[0] != gCfg.bubbleTexA &&
        (int)gBound[0] != gCfg.bubbleTexB) {      // the beacon's halo / icon: a flat blended array draw made under the 3D camera, replayed on top
        GLdouble P[16];
        GetPR(P);
        bool cam3d = true;
        for (int q = 0; q < 16; q++)
            if (fabs(P[q] - gCamP[q]) > 1e-9) {
                cam3d = false;
                break;
            }
        if (cam3d) {
            DeferredOverlay d;
            glGetFloatv(GL_CURRENT_COLOR, d.col);
            d.tex = gBound[0];
            glGetIntegerv(0x0BE1, &d.src);
            glGetIntegerv(0x0BE0, &d.dst);
            d.tex2d = glIsEnabled(GL_TEXTURE_2D);
            glGetDoublev(GL_MODELVIEW_MATRIX, d.mv);
            memcpy(d.pr, P, sizeof P);
            d.list = glGenLists(1);
            if (d.list) {
                glNewList(d.list, GL_COMPILE);
                oDrawArrays(m, f, c);
                glEndList();
                gDeferred.push_back(d);
                {
                    static int dl = 0;
                    if (gTransFramesLeft > 0 && dl < 80) {
                        dl++;
                        Log("DEFERRED array draw count %d tex %u colour %.2f %.2f %.2f %.2f", (int)c, d.tex, d.col[0], d.col[1], d.col[2], d.col[3]);
                    }
                }
                return;
            }
        }
    }
    TransDiag("arrays", m, f, c);
    if (gObjBench && (unsigned)(gDrawSerial - gNoteSerial) <= 6) {
        static std::set<unsigned long long> seenBD;
        unsigned long long bk = ((unsigned long long)c << 32) ^ ((unsigned long long)gCurProg << 8) ^ gBound[0];
        if (seenBD.size() < 40 && seenBD.insert(bk).second)
            Log("BENCHDRAW %s mode 0x%x count %d prog %d tex0 %u inwall %d", gBenchName, (unsigned)m, (int)c, (int)gCurProg, gBound[0],
                (int)gInWallPass);
    }
    UiTraceArm();
    bool ui = false;
    if (gUiTrace && !gInWallPass) {
        GLdouble P[16];
        GetPR(P);
        for (int i = 0; i < 16; i++)
            if (fabs(P[i] - gCamP[i]) > 1e-9) {
                ui = true;
                break;
            }
    }
    NearCellDiag(m, f, c);
    ModelDrawDiag("arrays", m, c);
    NoteOutdoors();
    DrawSurvey("arrays", m, c);
    if (gTilesetTex && gBound[0] == gTilesetTex && !gInWallPass) CaptureTerrain(f, c);
    FloorHook();
    if (gCurProg && gPatchedPrograms.count(gCurProg) && m == GL_TRIANGLES && c > 1500 && gCam.active && !gInWallPass) DumpCellLayer(f, c);
    if (!gInWallPass) ObjectLightAndShopFront(m, f, c);
    if (gCurProg && pUniform1f) {      // cells' lights (Sleep regime): only set when the value changed for this program
        auto lc = gLocs.find(gCurProg);
        if (lc != gLocs.end() && lc->second.ucells >= 0 && lc->second.lastCells != gCellsDark) { pUniform1f(lc->second.ucells, gCellsDark); lc->second.lastCells = gCellsDark; }
    }
    bool tinted = false;
    if (!gInWallPass && gObjPrisoner && gCfg.uniformTints && gCurProg && (unsigned)(gDrawSerial - gNoteSerial) <= 16 && pUniform4f) {
        auto lt = gLocs.find(gCurProg);
        if (lt == gLocs.end() || lt->second.utint < 0) {
            gPrisStat[5]++;
            static std::set<unsigned> np;
            if (np.size() < 8 && np.insert(gCurProg).second) Log("PRISSTAT prisoner drawn with program %u, which has no tint uniform", gCurProg);
        }
        if (lt != gLocs.end() && lt->second.utint >= 0) {      // this prisoner's own uniform colour, skin, hair and head (all read from the prisoner object in paiso_note)
            auto rgb = [&](GLint loc, uint32_t c, bool lowR) {      // the colour table keeps R in the low byte; skin / hair are 0x01RRGGBB
                float r = lowR ? (c & 255) / 255.0f : ((c >> 16) & 255) / 255.0f, b = lowR ? ((c >> 16) & 255) / 255.0f : (c & 255) / 255.0f;
                pUniform4f(loc, r, ((c >> 8) & 255) / 255.0f, b, 1.0f);
                tinted = true;
            };
            if (gPrisTint) rgb(lt->second.utint, gPrisTint, true);
            if (gPrisSkin && lt->second.uskin >= 0) rgb(lt->second.uskin, gPrisSkin, false);
            if (gPrisHair && lt->second.uhair >= 0) rgb(lt->second.uhair, gPrisHair, false);
            if (gPrisHead >= 0 && gCfg.prisonerHeads && lt->second.uheadgeo >= 0 && lt->second.uheads >= 0 && pUniform1i &&
                HeadOverlaySet(lt->second.uheads, lt->second.uheadgeo, lt->second.uheadcell, f, c)) {
                tinted = true;
                gHeadBound = true;
            }
        }
    }      // logged only: the security colour comes from the prisoner object (gPrisTint)
    if (tinted) gTintProg = gCurProg;      // reset by ResetPrisonerUniforms, whichever way this draw ends
    if (!gInWallPass && gCam.active && gCurProg && c >= 30 && (unsigned)(gDrawSerial - gNoteSerial) <= 16) {      // remember where 3D models stand
        auto lit = gLocs.find(gCurProg);
        if (lit != gLocs.end() && lit->second.wpass >= 0) {
            GLdouble mv[16];
            GetMV(mv);
            double ox = MVWorldX(mv),
                   oy = MVWorldY(mv);
            int hx = (int)floor(ox * 2.0), hy = (int)floor(oy * 2.0);
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    gModelTilesCur.insert((hy + dy) * 65536 + (hx + dx) + 0x40000000);
                    if (gObjPersonExt) gPersonSeen[(hy + dy) * 65536 + (hx + dx)] = gWallFramesSeenExt;
                    else if (!gObjSurface) gThingTilesCur.insert((hy + dy) * 65536 + (hx + dx));      // 1.9.87: a stack / crate model stands here (1.9.90: and the half-tiles round it)
                }
        }
    }
    // things on furniture: tables record their top, flat trays / meals on them are lifted to it (popped whichever way this draw ends)
    struct LiftPop { bool on; ~LiftPop() { if (on) glPopMatrix(); } } liftPop{false};
    if (gCfg.liftOnFurniture && !gInWallPass && gCam.active && (unsigned)(gDrawSerial - gNoteSerial) <= 16) {
        // each only on the noted object's own first draw: the 16-draw note window also covers the objects drawn after it
        if (gObjSurface && c >= 30) { if (gSurfNote != gNoteSerial) { gSurfNote = gNoteSerial; SurfaceRecord(f, c); } }
        else if (gObjLift && gLiftNote != gNoteSerial) {
            gLiftNote = gNoteSerial;
            double d = SurfaceLift();
            GLint mm = 0;
            if (d > 0) glGetIntegerv(GL_MATRIX_MODE, &mm);
            if (d > 0 && mm == GL_MODELVIEW) {
                GLdouble mv[16];
                GetMV(mv);
                for (int i = 0; i < 3; i++) mv[12 + i] += gViewInv[i * 4 + 2] * gCfg.upSign * d;
                {      // 1.9.77: and inside the furniture's outline (a wall-side table stands half a tile off its tiles)
                    float ox = (float)(MVWorldX(mv));
                    float oy = (float)(MVWorldY(mv));
                    float qx = ox, qy = oy;
                    if (SurfClamp(qx, qy, 0.35f, 0.35f))
                        for (int i = 0; i < 3; i++) mv[12 + i] += gViewInv[i * 4 + 0] * (qx - ox) + gViewInv[i * 4 + 1] * (qy - oy);
                }
                glPushMatrix();
                glLoadMatrixd(mv);
                liftPop.on = true;
            }
        }
    }
    if (gCfg.seatDrop > 0.0f && gObjPersonExt && gPersonNote == gNoteSerial && !liftPop.on && !gInWallPass && gCam.active && m == GL_TRIANGLES && c >= 12 &&
        gMatMode == GL_MODELVIEW && (unsigned)(gDrawSerial - gNoteSerial) <= 16) {
        const double d = SeatDrop();
        if (d > 0) {
            GLdouble mv[16];
            GetMV(mv);
            for (int i = 0; i < 3; i++) mv[12 + i] -= gViewInv[i * 4 + 2] * gCfg.upSign * d;
            glPushMatrix();
            glLoadMatrixd(mv);
            liftPop.on = true;      // the hands, cuffs and weapons recorded below come down with the body
        }
    }
    if (gCfg.objectShadows && !gInWallPass && gCam.active && c >= 30 && (unsigned)(gDrawSerial - gNoteSerial) <= 16 && !gObjPersonExt && !gObjLift &&
        !gObjDoor && !(c >= 400 && (unsigned)(gDrawSerial - gPersonNote) <= 24))      // not a body drawn after its baton / cuffs note
        ObjShadowRecord(f, c);
    if (gObjLight && gCfg.bulbPools && !gInWallPass && gCam.active && c >= 12 && (unsigned)(gDrawSerial - gNoteSerial) <= 2) BulbRecord();      // the lamp's own draw comes right after its note
    if (gObjDetector && !gInWallPass && !liftPop.on && gCam.active && c >= 12) {      // 1.9.93: metal detectors as thick as the walls; 1.9.95: as wide as
        GLint mm = 0;                                                                         // their opening, and cut down with the walls
        glGetIntegerv(GL_MATRIX_MODE, &mm);
        if (mm == GL_MODELVIEW) {
            GLdouble mv[16];
            GetMV(mv);
            double fitX = 1.0, cutY = 1.0;
            if (const ArrBox* db = ArrBounds(f, c)) {
                float w = db->hi[0] - db->lo[0];
                double widthW = w * sqrt(mv[0] * mv[0] + mv[1] * mv[1] + mv[2] * mv[2]);
                const double opening = 1.0;      // 1.9.114 (user: metal detectors too wide): one tile - the stock model is 2 tiles wide and was read as a 2-tile opening
                if (widthW > opening * 0.97 && widthW < 3.5) fitX = opening * 0.96 / widthW;
            }
            double cx = MVWorldX(mv),
                   cy = MVWorldY(mv);
            if (DoorCutNow()) cutY = gCfg.doorCutScale;      // 1.9.127 (user): metal detectors follow the doors - up in cutaway, down with the walls
            MarkDoorTiles(cx, cy);      // 1.9.106: the walls beside it run to its tile edge

            double dk = std::min(1.0, DoorDepthScale());      // 1.9.107 (user: metal detectors broken): thinner with thin walls, never deeper - thick walls made its crossbar a table top
            if (fitX != 1.0 || cutY != 1.0 || dk != 1.0) {
                glPushMatrix();
                glScaled(fitX, cutY, dk);
                liftPop.on = true;
                gDrawTick++;
            }
        }
    }
    // 1.9.103 (user: oversized items): the stock stack models (wood, logs, plates...) are ~2 tiles across; a stack fills one tile
    // 1.9.106 (user: wood is huge): stacks are "lift" objects too, so liftPop was already on and this never ran - it now scales inside that push;
    // and when the vertices cannot be read, the oversized stock stacks (wood, logs, plates, sheet metal) are known by their vertex counts
    if (gObjStack && !gInWallPass && gCam.active && c >= 12 && (unsigned)(gDrawSerial - gNoteSerial) <= 16) {
        GLint mm = 0;
        glGetIntegerv(GL_MATRIX_MODE, &mm);
        static const int kBigStack[] = {2232, 1488, 744, 3168, 1518, 660, 210, 120, 60, 1020, 540, 150};
        bool known = false;
        for (int k : kBigStack) if (c == k) known = true;
        const ArrBox* sbx = ArrBounds(f, c);
        if (mm == GL_MODELVIEW && !sbx) {
            if (known) {
                if (!liftPop.on) { glPushMatrix(); liftPop.on = true; gDrawTick++; }
                glScaled(0.5, 0.5, 0.5);
            }
        } else if (mm == GL_MODELVIEW) {
            const float span = std::max(sbx->hi[0] - sbx->lo[0], sbx->hi[2] - sbx->lo[2]);      // the larger footprint side
            GLdouble mv[16];
            GetMV(mv);
            double spanW = span * sqrt(mv[0] * mv[0] + mv[1] * mv[1] + mv[2] * mv[2]);      // in tiles
            if (spanW > 1.15 && spanW < 4.0) {
                double s = 0.96 / spanW;
                if (!liftPop.on) { glPushMatrix(); liftPop.on = true; gDrawTick++; }
                glScaled(s, s, s);
            }
        }
    }
    // 1.9.66 turned every draw within 16 of a boiler's note, wall pieces included: walls near boilers turned in their tiles and showed open sides
    if (gObjRot90 && gCfg.rotateBoiler && !gInWallPass && !liftPop.on && gCam.active && (unsigned)(gDrawSerial - gNoteSerial) <= 16 && c >= 30 &&
        gRotNote != gNoteSerial) {
        gRotNote = gNoteSerial;
        GLint mm = 0;
        glGetIntegerv(GL_MATRIX_MODE, &mm);
        if (mm == GL_MODELVIEW) {
            glPushMatrix();
            glRotatef(90.0f, 0.0f, 1.0f, 0.0f);      // about the model's own upright (y) axis, through its centre
            liftPop.on = true;
            gDrawTick++;      // GetMV's per-draw cache: the wall snap below must see the turned matrix
        }
    }
    const std::unordered_set<int>& noWall = gWbCapture ? gNoWallEver : gNoWallPrev;      // 1.9.130: recorded - every shop front ever seen
    if (gInWallPass && !noWall.empty()) {
        GLdouble mv[16];
        GetMV(mv);
        double ox = MVWorldX(mv),
               oy = MVWorldY(mv);
        if (noWall.count((int)floor(oy) * 65536 + (int)floor(ox))) {
            gDrawnCur.insert((int)floor(oy) * 65536 + (int)floor(ox));
            return;
        }
    }      // no wall over a shop front
    if (gInWallPass) {
        if (gPerfOn) {
            double _w0 = PerfNow();
            WallPiece(m, f, c);
            gPerf.t[4] += PerfNow() - _w0;
        } else
            WallPiece(m, f, c);
    } else if (PERF(0, HideCutFitting(m, f, c))) {
        if (ui) UiTraceLog("DROPPED-hidecut", m, f, c);
        return;
    } else if (PERF(1, BubbleDraw(m, f, c))) {
        if (ui) UiTraceLog("DROPPED-bubble", m, f, c);
        return;
    } else if (PERF(2, WallSnapObject(m, f, c))) {
        if (ui) UiTraceLog("DROPPED-snap", m, f, c);
        return;
    } else if (PERF(3, SnapLying(m, f, c))) {
        if (ui) UiTraceLog("DROPPED-lying", m, f, c);
        return;
    }
    if (HideThisDraw(c)) {
        if ((int)gBound[0] == gCfg.hideTexA && m == GL_TRIANGLES) { GLCHK("before items"); double _i0 = gPerfOn ? PerfNow() : 0; DrawItems3D(f, c); if (gPerfOn) gPerf.t[5] += PerfNow() - _i0; GLCHK("DrawItems3D"); }      // the main sprite sheet's hidden batch: stacks and loose items
        if ((int)gBound[0] == gCfg.hideTexB) {
            
            DrawSmallSprites(m, f, c);

        }
        {
            static std::set<long long> seenH;
            long long hk = ((long long)c << 20) ^ ((long long)gBound[0] << 4) ^ gCurProg;
            if (seenH.size() < 40 && seenH.insert(hk).second)
                Log("HIDDEN draw count %d tex0 %u prog %u note-gap %u", (int)c, gBound[0], gCurProg, gDrawSerial - gNoteSerial);
        }
        if (ui) UiTraceLog("DROPPED-hide", m, f, c);
        return;
    }
    if (ui) UiTraceLog("drawn", m, f, c);
    // 1.9.96: doors through ShiftDoor again (the call had been lost: no door ever got its thickness, fit, cut or double-door split)
    if (gObjDoor && !gInWallPass && gCam.active && (unsigned)(gDrawSerial - gNoteSerial) <= 16 && ShiftDoor(m, f, c)) { ResetPrisonerUniforms(); return; }
    if (gObjServing && !gInWallPass && gCam.active && m == GL_TRIANGLES && (unsigned)(gDrawSerial - gNoteSerial) <= 16) { GLCHK("before serving"); c = ServingTableCount(f, c); GLCHK("ServingTableCount"); }
    if (gObjShopFront && gCfg.shopFrontOut != 0.0f && !gInWallPass && gCam.active && (unsigned)(gDrawSerial - gNoteSerial) <= 16) {
        // the shop counter overhangs the wall line in 2D: move it out along its front (model +z)
        GLdouble mv[16];
        GetMV(mv);
        double fx = gViewInv[0] * mv[8] + gViewInv[4] * mv[9] + gViewInv[8] * mv[10], fy = gViewInv[1] * mv[8] + gViewInv[5] * mv[9] + gViewInv[9] * mv[10];
        double L = sqrt(fx * fx + fy * fy);
        if (L > 1e-6) {
            double d[2] = {fx / L * gCfg.shopFrontOut, fy / L * gCfg.shopFrontOut};
            GLdouble M2[16];
            memcpy(M2, mv, sizeof M2);
            for (int i = 0; i < 3; i++) M2[12 + i] += gViewInv[i * 4 + 0] * d[0] + gViewInv[i * 4 + 1] * d[1];
            {      // stretch it along its length to the whole tiles it stands in, so it meets the wall at both ends (it is 1.8 tiles for a 2-tile gap)
                const ArrState& AS = GetArr();
                int st = AS.st ? AS.st : AS.sz * 4;
                double mx = 0, ax = sqrt(mv[0] * mv[0] + mv[1] * mv[1] + mv[2] * mv[2]);
                if (AS.vp && !AS.vbo && AS.ty == 0x1406 && AS.sz >= 3 && Readable((uint8_t*)AS.vp + (size_t)f * st, (size_t)c * st))
                    for (int i = 0; i < c; i++) mx = std::max(mx, (double)fabsf(((const float*)((uint8_t*)AS.vp + (size_t)(f + i) * st))[0]));
                double half = mx * ax, want = std::max(1.0, floor(half * 2.0 + 0.5)) * 0.5;
                if (half > 0.2 && want / half > 1.0 && want / half < 1.35)
                    for (int i = 0; i < 3; i++) M2[i] *= want / half;
            }
            glPushMatrix();
            glLoadMatrixd(M2);
            oDrawArrays(m, f, c);
            glPopMatrix();
            ResetPrisonerUniforms();
            return;
        }
    }
    bool scaled = gCfg.bodyScale && gObjPrisoner && fabsf(gPrisScale - 1.0f) > 0.02f && (unsigned)(gDrawSerial - gNoteSerial) <= 16 && !gInWallPass;
    if (gObjPrisoner && gCfg.poseDiag && !gInWallPass && gCam.active && (unsigned)(gDrawSerial - gNoteSerial) <= 16 && m == GL_TRIANGLES && c >= 400) {      // diagnostic: prisoners drawn small or lifted (reads every vertex: off by default)
        GLdouble mv[16];
        GetMV(mv);
        double up = sqrt(mv[4] * mv[4] + mv[5] * mv[5] + mv[6] * mv[6]);      // model y axis in eye units
        double ax = sqrt(mv[0] * mv[0] + mv[1] * mv[1] + mv[2] * mv[2]), az = sqrt(mv[8] * mv[8] + mv[9] * mv[9] + mv[10] * mv[10]);
        double h = gCfg.upSign * (gViewInv[2] * mv[12] + gViewInv[6] * mv[13] + gViewInv[10] * mv[14] + gViewInv[14]);      // origin height above the floor
        // the model's own vertices: tallest point and widest x, against the first draw of the same body (the game could animate them)
        double vTop = 0, vHalf = 0, vRel = 1, hRel = 1;
        {
            const ArrState& AS = GetArr();
            int st = AS.st ? AS.st : AS.sz * 4;
            if (AS.vp && !AS.vbo && AS.ty == 0x1406 && AS.sz >= 3 && Readable((uint8_t*)AS.vp + (size_t)f * st, (size_t)c * st))
                for (int i = 0; i < c; i++) { const float* P = (const float*)((uint8_t*)AS.vp + (size_t)(f + i) * st); vTop = std::max(vTop, (double)P[1]); vHalf = std::max(vHalf, (double)fabsf(P[0])); }
            static std::map<int, std::pair<double, double>> first;
            auto fi = first.find((int)c);
            if (fi == first.end()) first[(int)c] = std::make_pair(vTop, vHalf);
            else { if (fi->second.first > 0) vRel = vTop / fi->second.first; if (fi->second.second > 0) hRel = vHalf / fi->second.second; }
        }
        static double typUp = 0, typAx = 0, typH = -99; static int pl = 0; static std::map<std::string, DWORD> lastLog;
        if (typUp == 0) { typUp = up; typAx = ax; typH = h; }
        if (pl < 200 && (fabs(up / typUp - 1.0) > 0.06 || fabs(ax / typAx - 1.0) > 0.06 || fabs(az / typAx - 1.0) > 0.06 || fabs(h - typH) > 0.05 || fabs(vRel - 1.0) > 0.04 || fabs(hRel - 1.0) > 0.04)) {
            std::string nm = gPrisBio ? BioName(gPrisBio + 0x18) + " " + BioName(gPrisBio + 0x38) : "?";
            DWORD now = GetTickCount();
            auto it = lastLog.find(nm);
            if (it == lastLog.end() || now - it->second > 5000) {      // each prisoner at most every 5 s
                lastLog[nm] = now; pl++;
                Log("PRISPOSE %s (%d verts): placed height %.2f width %.2f depth %.2f of usual, origin %.3f vs usual %.3f; own vertices top %.2f width %.2f of first draw; head %d, body scale %.2f", nm.c_str(), (int)c, up / typUp, ax / typAx, az / typAx, h, typH, vRel, hRel, gPrisHead, gPrisScale);
            }
        }
    }
    if (scaled) { glPushMatrix(); glScalef(gPrisScale, gPrisScale, gPrisScale); }      // around the model's feet: it stays on the floor
    GLCHK("before body");
    if (!HeadSwapDraw(m, f, c)) { oDrawArrays(m, f, c); GLCHK(gObjServing ? "serving table draw" : "plain draw"); } else GLCHK("HeadSwapDraw");
    DrawEquipment(m, f, c);
    GLCHK("DrawEquipment");
    if (scaled) glPopMatrix();
    if (tinted) gPrisStat[4]++;
    ResetPrisonerUniforms();
}
static bool gDoorXf = false;      // which side of its wall cell a door should slide to (towards the outer face of the room)
static void DoorSideShift(double cxw, double cyw, double& sx, double& sy) {
    sx = sy = 0;
    double amt = gCfg.doorShift < 0.0f ? std::max(0.0, gCfg.wallThick * 0.5 - 0.08) : (double)gCfg.doorShift;
    if (amt < 0.001 || gRoomW <= 0 || gRoomBuf.empty()) return;
    int cx = (int)floor(cxw), cy = (int)floor(cyw);
    auto room = [&](int x, int y) -> int {
        if (x < 0 || y < 0 || x >= gRoomW || y >= gRoomH) return -1;
        return gRoomBuf[((size_t)y * gRoomW + x) * 2 + 1] > 127 ? 1 : 0;
    };
    int rE = room(cx + 1, cy), rW = room(cx - 1, cy), rN = room(cx, cy + 1), rS = room(cx, cy - 1);
    if (rE >= 0 && rW >= 0 && rE != rW && rN == rS)
        sx = (rE == 0 ? 1.0 : -1.0) * amt;
    else if (rN >= 0 && rS >= 0 && rN != rS && rE == rW)
        sy = (rN == 0 ? 1.0 : -1.0) * amt;
}
static std::unordered_map<long long, DoorDecision> gDoorCache;
// decision for a door, held steady while the door animates: keyed on its position rounded to half a tile, refreshed every 4 s
static DoorDecision DoorStable(double cxw, double cyw) {
    long long key = (long long)lround(cxw * 2.0) * 100003LL + (long long)lround(cyw * 2.0);
    double now = NowSec();
    auto it = gDoorCache.find(key);
    if (it != gDoorCache.end() && now - it->second.t < 4.0) return it->second;
    DoorDecision r;
    DoorSideShift(cxw, cyw, r.sx, r.sy);
    r.cut = WallCutAt(cxw + 0.5, cyw + 0.5) || WallCutAt(cxw - 0.5, cyw - 0.5);
    r.t = now;
    if (gDoorCache.size() > 2000) gDoorCache.clear();
    gDoorCache[key] = r;
    return r;
}
static bool DoorXformPush() {
    GLint mm = 0;
    glGetIntegerv(GL_MATRIX_MODE, &mm);
    if (mm != GL_MODELVIEW) return false;
    GLdouble mv[16];
    GetMV(mv);
    double ex0 = mv[12], ey0 = mv[13], ez0 = mv[14];
    double cxw = gViewInv[0] * ex0 + gViewInv[4] * ey0 + gViewInv[8] * ez0 + gViewInv[12],
           cyw = gViewInv[1] * ex0 + gViewInv[5] * ey0 + gViewInv[9] * ez0 + gViewInv[13];
    DoorDecision dd = DoorStable(cxw, cyw);
    double sx = dd.sx, sy = dd.sy;
    bool cutNow = DoorCutNow();
    double e[3];
    for (int i = 0; i < 3; i++) e[i] = gViewInv[i * 4 + 0] * sx + gViewInv[i * 4 + 1] * sy;
    static int il = 0;
    if (il < 16) {
        il++;
        Log("DOORXFORM origin (%.2f, %.2f) shift (%.2f, %.2f) cut %d", cxw, cyw, sx, sy, (int)cutNow);
    }
    glPushMatrix();
    glLoadIdentity();
    glTranslated(e[0], e[1], e[2]);
    glMultMatrixd(mv);
    if (cutNow) glScaled(1.0, gCfg.doorCutScale, 1.0);
    if (DoorDepthScale() != 1.0) glScaled(1.0, 1.0, DoorDepthScale());      // 1.9.78: thickness follows the wall thickness
    return true;
}
// Immediate-mode draws (glBegin .. glEnd) can be hidden too: colour writes are switched off until glEnd.
typedef void(APIENTRY* PFN_End)(void);
static PFN_End oEnd = nullptr;
static bool gImmMasked = false;
static void APIENTRY hkBegin(GLenum m) {
    gDrawTick++;
    if (gInWallPass && gWbCapture) gWbOther++;
    FlushOverlaysIfUi();
    TransDiag("begin", m, 0, 0);
    NoteOutdoors();
    {
        static GLuint lastImm = 0xFFFFFFFF;
        if (gBound[0] != lastImm) {
            lastImm = gBound[0];
            if (gBound[0] < 4096 && gBound[0] && !gInWallPass) gSeenTex.insert((int)gBound[0]);
        }
    }
    if (HideThisDraw()) {
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        gImmMasked = true;
    }
    if (false && gObjDoor && gCfg.liftDoors && gCam.active && !gInWallPass &&
        !gDoorXf) {      // door drawn in immediate mode: move / cut it by changing the matrix first
        GLint mm = 0;
        glGetIntegerv(GL_MATRIX_MODE, &mm);
        if (mm == GL_MODELVIEW) {
            GLdouble mv[16];
            GetMV(mv);
            double ex0 = mv[12], ey0 = mv[13], ez0 = mv[14];
            double cxw = gViewInv[0] * ex0 + gViewInv[4] * ey0 + gViewInv[8] * ez0 + gViewInv[12],
                   cyw = gViewInv[1] * ex0 + gViewInv[5] * ey0 + gViewInv[9] * ez0 + gViewInv[13];
            double sx = 0, sy = 0;
            DoorSideShift(cxw, cyw, sx, sy);
            bool cutNow = DoorCutNow();
            double e[3];
            for (int i = 0; i < 3; i++) e[i] = gViewInv[i * 4 + 0] * sx + gViewInv[i * 4 + 1] * sy;
            static int il = 0;
            if (il < 12) {
                il++;
                Log("DOORIMM origin (%.2f, %.2f) shift (%.2f, %.2f) cut %d", cxw, cyw, sx, sy, (int)cutNow);
            }
            glPushMatrix();
            glLoadIdentity();
            glTranslated(e[0], e[1], e[2]);
            glMultMatrixd(mv);
            if (cutNow) glScaled(1.0, gCfg.doorCutScale, 1.0);
            gDoorXf = true;
        }
    }
    if (gCfg.deferOverlays && !gImmMasked && !gDeferRec && gCam.active && !gInWallPass && !gCurProg && gDeferred.size() < 64 &&
        glIsEnabled(GL_BLEND) && !glIsEnabled(GL_DEPTH_TEST)) {
        GLdouble P[16];
        GetPR(P);
        bool cam3d = true;
        for (int q = 0; q < 16; q++)
            if (fabs(P[q] - gCamP[q]) > 1e-9) {
                cam3d = false;
                break;
            }
        if (cam3d) {
            DeferredOverlay& d = gDeferPending;
            glGetFloatv(GL_CURRENT_COLOR, d.col);
            d.tex = gBound[0];
            glGetIntegerv(0x0BE1, &d.src);
            glGetIntegerv(0x0BE0, &d.dst);
            d.tex2d = glIsEnabled(GL_TEXTURE_2D);
            glGetDoublev(GL_MODELVIEW_MATRIX, d.mv);
            memcpy(d.pr, P, sizeof P);
            d.list = glGenLists(1);
            if (d.list) {
                glNewList(d.list, GL_COMPILE);
                gDeferRec = true;
            }
        }
    }
    oBegin(m);
}
static void APIENTRY hkEnd(void) {
    oEnd();
    if (gDeferRec) {
        glEndList();
        gDeferRec = false;
        gDeferred.push_back(gDeferPending);
    }
    if (gDoorXf) {
        glPopMatrix();
        gDoorXf = false;
    }
    if (gImmMasked) {
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        gImmMasked = false;
    }
}
static void APIENTRY hkDrawElements(GLenum m, GLsizei c, GLenum t, const void* i) {
    gDrawTick++;
    if (gInWallPass && gWbCapture) gWbOther++;
    if (gObjTV && !gInWallPass) {
        static std::set<int> seenE;
        if (seenE.size() < 8 && seenE.insert((int)c).second) Log("TVPATH elements count %d serial gap %u", (int)c, gDrawSerial - gNoteSerial);
    }
    FlushOverlaysIfUi();
    TransDiag("elements", m, 0, c);
    ModelDrawDiag("elements", m, c);
    {
        static int dlog = 0;
        if (dlog < 30 && gCam.active && !gInWallPass && (c % 3) == 0 && c >= 150 && c <= 1000) {
            GLint vb = 0;
            glGetIntegerv(0x8894, &vb);
            GLint eb = 0;
            glGetIntegerv(0x8895, &eb);
            dlog++;
            Log("ELEMDRAW count %d mode 0x%x vbo %d ebo %d prog %u", (int)c, (unsigned)m, (int)vb, (int)eb, gCurProg);
        }
    }
    NoteOutdoors();
    DrawSurvey("elements", m, c);
    FloorHook();
    if (HideThisDraw(c)) return;
    oDrawElements(m, c, t, i);
}

// Flicker diagnostic: frame-buffer binds and program binds per frame
static int gFrFbo = 0, gFrProg = 0;
static unsigned gFrFboMask = 0;
typedef void(APIENTRY* PFN_BindFB)(GLenum, GLuint);
static PFN_BindFB oBindFramebuffer = nullptr, oBindFramebufferEXT = nullptr;
// Flicker diagnostic 4: the extra frame-buffer pass. Log the frame-buffer sequence of a few frames, and the GL state just before the extra
// buffer is bound and just after the game leaves it, so a setting it leaves behind shows up.
struct GlSnap {
    GLint vp[4], prog, at, tex[8], dfunc, bs, bd, mm, fbo;
    GLboolean dm, dt, bl, cull, sc;
    double mv0, pr0, pr5, pr15;
};
static void TakeSnap(GlSnap& s) {
    glGetIntegerv(GL_VIEWPORT, s.vp);
    glGetIntegerv(GL_CURRENT_PROGRAM, &s.prog);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &s.at);
    for (int i = 0; i < 8; i++) s.tex[i] = (GLint)gBound[i];
    glGetIntegerv(GL_DEPTH_FUNC, &s.dfunc);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &s.dm);
    s.dt = glIsEnabled(GL_DEPTH_TEST);
    s.bl = glIsEnabled(GL_BLEND);
    s.cull = glIsEnabled(GL_CULL_FACE);
    s.sc = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(0x80C9, &s.bs);
    glGetIntegerv(0x80C8, &s.bd);
    glGetIntegerv(GL_MATRIX_MODE, &s.mm);
    GLdouble M[16];
    glGetDoublev(GL_MODELVIEW_MATRIX, M);
    s.mv0 = M[0];
    glGetDoublev(GL_PROJECTION_MATRIX, M);
    s.pr0 = M[0];
    s.pr5 = M[5];
    s.pr15 = M[15];
    s.fbo = (GLint)gCurFbo;
}
static void LogSnap(const char* tag, const GlSnap& s) {
    Log("FBOSTATE %s fbo %d serial %u wallpass %d camActive %d vp %d,%d,%d,%d prog %d activeTex %d tex0-7 %d %d %d %d %d %d %d %d depth %d func 0x%x "
        "write %d blend %d %x/%x cull %d scissor %d matMode 0x%x mv0 %.4f proj0 %.5f proj5 %.5f proj15 %.1f",
        tag, s.fbo, gDrawSerial, (int)gInWallPass, (int)gCam.active, s.vp[0], s.vp[1], s.vp[2], s.vp[3], s.prog, s.at - GL_TEXTURE0, s.tex[0],
        s.tex[1], s.tex[2], s.tex[3], s.tex[4], s.tex[5], s.tex[6], s.tex[7], (int)s.dt, s.dfunc, (int)s.dm, (int)s.bl, s.bs, s.bd, (int)s.cull,
        (int)s.sc, s.mm, s.mv0, s.pr0, s.pr5, s.pr15);
}
static int gFboDiagLeft = 0, gFboDiagShots = 0;
static void FboDiag(GLuint f) {
    if (!gCfg.diagLog) return;      // one-off diagnostic (Settings: diagLog)
    static GLuint prev = 0;
    static int lines = 0;
    if (!gCam.active || !WorldLoaded()) {
        prev = f;
        return;
    }
    if (f == 4 && prev != 4 && gFboDiagShots < 4) {
        gFboDiagShots++;
        gFboDiagLeft = 40;
        GlSnap s;
        TakeSnap(s);
        LogSnap("BEFORE-extra", s);
    }
    if (gFboDiagLeft > 0 && lines < 400) {
        lines++;
        gFboDiagLeft--;
        Log("FBOSEQ bind %u (was %u) serial %u wallpass %d", f, prev, gDrawSerial, (int)gInWallPass);
    }
    prev = f;
}
// THE FLICKER: every second or two of game time the game draws a small picture into an extra frame buffer (fbo 4). For that it loads an identity
// modelview and does not restore it, because in the plain 2D game the camera lives in the projection and the modelview is identity anyway.
// The 3D view keeps its camera in the modelview, so walls and objects drawn after that pass lost the camera for one frame and vanished.
// Put the camera back whenever the game leaves another frame buffer, and again at the start of the wall pass, if the modelview was left as identity.
static void RestoreCamMV(const char* why) {
    if (!gCam.active || !WorldLoaded()) return;
    GLdouble M[16];
    glGetDoublev(GL_MODELVIEW_MATRIX, M);
    bool ident = true;
    for (int i = 0; i < 16; i++)
        if (fabs(M[i] - ((i % 5) == 0 ? 1.0 : 0.0)) > 1e-9) {
            ident = false;
            break;
        }
    if (!ident || fabs(gCamMV[0] - 1.0) < 1e-9 && fabs(gCamMV[5] - 1.0) < 1e-9) return;
    GLint mm = GL_MODELVIEW;
    glGetIntegerv(GL_MATRIX_MODE, &mm);
    if (mm != GL_MODELVIEW) glMatrixMode(GL_MODELVIEW);
    glLoadMatrixd(gCamMV);
    if (mm != GL_MODELVIEW) glMatrixMode((GLenum)mm);
    static int lines = 0;
    if (lines < 10) {
        lines++;
        Log("CAMFIX camera modelview restored (%s)", why);
    }
}
static void FboDiagAfter(GLuint f, GLuint was) {
    if (was == 4 && f != 4 && gCfg.fixExtraPass) RestoreCamMV("left the extra frame buffer");
    if (was == 4 && f != 4 && gFboDiagLeft > 0) {
        GlSnap s;
        TakeSnap(s);
        LogSnap("AFTER-extra", s);
    }
}
static void APIENTRY hkBindFramebuffer(GLenum t, GLuint f) {
    GLuint was = gCurFbo;
    if (t == 0x8D40 || t == 0x8CA9) {
        gCurFbo = f;
        FboDiag(f);
    }
    gFrFbo++;
    gFrFboMask |= 1u << (f & 31);
    oBindFramebuffer(t, f);
    FboDiagAfter(f, was);
}
static void APIENTRY hkBindFramebufferEXT(GLenum t, GLuint f) {
    GLuint was = gCurFbo;
    if (t == 0x8D40 || t == 0x8CA9) {
        gCurFbo = f;
        FboDiag(f);
    }
    gFrFbo++;
    gFrFboMask |= 1u << (f & 31);
    oBindFramebufferEXT(t, f);
    FboDiagAfter(f, was);
}
typedef void(APIENTRY* PFN_VertexPointer)(GLint, GLenum, GLsizei, const void*);
static PFN_VertexPointer oVertexPointer = nullptr;
static void APIENTRY hkVertexPointer(GLint size, GLenum type, GLsizei stride, const void* ptr) {
    gVPSize = size;
    gVPType = (GLint)type;
    gVPStride = stride;
    gVPPtr = ptr;
    oVertexPointer(size, type, stride, ptr);
}
typedef void(APIENTRY* PFN_BindBuf)(GLenum, GLuint);
static PFN_BindBuf oBindBuffer = nullptr, oBindBufferARB = nullptr;
static void APIENTRY hkBindBuffer(GLenum t, GLuint b) {
    if (t == 0x8892) gArrayVbo = (GLint)b;
    oBindBuffer(t, b);
}
static void APIENTRY hkBindBufferARB(GLenum t, GLuint b) {
    if (t == 0x8892) gArrayVbo = (GLint)b;
    oBindBufferARB(t, b);
}
static PROC WINAPI hkWglGetProcAddress(LPCSTR name) {
    PROC p = oWglGetProcAddress(name);
    static int calls = 0;
    if (name && ((uintptr_t)name >> 16) != 0 && (calls++ < 3 || !p) && calls < 60) Log("wglGetProcAddress(%s) -> %p", name, (void*)p);
    if (!p || !name || ((uintptr_t)name >> 16) == 0) return p;
#define WRAP(str, orig, hook)                                                                                                                        \
    if (strcmp(name, str) == 0) {                                                                                                                    \
        orig = (decltype(orig))p;                                                                                                                    \
        return (PROC)hook;                                                                                                                           \
    }
    WRAP("glShaderSource", oShaderSource, hkShaderSource)
    WRAP("glShaderSourceARB", oShaderSourceARB, hkShaderSourceARB)
    WRAP("glAttachShader", oAttachShader, hkAttachShader)
    WRAP("glAttachObjectARB", oAttachObjectARB, hkAttachObjectARB)
    WRAP("glUseProgram", oUseProgram, hkUseProgram)
    WRAP("glUseProgramObjectARB", oUseProgramObjectARB, hkUseProgramObjectARB)
    WRAP("glActiveTexture", oActiveTexture, hkActiveTexture)
    WRAP("glActiveTextureARB", oActiveTextureARB, hkActiveTextureARB)
    WRAP("glBindBuffer", oBindBuffer, hkBindBuffer)
    WRAP("glBindBufferARB", oBindBufferARB, hkBindBufferARB)
    WRAP("glBindFramebuffer", oBindFramebuffer, hkBindFramebuffer)
    WRAP("glBindFramebufferEXT", oBindFramebufferEXT, hkBindFramebufferEXT)
#undef WRAP
    return p;
}
