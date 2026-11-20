// parts/06_objects.inc - per-draw object work: hiding, lying people, bubbles, doors, wall snap, fittings, overlays, culling. Part of main.cpp (one translation unit, included in order): it uses everything defined in the parts before it.
static bool HideThisDraw(int cnt = 100000) {
    if (gCfg.hideSunShadows && gCam.active && !gInWallPass && gCurProg &&
        gSunShadowPrograms.count(gCurProg)) {      // flat 2D sun shadows do not match 3D objects
        GLdouble P[16];
        GetPR(P);
        bool cam3d = true;
        for (int i = 0; i < 16; i++)
            if (fabs(P[i] - gCamP[i]) > 1e-9) {
                cam3d = false;
                break;
            }
        if (cam3d) return true;
        static int ul = 0;
        if (ul < 10) {
            ul++;
            Log("SHADOWPROG draw under a non-camera projection left visible (prog %u tex0 %u)", gCurProg, gBound[0]);
        }      // would have been UI
    }
    if ((gCfg.hideTexA == 0 && gCfg.hideTexB == 0) || gInWallPass || !gCfg.isoEnabled || !gCam.active) return false;
    if (!gCurProg) return false;      // fixed-function draws are the game's UI: its texture names get reused, so never hide those
    GLuint t = gBound[0];
    if (!t || ((int)t != gCfg.hideTexA && (int)t != gCfg.hideTexB)) return false;
    if (cnt < 60)
        return false;      // the hidden layers are big map-wide batches; a small draw on the same texture is a placement / move ghost and must stay visible
    {
        GLdouble P[16];
        GetPR(P);
        for (int i = 0; i < 16; i++)
            if (fabs(P[i] - gCamP[i]) > 1e-9) return false;
    }      // only draws under the 3D camera: the UI (orthographic) shares texture names and must never be hidden
    return true;
}
// People lying down (knocked out, dead, sleeping on the floor) are the upright model rotated about its origin, so
// they hover. A model whose up axis is tipped over and whose lowest point is a little above the floor is drawn
// lowered until it touches. Anything higher than snapMax (a person on a bed) is left alone.
static bool SnapLying(GLenum mode, GLint first, GLsizei count) {
    if (gMenuWorld) return false;
    if (!gCfg.snapLying || !gCam.active || mode != GL_TRIANGLES || count < 30 || count > 6000 || !gCurProg) return false;
    auto it = gLocs.find(gCurProg);
    if (it == gLocs.end() || it->second.wpass < 0) return false;      // the 3D-mode program only
    GLdouble mv[16];
    GetMV(mv);
    double Uz = gViewInv[0 * 4 + 2] * mv[4] + gViewInv[1 * 4 + 2] * mv[5] + gViewInv[2 * 4 + 2] * mv[6];      // world z of the model's up axis
    if (fabs(Uz) > 0.5) return false;                                                                         // upright: leave alone
    const ArrState& AS = GetArr();
    void* vp = AS.vp;
    GLint sz = AS.sz, ty = AS.ty, st = AS.st, vbo = AS.vbo;
    int stride = st ? st : sz * 4;
    if (sz < 3 || ty != 0x1406 || vbo || !vp || !Readable((uint8_t*)vp + (size_t)first * stride, (size_t)count * stride)) return false;
    double minH = 1e9;
    for (int i = 0; i < count; i++) {
        const float* P = (const float*)((uint8_t*)vp + (size_t)(first + i) * stride);
        double ex = mv[0] * P[0] + mv[4] * P[1] + mv[8] * P[2] + mv[12], ey = mv[1] * P[0] + mv[5] * P[1] + mv[9] * P[2] + mv[13],
               ez = mv[2] * P[0] + mv[6] * P[1] + mv[10] * P[2] + mv[14];
        double wz = gViewInv[0 * 4 + 2] * ex + gViewInv[1 * 4 + 2] * ey + gViewInv[2 * 4 + 2] * ez + gViewInv[14];
        double hgt = gCfg.upSign * wz;
        if (hgt < minH) minH = hgt;
    }
    static int logged = 0;
    if (logged < 30 && minH > 0.01) {
        logged++;
        Log("LYING model: %d verts, lowest point %.3f above the floor", (int)count, minH);
    }
    if (minH < 0.02 || minH > 1.1) return false;
    double target = 0.0;
    bool onBed = false;
    if (minH > gCfg.snapMax) {      // lying on a bed: the game holds them about 0.76 up, well above the 3D beds' mattresses; bring them down onto it
        if (gCfg.bedTop <= 0.0f || minH > 0.95 || minH <= gCfg.bedTop + 0.01) return false;
        target = gCfg.bedTop;
        onBed = true;
    }
    double d = -gCfg.upSign * (minH - target);      // move along world z towards the floor
    double sx = 0, sy = 0;
    if (onBed &&
        gCfg.bedShift != 0.0f) {      // the game lays sleepers with their head past the pillow end: slide them towards their feet onto the bed
        double Ux = gViewInv[0] * mv[4] + gViewInv[4] * mv[5] + gViewInv[8] * mv[6],
               Uy = gViewInv[1] * mv[4] + gViewInv[5] * mv[5] + gViewInv[9] * mv[6];
        double L = sqrt(Ux * Ux + Uy * Uy);
        if (L > 1e-6) {
            sx = -Ux / L * gCfg.bedShift;
            sy = -Uy / L * gCfg.bedShift;
        }
    }
    double e[3];
    for (int i = 0; i < 3; i++) e[i] = gViewInv[i * 4 + 0] * sx + gViewInv[i * 4 + 1] * sy + gViewInv[i * 4 + 2] * d;
    glPushMatrix();
    glLoadIdentity();
    glTranslated(e[0], e[1], e[2]);
    glMultMatrixd(mv);
    oDrawArrays(mode, first, count);
    glPopMatrix();
    return true;
}
// Speech / thought bubbles, their icons and their text are 2D quads laid flat on the map above the speaker. In the 3D view
// they lie on the floor, tilted and mirrored. Redraw them as upright screen-facing sprites: the quad's map position
// gives the speaker (it sits bubbleDrop tiles below the bubble), which is projected through the 3D camera.
static std::unordered_map<int, int> gPersonSeen;      // half-tile -> frame a person model was drawn there
static double gBubbleDy = 1.4;                         // 1.9.119: how far south of this bubble draw its speaker stands (tiles)
static std::unordered_set<GLuint> gUiTex;              // 1.9.117: textures the game's own screen overlay uses (interface atlas, fonts)
static bool BubbleDraw(GLenum mode, GLint first, GLsizei count) {
    if (!gCfg.billboardBubbles || !gCam.active || gInWallPass || mode != GL_TRIANGLES || count < 3 || (count % 3)) return false;
    GLuint t = gBound[0];
    if (!t) return false;
    // 1.9.121 (user: 42 -> 15 fps): the projection is read back only when it changes (gProjGen) - 1.9.117 read it on every draw, a GPU stall each
    static unsigned pGen = 0;
    static GLdouble P[16];
    static bool camPc = false;
    if (pGen != gProjGen) {
        pGen = gProjGen;
        GetPR(P);
        camPc = true;
        for (int i = 0; i < 16; i++)
            if (fabs(P[i] - gCamP[i]) > 1e-4 * (1.0 + fabs(gCamP[i]))) { camPc = false; break; }
    }
    if (!camPc) {
        if (P[15] == 1.0 && P[3] == 0.0 && gUiTex.size() < 64 && !gUiTex.count(t) && gUiTex.insert(t).second) Log("BUBBLES interface texture %u learned (drawn on the screen overlay)", t);
        return false;
    }
    if (!gUiTex.count(t) && (int)t != gCfg.bubbleTexA && (int)t != gCfg.bubbleTexB) return false;
    // 1.9.117 (user: upright bubbles do nothing): the texture ids were fixed numbers (5, 27) that change between launches, and the camera test
    // needed a 1e-9 match against a matrix read back as floats. Now: a texture the HUD draws with (an orthographic projection) is learned as an
    // interface texture; under the 3D camera, a draw with one of those whose quads sit just above a person is a bubble.
    {      // above a person? (floor labels - room names - use the same font and must stay flat)
        void* vp0 = nullptr; GLint vs0 = 0, vsz0 = 0;
        glGetPointerv(0x808E, &vp0); glGetIntegerv(0x807C, &vs0); glGetIntegerv(0x807A, &vsz0);
        int vst0 = vs0 ? vs0 : vsz0 * 4;
        if (!vp0 || vsz0 < 2 || !Readable((uint8_t*)vp0 + (size_t)first * vst0, (size_t)count * vst0)) return false;
        // a bubble floats bubbleDrop tiles north of its speaker; an item a person carries or stands by lies AT the person - those stay as they are
        if (count > 600) return false;
        auto personAt = [&](double x, double y, int r) {
            for (int dy = -r; dy <= r; dy++)
                for (int dx = -r; dx <= r; dx++) {
                    auto ps = gPersonSeen.find(((int)floor(y * 2) + dy) * 65536 + (int)floor(x * 2) + dx);
                    if (ps != gPersonSeen.end() && gWallFramesSeenExt - ps->second <= 3) return true;
                }
            return false;
        };
        float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
        for (int i = 0; i < count; i++) {
            const float* V = (const float*)((uint8_t*)vp0 + (size_t)(first + i) * vst0);
            mnx = std::min(mnx, V[0]); mxx = std::max(mxx, V[0]); mny = std::min(mny, V[1]); mxy = std::max(mxy, V[1]);
        }
        double qx = (mnx + mxx) * 0.5, qy = (mny + mxy) * 0.5;
        // 1.9.119 (user: the bubble sliders do nothing - moving "Bubble drop" broke the speaker test): the speaker is the nearest person found
        // 0.5..3.5 tiles south of the bubble, whatever the slider says; the bubble is anchored on that person
        bool found = false;
        gBubbleDy = -1.0;
        for (int hy = 1; hy <= 7 && !found; hy++)
            for (int dx = -1; dx <= 1; dx++) {
                auto ps = gPersonSeen.find(((int)floor(qy * 2) + hy) * 65536 + (int)floor(qx * 2) + dx);
                if (ps != gPersonSeen.end() && gWallFramesSeenExt - ps->second <= 3) { gBubbleDy = ((int)floor(qy * 2) + hy + 0.5) * 0.5 - qy; found = true; break; }
            }
        // 1.9.127 (user: bubbles still on the floor - a "Luxuries" need burst lying beside its prisoner): need bubbles are drawn next to the
        // speaker, not north of it, and were taken for a carried thing. With no speaker to the south, a person within a tile on either side
        // is the speaker (only interface textures get here, never a carried object's)
        if (!found)
            for (int hy = 0; hy >= -2 && !found; hy--)
                for (int dx = -2; dx <= 2 && !found; dx++) {
                    auto ps = gPersonSeen.find(((int)floor(qy * 2) + hy) * 65536 + (int)floor(qx * 2) + dx);
                    if (ps != gPersonSeen.end() && gWallFramesSeenExt - ps->second <= 3) { gBubbleDy = ((int)floor(qy * 2) + hy + 0.5) * 0.5 - qy; found = true; }
                }
        if (!found) {
            static int nl = 0;
            if (nl < 6 && personAt(qx, qy, 4)) { nl++; Log("Bubble left flat: interface texture %u at (%.2f, %.2f), %d vertices - no speaker found next to it", t, qx, qy, (int)count); }
            return false;
        }
    }
    void *vp = nullptr, *tp = nullptr, *cp = nullptr;
    glGetPointerv(0x808E, &vp);
    glGetPointerv(0x8092, &tp);
    glGetPointerv(0x8090, &cp);
    GLint vsz = 0, vst = 0, tsz = 0, tst = 0, csz = 0, cty = 0, cst = 0;
    glGetIntegerv(0x807A, &vsz);
    glGetIntegerv(0x807C, &vst);
    glGetIntegerv(0x8088, &tsz);
    glGetIntegerv(0x808A, &tst);
    glGetIntegerv(0x8081, &csz);
    glGetIntegerv(0x8082, &cty);
    glGetIntegerv(0x8083, &cst);
    bool cEn = glIsEnabled(0x8076) != 0;
    int vs = vst ? vst : vsz * 4, ts = tst ? tst : tsz * 4, cs = cst ? cst : csz * (cty == 0x1401 ? 1 : 4);
    if (!vp || !tp || vsz < 2 || tsz < 2) return false;
    if (!Readable((uint8_t*)vp + (size_t)first * vs, (size_t)count * vs) || !Readable((uint8_t*)tp + (size_t)first * ts, (size_t)count * ts))
        return false;
    if (cEn && (!cp || csz < 3 || !Readable((uint8_t*)cp + (size_t)first * cs, (size_t)count * cs))) cEn = false;
    GLint vpv[4];
    glGetIntegerv(GL_VIEWPORT, vpv);
    GLfloat cur[4];
    glGetFloatv(GL_CURRENT_COLOR, cur);
    bool persp = P[15] == 0.0;
    int grp = (count % 6 == 0) ? 6 : 3;
    static int logged = 0;
    if (logged++ == 0) Log("Speech bubbles: redrawing texture %u as upright sprites (%d vertices)", t, (int)count);
    GLuint prog = gCurProg;
    if (prog && oUseProgram) oUseProgram(0);
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_TRANSFORM_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(vpv[0], vpv[0] + vpv[2], vpv[1], vpv[1] + vpv[3], -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glBegin(GL_TRIANGLES);
    for (int g0 = 0; g0 + grp <= count; g0 += grp) {
        float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
        for (int k = 0; k < grp; k++) {
            const float* V = (const float*)((uint8_t*)vp + (size_t)(first + g0 + k) * vs);
            if (V[0] < minx) minx = V[0];
            if (V[0] > maxx) maxx = V[0];
            if (V[1] < miny) miny = V[1];
            if (V[1] > maxy) maxy = V[1];
        }
        double cx = (minx + maxx) * 0.5, cy = (miny + maxy) * 0.5;
        double Wp[3] = {cx, cy + gBubbleDy, gCfg.upSign * gCfg.bubbleLift};      // the speaker's head, in map units (1.9.119: the speaker found, not the slider)
        double ex = gCamMV[0] * Wp[0] + gCamMV[4] * Wp[1] + gCamMV[8] * Wp[2] + gCamMV[12];
        double ey = gCamMV[1] * Wp[0] + gCamMV[5] * Wp[1] + gCamMV[9] * Wp[2] + gCamMV[13];
        double ez = gCamMV[2] * Wp[0] + gCamMV[6] * Wp[1] + gCamMV[10] * Wp[2] + gCamMV[14];
        double clx = gCamP[0] * ex + gCamP[4] * ey + gCamP[8] * ez + gCamP[12];
        double cly = gCamP[1] * ex + gCamP[5] * ey + gCamP[9] * ez + gCamP[13];
        double clw = gCamP[3] * ex + gCamP[7] * ey + gCamP[11] * ez + gCamP[15];
        if (fabs(clw) < 1e-9 || (persp && ez > -0.01)) continue;
        double sx = vpv[0] + (clx / clw * 0.5 + 0.5) * vpv[2], sy = vpv[1] + (cly / clw * 0.5 + 0.5) * vpv[3];
        double px = persp ? gCamP[5] * vpv[3] * 0.5 / (-ez) : gCamP[5] * vpv[3] * 0.5;      // pixels per map unit at that depth
        px *= gCfg.bubbleScale;
        sy += (maxy - miny) * px * 0.5 + 0.35 * px;      // 1.9.118 (user screenshot: the bubble sat on the speaker's body, which drew over it): its bottom edge above the head
        for (int k = 0; k < grp; k++) {
            const float* V = (const float*)((uint8_t*)vp + (size_t)(first + g0 + k) * vs);
            const float* T = (const float*)((uint8_t*)tp + (size_t)(first + g0 + k) * ts);
            if (cEn) {
                const uint8_t* C8 = (const uint8_t*)cp + (size_t)(first + g0 + k) * cs;
                if (cty == 0x1401)
                    glColor4f(C8[0] / 255.0f, C8[1] / 255.0f, C8[2] / 255.0f, csz > 3 ? C8[3] / 255.0f : 1.0f);
                else {
                    const float* Cf = (const float*)C8;
                    glColor4f(Cf[0], Cf[1], Cf[2], csz > 3 ? Cf[3] : 1.0f);
                }
            } else
                glColor4f(cur[0], cur[1], cur[2], cur[3]);
            glTexCoord2f(T[0], T[1]);
            glVertex2f((float)(sx + (V[0] - cx) * px), (float)(sy - (V[1] - cy) * px));
        }
    }
    glEnd();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
    if (prog && oUseProgram) oUseProgram(prog);
    return true;
}
static inline void FloorHook() {
    if (!gCfg.ownFloor || !gCam.active || gInWallPass || !gTilesetTex) return;
    if (gBound[0] == gTilesetTex)
        gFloorSeen = true;
    else if (gFloorSeen && !gFloorDone) {
        gFloorDone = true;
        DrawOwnFloor();
    }
}
// The game's door models (door, staff door, jail doors, double doors) stand on the same raised base as its wall pieces. Lower
// them by the wall lift so they meet the floor: matched by vertex count (3 x the face counts in the model loader's log).
static int gRoomW = 0, gRoomH = 0;
static GLuint gRoomTex = 0;      // the room map as a texture (L = indoors, A = room: 255 cell, 200 other)
static std::vector<unsigned char> gRoomBuf;
// CPU copy of the wall-cut rule in the vertex shader, used to lower doors with their walls.
static bool gObjDoor = false;      // set by paiso_note: the object being drawn is a door
static bool gObjBench = false;
static char gBenchName[64] = {0};      // diagnostic: the object being drawn is some kind of bench
static bool gObjSnapType = false;
static unsigned gNoteSerial = 0;      // paiso_note: the object is a wall-mounted / against-the-wall type (toilet, bookcase...)
extern bool gObjPersonExt;
static bool DoorXformPush();
struct DoorDecision {
    double sx, sy;
    bool cut;
    double t;
};
static DoorDecision DoorStable(double cxw, double cyw);
// 1.9.78 (user: "if I scale the wall thickness, the door width should auto scale also"): doors keep the proportion they have to the game's
// own half-tile wall, so their thickness (model z) grows and shrinks with the wall thickness setting.
static double DoorDepthScale() {
    if (!gCfg.doorAutoDepth) return 1.0;
    double k = gCfg.wallThick / 0.5;
    return k < 0.6 ? 0.6 : (k > 2.2 ? 2.2 : k);
}
// 1.9.127 (user: "walls cutaway (doors up), walls down (doors down), walls up (doors up)"): doors, gates and metal detectors are cut
// only when the walls are down, never by the cutaway
static inline bool DoorCutNow() { return gCfg.wallMode == 2; }
static bool WallCutAt(double wx, double wy) {
    if (gCfg.wallMode == 1 && gCfg.cutScreen > -1.5f) {      // 1.9.94: the screen-split cutaways (Close / Far / All), as the wall shader decides
        if (gCfg.cutScreen >= 1.5f) return true;
        double p[4] = {wx, wy, 0.0, 1.0}, e[4], c[4];
        for (int r = 0; r < 4; r++) e[r] = gCamMV[r] * p[0] + gCamMV[4 + r] * p[1] + gCamMV[8 + r] * p[2] + gCamMV[12 + r];
        for (int r = 0; r < 4; r++) c[r] = gCamP[r] * e[0] + gCamP[4 + r] * e[1] + gCamP[8 + r] * e[2] + gCamP[12 + r] * e[3];
        return c[3] > 0 && c[1] / c[3] < gCfg.cutScreen;
    }
    if (!gCfg.hideCutFittings || gCfg.wallMode != 1 || gRoomW <= 0 || gRoomBuf.empty()) return gCfg.wallMode == 2;
    auto at = [&](int x, int y, int ch) -> float {
        if (x < 0 || y < 0 || x >= gRoomW || y >= gRoomH) return 0.f;
        return gRoomBuf[((size_t)y * gRoomW + x) * 2 + ch] > 127 ? 1.f : 0.f;
    };
    double tcx = floor(wx) + 0.5, tcy = floor(wy) + 0.5;
    int sx = fabs(gCam.camDir[0]) > 0.3 ? (gCam.camDir[0] > 0 ? 1 : -1) : 0, sy = fabs(gCam.camDir[1]) > 0.3 ? (gCam.camDir[1] > 0 ? 1 : -1) : 0;
    int cx = (int)floor(tcx), cy = (int)floor(tcy);
    float behind = std::max(at(cx - sx, cy - sy, 0), std::max(at(cx - sx, cy, 0), at(cx, cy - sy, 0)));
    float front = std::min(at(cx + sx, cy + sy, 1), std::min(at(cx + sx, cy, 1), at(cx, cy + sy, 1)));
    return behind > 0.5f && front < 0.5f;
}
static bool IsDoorShape(GLenum mode, GLint first, GLsizei count) {
    // doors are thin (depth <= 0.34 tile), 0.7..2.1 tiles wide, 1.4..1.7 tall and stand on the floor; windows and wall fittings fail the last test
    if (mode != GL_TRIANGLES || count < 60 || count > 3000) return false;
    const ArrState& AS = GetArr();
    void* vp = AS.vp;
    GLint sz = AS.sz, ty = AS.ty, st = AS.st, vbo = AS.vbo;
    int stride = st ? st : sz * 4;
    static int vboSeen = 0;
    if (vbo && vboSeen < 3) {
        vboSeen++;
        Log("DOORSHAPE: a draw of %d vertices uses a vertex buffer (not readable here)", (int)count);
    }
    if (sz < 3 || ty != 0x1406 || vbo || !vp || !Readable((uint8_t*)vp + (size_t)first * stride, (size_t)count * stride)) return false;
    float mn[3] = {1e9f, 1e9f, 1e9f}, mx[3] = {-1e9f, -1e9f, -1e9f};
    for (int i = 0; i < count; i++) {
        const float* P = (const float*)((uint8_t*)vp + (size_t)(first + i) * stride);
        for (int k = 0; k < 3; k++) {
            if (P[k] < mn[k]) mn[k] = P[k];
            if (P[k] > mx[k]) mx[k] = P[k];
        }
    }
    float dx = mx[0] - mn[0], dy = mx[1] - mn[1], dz = mx[2] - mn[2];
    bool door = dy > 105.0f && dy < 175.0f && dz < 34.0f && dx > 70.0f && dx < 215.0f && mn[1] < 6.0f;
    static std::set<int> seen;
    if (door && seen.insert(count).second) Log("DOORSHAPE: door-like model with %d vertices (%.0f x %.0f x %.0f)", (int)count, dx, dy, dz);
    return door;
}
static bool ShiftDoor(GLenum mode, GLint first, GLsizei count) {
    if (!gCfg.doorFixes || !gCam.active || mode != GL_TRIANGLES) return false;      // 1.9.96: its own setting (was liftDoors, off - and the call was gone)
    if (!gObjDoor) return false;
    GLdouble mv[16];
    GetMV(mv);
    const ArrState& AS = GetArr();
    void* vp = AS.vp;
    GLint sz = AS.sz, ty = AS.ty, st = AS.st, vbo = AS.vbo;
    int stride = st ? st : sz * 4;
    if (sz < 3 || ty != 0x1406 || vbo || !vp || !Readable((uint8_t*)vp + (size_t)first * stride, (size_t)count * stride)) {
        if (!gObjDoor || !DoorXformPush()) return false;      // vertex buffer: cannot measure, but a door still gets its shift and cut
        oDrawArrays(mode, first, count);
        glPopMatrix();
        return true;
    }
    double minH = 1e9, rawMinY = 1e9, rawMaxY = -1e9, mnZ = 1e9, mxZ = -1e9, mnX = 1e9, mxX = -1e9;
    for (int i = 0; i < count; i++) {
        const float* Q = (const float*)((uint8_t*)vp + (size_t)(first + i) * stride);
        if (Q[1] < rawMinY) rawMinY = Q[1];
        if (Q[1] > rawMaxY) rawMaxY = Q[1];
        if (Q[2] < mnZ) mnZ = Q[2];
        if (Q[2] > mxZ) mxZ = Q[2];
        if (Q[0] < mnX) mnX = Q[0];
        if (Q[0] > mxX) mxX = Q[0];
        double ex = mv[0] * Q[0] + mv[4] * Q[1] + mv[8] * Q[2] + mv[12], ey = mv[1] * Q[0] + mv[5] * Q[1] + mv[9] * Q[2] + mv[13],
               ez = mv[2] * Q[0] + mv[6] * Q[1] + mv[10] * Q[2] + mv[14];
        double hh = gCfg.upSign * (gViewInv[2] * ex + gViewInv[6] * ey + gViewInv[10] * ez + gViewInv[14]);
        if (hh < minH) minH = hh;
    }
    if (rawMinY > 0.30) return false;      // base well above the floor: a window or wall fitting that happens to share a door's vertex count
    static int dl = 0;
    if (dl < 8) {
        dl++;
        Log("DOORGROUND %d verts, lowest point %.3f above the floor", (int)count, minH);
    }
    double d = (minH < 0.005 || minH > 1.5) ? -(double)gCfg.upSign * gCfg.wallLift
                                            : minH;      // sign flipped: doors were rising, not sinking   // unreliable reading: manual lift
    // (1.9.99: the lowering is always on again - the game draws doors on the walls' raised base; 1.9.96-98 left them floating)
    double e[3];
    for (int i = 0; i < 3; i++) e[i] = gViewInv[i * 4 + 2] * d;
    double ex0 = mv[12], ey0 = mv[13], ez0 = mv[14];
    double cxw = gViewInv[0] * ex0 + gViewInv[4] * ey0 + gViewInv[8] * ez0 + gViewInv[12],
           cyw = gViewInv[1] * ex0 + gViewInv[5] * ey0 + gViewInv[9] * ez0 + gViewInv[13];
    bool cutNow = DoorCutNow();      // 1.9.127 (user): doors stay up in cutaway, come down only with the walls down
    MarkDoorTiles(cxw, cyw);      // 1.9.106: the walls beside this door run to its tile edge (05_walls isoWallExt)

    // slide the door towards the outer face of the room: find which side of its cell is inside a room and which is not
    double sx = 0, sy = 0;
    const double dk = DoorDepthScale();
    const bool zWideS = (mxZ - mnZ) > (mxX - mnX) * 1.5;      // 1.9.109: thickness is x for a gate modelled along z
    const double depthW = (zWideS ? (mxX - mnX) * sqrt(mv[0] * mv[0] + mv[1] * mv[1] + mv[2] * mv[2])
                                  : (mxZ - mnZ) * sqrt(mv[8] * mv[8] + mv[9] * mv[9] + mv[10] * mv[10])) * dk;      // the door's thickness in tiles, as drawn
    double shiftAmt = gCfg.doorShift < 0.0f ? std::max(0.0, gCfg.wallThick * 0.5 - (depthW > 0.01 && depthW < 1.5 ? depthW * 0.5 : 0.08))
                                            : (double)gCfg.doorShift;      // auto: flush with the outer wall face
    if (shiftAmt > 0.001 && gRoomW > 0 && !gRoomBuf.empty()) {
        int cx = (int)floor(cxw), cy = (int)floor(cyw);
        auto room = [&](int x, int y) -> int {
            if (x < 0 || y < 0 || x >= gRoomW || y >= gRoomH) return -1;
            return gRoomBuf[((size_t)y * gRoomW + x) * 2 + 1] > 127 ? 1 : 0;
        };
        int rE = room(cx + 1, cy), rW = room(cx - 1, cy), rN = room(cx, cy + 1), rS = room(cx, cy - 1);
        if (rE >= 0 && rW >= 0 && rE != rW && rN == rS)
            sx = (rE == 0 ? 1.0 : -1.0) * shiftAmt;
        else if (rN >= 0 && rS >= 0 && rN != rS && rE == rW)
            sy = (rN == 0 ? 1.0 : -1.0) * shiftAmt;
    }
    {
        static int dl2 = 0;
        if (dl2 < 40 && cxw >= 0 && cxw < gRoomW && cyw >= 0 && cyw < gRoomH) {
            dl2++;
            Log("DOORSHIFT origin (%.2f, %.2f) shift (%.2f, %.2f) amt %.2f roomBuf %dx%d", cxw, cyw, sx, sy, shiftAmt, gRoomW, gRoomH);
        }
    }
    // a double door: back into its frame (undo the game's sideways slide), then the shader parts the two leaves by how far it is open
    GLint uSplit = -1;
    if (gDoorSplitOK && (unsigned)(gDrawSerial - gNoteSerial) <= 16 && fabs(gDoorPos[0] - cxw) < 2.5 && fabs(gDoorPos[1] - cyw) < 2.5) {
        auto lt = gLocs.find(gCurProg);
        if (lt != gLocs.end() && lt->second.udoor >= 0) {
            float half = 0.0f;      // half the door's width in model units, from its own vertices
            for (int i = 0; i < count; i++) half = std::max(half, fabsf(((const float*)((uint8_t*)vp + (size_t)(first + i) * stride))[0]));
            sx += gDoorPos[0] - cxw;
            sy += gDoorPos[1] - cyw;
            uSplit = lt->second.udoor;
            pUniform1f(uSplit, gDoorOpen * half * 0.92f);
        }
    }
    double dsh[3] = {sx, sy, 0.0};
    for (int i = 0; i < 3; i++) e[i] += gViewInv[i * 4 + 0] * dsh[0] + gViewInv[i * 4 + 1] * dsh[1];
    glPushMatrix();
    glLoadIdentity();
    glTranslated(e[0], e[1], e[2]);
    glMultMatrixd(mv);
    // 1.9.100 (user: too small): the door's height follows the walls - its top at the wall top (or at the cut stub when the walls are cut),
    // not the game's own door height shrunk to 60 %; the width fits the opening exactly
    // 1.9.103 (user: squished with the walls down): the door keeps its full height and is cut at the stub like the walls, not squashed
    double fitY = 1.0;
    {
        double doorH = (rawMaxY - rawMinY) * sqrt(mv[4] * mv[4] + mv[5] * mv[5] + mv[6] * mv[6]);
        if (gWallWorldH > 0.05f && doorH > 0.05) fitY = std::min(1.8, std::max(0.25, gWallWorldH * 0.97 / doorH));
    }
    GLint uClip = -1;
    if (cutNow) {
        auto lt = gLocs.find(gCurProg);
        if (lt != gLocs.end() && lt->second.udoorclip >= 0 && pUniform2f) {
            double keep = gCfg.wallMode == 2 ? gCfg.downStub : gCfg.stub;
            // which end of the model's y axis is the floor: model +y pointing up in the world -> keep below, else keep above
            double upY = gCfg.upSign * (gViewInv[2] * mv[4] + gViewInv[6] * mv[5] + gViewInv[10] * mv[6]);
            double range = rawMaxY - rawMinY;
            uClip = lt->second.udoorclip;
            if (upY >= 0) pUniform2f(uClip, 1.0f, (float)(rawMinY + range * keep));
            else pUniform2f(uClip, -1.0f, (float)(rawMaxY - range * keep));
        }
    }
    double fitX = 1.0;      // 1.9.94 (user: doors too wide, they overlap the walls): a door wider than its opening (1 or 2 tiles) is narrowed to fit it
    // 1.9.109 (user: gate and wall widened together, overlapping): some gates are modelled with their width along z, not x - then z is the
    // width (fitted to the opening) and x the thickness (following the walls)
    const bool zWide = (mxZ - mnZ) > (mxX - mnX) * 1.5;
    {
        double widthW = zWide ? (mxZ - mnZ) * sqrt(mv[8] * mv[8] + mv[9] * mv[9] + mv[10] * mv[10])
                              : (mxX - mnX) * sqrt(mv[0] * mv[0] + mv[1] * mv[1] + mv[2] * mv[2]);
        // 1.9.107 (user: gap beside the jail door): narrower doors are widened to their opening too (a 0.6-tile jail door left gaps)
        double opening = std::max(1.0, floor(widthW + 0.25));      // 1.9.109 (user: doors got wider): +0.5 rounded a 1.5-tile door up to 2 tiles
        if (fabs(widthW - opening) > opening * 0.005 && widthW > 0.35 && widthW < 3.5) fitX = opening / widthW;
    }
    if (dk != 1.0 || fitX != 1.0 || fitY != 1.0) {      // thickness follows the walls, width fits the opening, height the walls
        if (zWide) glScaled(dk, fitY, fitX);
        else glScaled(fitX, fitY, dk);
    }
    { static int zl = 0; if (zWide && zl < 4) { zl++; Log("DOORZWIDE a door / gate modelled along z (%d verts): width fitted on z, thickness on x", (int)count); } }
    GLfloat col[4] = {1, 1, 1, 1};
    glGetFloatv(GL_CURRENT_COLOR, col);
    bool ghost = col[3] < 0.98f && col[3] > 0.05f;      // 1.9.87: an open door is drawn a little see-through; the shader's blueprint look made it a blue pane in the doorway
    if (ghost) glColor4f(col[0], col[1], col[2], 1.0f);
    oDrawArrays(mode, first, count);
    if (ghost) glColor4fv(col);
    glPopMatrix();
    if (uSplit >= 0) pUniform1f(uSplit, 0.0f);
    if (uClip >= 0) pUniform2f(uClip, 0.0f, 0.0f);
    return true;
}
// Objects are placed against the edge of a tile; the game's walls are half a tile thick, so wall-mounted objects (toilet, bookcase, TV...)
// end up a quarter tile from the wall face. For each object whose bounding box touches a tile edge with a wall cell beyond it,
// slide it that gap towards the wall. Wall cells = those drawn in the last wall pass (gDrawnPrev).
static unsigned gSnapStat[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static int gTexUpFrame = 0;
static bool gObjTV = false;
static bool gSkipSwap = false;      // diagnostic: how far WallSnapObject calls get
static bool WallSnapObject(GLenum mode, GLint first, GLsizei count) {
    gSnapStat[0]++;
    if (!gObjSnapType) gSnapStat[7]++;
    if (gMenuWorld || gObjPersonExt || !gObjSnapType || (unsigned)(gDrawSerial - gNoteSerial) > 16) return false;
    gSnapStat[1]++;      // only by object type (toilet, bookcase...), never people or loose furniture such as chairs
    if (!gCfg.snapToWalls || !gCam.active || mode != GL_TRIANGLES || count < 30 || count > 3000 || !gCurProg)
        return false;      // the TV is 2214 vertices
    if (gDrawnPrev.empty()) return false;
    if (gObjDoor) return false;      // doors
    auto it = gLocs.find(gCurProg);
    if (it == gLocs.end() || it->second.wpass < 0) return false;
    struct SnapRec {
        int last, decided;
        double sx, sy, mnx, mny, mxx, mxy;
    };
    static std::unordered_map<long long, SnapRec> snapRec;
    extern int gWallFramesSeenExt;
    GLdouble mv[16];
    GetMV(mv);
    double ox = MVWorldX(mv),
           oy = MVWorldY(mv);
    long long k2 = (long long)lround(ox * 16.0) * 1000003LL + (long long)lround(oy * 16.0);
    int fr = gWallFramesSeenExt;
    {      // fast path: an object already decided and seen last frame keeps its shift without re-reading its vertices (most draws on a big prison)
        auto fs = snapRec.find(k2);
        if (fs != snapRec.end() && fs->second.decided >= 0 && fr - fs->second.decided <= 300 && fr - fs->second.last <= 1) {
            SnapRec& F = fs->second;
            F.last = fr;
            gSnapStat[4]++;
            if (F.sx == 0 && F.sy == 0) return false;
            gSnapStat[5]++;
            double d[3] = {F.sx, F.sy, 0.0}, e[3];
            for (int i = 0; i < 3; i++) e[i] = gViewInv[i * 4 + 0] * d[0] + gViewInv[i * 4 + 1] * d[1] + gViewInv[i * 4 + 2] * d[2];
            GLdouble M2[16];
            memcpy(M2, mv, sizeof M2);
            M2[12] += e[0];
            M2[13] += e[1];
            M2[14] += e[2];      // a translation in front of the modelview = add to its last column
            glPushMatrix();
            glLoadMatrixd(M2);
            oDrawArrays(mode, first, count);
            glPopMatrix();
            return true;
        }
    }
    const ArrState& AS = GetArr();
    void* vp = AS.vp;
    GLint sz = AS.sz, ty = AS.ty, st = AS.st, vbo = AS.vbo;
    int stride = st ? st : sz * 4;
    if (sz < 3 || ty != 0x1406 || vbo || !vp || !Readable((uint8_t*)vp + (size_t)first * stride, (size_t)count * stride)) return false;
    gSnapStat[2]++;
    static std::unordered_map<unsigned long long, std::array<float, 6>> cache;
    const float* A = (const float*)((uint8_t*)vp + (size_t)first * stride);
    const float* B = (const float*)((uint8_t*)vp + (size_t)(first + count / 2) * stride);
    unsigned long long key = (unsigned long long)count * 1000003ull ^ (unsigned long long)(int)(A[0] * 997.0f) * 7919ull ^
                             (unsigned long long)(int)(A[1] * 991.0f) * 104729ull ^ (unsigned long long)(int)(B[2] * 983.0f) * 15485863ull;
    auto cit = cache.find(key);
    std::array<float, 6> bb;
    if (cit != cache.end())
        bb = cit->second;
    else {
        bb = {1e9f, 1e9f, 1e9f, -1e9f, -1e9f, -1e9f};
        for (int i = 0; i < count; i++) {
            const float* P = (const float*)((uint8_t*)vp + (size_t)(first + i) * stride);
            for (int k = 0; k < 3; k++) {
                if (P[k] < bb[k]) bb[k] = P[k];
                if (P[k] > bb[3 + k]) bb[3 + k] = P[k];
            }
        }
        if (cache.size() > 4000) cache.clear();
        cache[key] = bb;
    }
    gSnapStat[3]++;
    if (gObjTV) {
        static std::set<int> seenC;
        if (seenC.size() < 12 && seenC.insert((int)count).second) {
            Log("TVDRAW count %d model x %.1f..%.1f y %.1f..%.1f z %.1f..%.1f", (int)count, bb[0], bb[3], bb[1], bb[4], bb[2], bb[5]);
        }
    }
    if (bb[1] > 20.0f) return false;      // base well above the floor: windows, wall lights and paintings are already on the wall
    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    for (int c = 0; c < 8; c++) {
        double px = (c & 1) ? bb[3] : bb[0], py = (c & 2) ? bb[4] : bb[1], pz = (c & 4) ? bb[5] : bb[2];
        double ex = mv[0] * px + mv[4] * py + mv[8] * pz + mv[12], ey = mv[1] * px + mv[5] * py + mv[9] * pz + mv[13],
               ez = mv[2] * px + mv[6] * py + mv[10] * pz + mv[14];
        double wx = gViewInv[0] * ex + gViewInv[4] * ey + gViewInv[8] * ez + gViewInv[12],
               wy = gViewInv[1] * ex + gViewInv[5] * ey + gViewInv[9] * ez + gViewInv[13];
        if (wx < minX) minX = wx;
        if (wx > maxX) maxX = wx;
        if (wy < minY) minY = wy;
        if (wy > maxY) maxY = wy;
    }
    if (maxX - minX > 12.0 || maxY - minY > 12.0 || maxX - minX < 0.05) return false;
    // The snap is decided once per OBJECT and then kept. An object is its model origin; all of its parts (a shower head is several draws) feed one
    // union bounding box, so every part gets the same shift (parts that decided separately used to drift apart and flicker). Re-decided every 300 frames.
    if (snapRec.size() > 30000) snapRec.clear();
    auto sr = snapRec.find(k2);
    double sx = 0, sy = 0;
    if (sr == snapRec.end()) {
        if ((bb[4] - bb[1]) < 0.03f) return false;
        snapRec[k2] = SnapRec{fr, -1, 0, 0, minX, minY, maxX, maxY};
        return false;
    }
    SnapRec& R = sr->second;
    int age = fr - R.last;
    if (age > 60) {
        if ((bb[4] - bb[1]) < 0.03f) return false;
        R = SnapRec{fr, -1, 0, 0, minX, minY, maxX, maxY};
        return false;
    }      // gone for a while: treat as new
    if (age > 0 && R.decided < 0) {
        R.last = fr;
        if (age > 1) {
            if ((bb[4] - bb[1]) >= 0.03f) {
                R.mnx = minX;
                R.mny = minY;
                R.mxx = maxX;
                R.mxy = maxY;
            }
            return false;
        }
    }
    const bool flatPart =
        (bb[4] - bb[1]) <
        0.03f;      // a flat part (the object's floor shadow / footprint plate) is wider than the object: it must not set where the object meets the wall
    R.last = fr;
    if (!flatPart) {
        R.mnx = std::min(R.mnx, minX);
        R.mny = std::min(R.mny, minY);
        R.mxx = std::max(R.mxx, maxX);
        R.mxy = std::max(R.mxy, maxY);
    }
    if (R.decided < 0 && age > 1) return false;      // not yet stable
    if (R.decided >= 0 && fr - R.decided <= 300) {
        sx = R.sx;
        sy = R.sy;
    } else {
        double gap = (1.0 - gCfg.wallThick) * 0.5;
        if (gap < 0.02) return false;
        double uMinX = R.mnx, uMaxX = R.mxx, uMinY = R.mny, uMaxY = R.mxy;      // the whole object, not just this part
        double cx = (uMinX + uMaxX) * 0.5, cy = (uMinY + uMaxY) * 0.5;
        auto isWall = [&](int x, int y) { return gDrawnPrev.count(y * 65536 + x) != 0; };
        const double T = gObjTV ? 0.6 : 0.3;      // televisions and toilets are pushed right back to the wall even when they sit well off it
        // per axis, only the nearer wall counts (a narrow cell has a wall on both sides)
        {
            double best = 1e9, s = 0, r = ceil(uMaxX - T);
            if (r - uMaxX < T && isWall((int)r, (int)floor(cy))) {
                best = r - uMaxX;
                s = r + gap - uMaxX;
            }
            r = floor(uMinX + T);
            if (uMinX - r < T && uMinX - r < best && isWall((int)r - 1, (int)floor(cy))) { s = -(uMinX - (r - gap)); }
            sx = s;
        }
        {
            double best = 1e9, s = 0, r = ceil(uMaxY - T);
            if (r - uMaxY < T && isWall((int)floor(cx), (int)r)) {
                best = r - uMaxY;
                s = r + gap - uMaxY;
            }
            r = floor(uMinY + T);
            if (uMinY - r < T && uMinY - r < best && isWall((int)floor(cx), (int)r - 1)) { s = -(uMinY - (r - gap)); }
            sy = s;
        }
        R.decided = fr;
        R.sx = sx;
        R.sy = sy;
        if (gObjTV) {
            static std::set<long long> seenT;
            if (seenT.size() < 30 && seenT.insert(k2).second)
                Log("TVSNAP at (%.2f,%.2f) bounds x %.2f..%.2f y %.2f..%.2f walls E %d W %d N %d S %d shift %.2f,%.2f around %s", ox, oy, uMinX,
                    uMaxX, uMinY, uMaxY, (int)isWall((int)ceil(uMaxX - T), (int)floor(cy)), (int)isWall((int)floor(uMinX + T) - 1, (int)floor(cy)),
                    (int)isWall((int)floor(cx), (int)ceil(uMaxY - T)), (int)isWall((int)floor(cx), (int)floor(uMinY + T) - 1), sx, sy, [&]() {
                        static char nb[64];
                        int k = 0;
                        for (int dy = 1; dy >= -1; dy--) {
                            for (int dx = -1; dx <= 1; dx++) nb[k++] = isWall((int)floor(cx) + dx, (int)floor(cy) + dy) ? '#' : '.';
                            nb[k++] = '/';
                        }
                        nb[k] = 0;
                        return (const char*)nb;
                    }());
        }
    }
    gSnapStat[4]++;
    if (sx == 0 && sy == 0) return false;
    gSnapStat[5]++;
    {
        static std::set<long long> seenS;
        if (seenS.size() < 60 && seenS.insert(k2).second)
            Log("SNAPPED object at (%.2f,%.2f) shift %.2f,%.2f bounds x %.2f..%.2f y %.2f..%.2f", ox, oy, sx, sy, R.mnx, R.mxx, R.mny, R.mxy);
    }
    double d[3] = {sx, sy, 0.0}, e[3];
    for (int i = 0; i < 3; i++) e[i] = gViewInv[i * 4 + 0] * d[0] + gViewInv[i * 4 + 1] * d[1] + gViewInv[i * 4 + 2] * d[2];
    GLdouble M2[16];
    memcpy(M2, mv, sizeof M2);
    M2[12] += e[0];
    M2[13] += e[1];
    M2[14] += e[2];
    glPushMatrix();
    glLoadMatrixd(M2);
    oDrawArrays(mode, first, count);
    glPopMatrix();
    return true;
}
// Diagnostic: how are the game's object models drawn? Logs distinct (kind, vbo, count) once each for draws that look like 3D models.
static void ModelDrawDiag(const char* kind, GLenum mode, GLsizei count) {
    if (!gCfg.diagLog) return;      // one-off diagnostic (Settings: diagLog)
    static int diagLines = 0;
    if (diagLines > 60 || !gCam.active || gInWallPass || count < 60 || count > 4000) return;
    GLint vb = 0, eb = 0, prog = 0;
    glGetIntegerv(0x8894, &vb);
    glGetIntegerv(0x8895, &eb);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    static std::set<unsigned long long> seen;
    static int lines = 0;
    if (lines > 60) return;
    unsigned long long key = ((unsigned long long)(unsigned)vb << 32) ^ ((unsigned long long)(unsigned)count << 8) ^ (kind[0] == 'e');
    if (!seen.insert(key).second) return;
    lines++;
    diagLines = lines;
    Log("MODELDRAW %s mode 0x%x count %d vbo %d ebo %d prog %d tex0 %u door-tag %d", kind, (unsigned)mode, (int)count, (int)vb, (int)eb, (int)prog,
        gBound[0], (int)gObjDoor);
}
// Wall fittings (windows, wall lights, paintings, signs) hang high on the wall. When their wall is lowered by the cutaway they would float
// in mid air, so skip drawing them while their wall is cut. They are recognised by a model base well above the floor.
// Wall fittings (signs, hooks, paintings, windows' trims) are placed for the game's half-tile wall; the 3D walls are thicker, so they sank into
// the wall. Push each one out along its thin axis until it stands just proud of the wall face.
static bool PokeFitting(GLenum mode, GLint first, GLsizei count, const float* mn, const float* mx, const GLdouble* mv) {
    if (!gCfg.pokeFittings || gDrawnPrev.empty()) return false;
    double wmn[2] = {1e9, 1e9}, wmx[2] = {-1e9, -1e9};
    for (int c = 0; c < 8; c++) {
        double px = (c & 1) ? mx[0] : mn[0], py = (c & 2) ? mx[1] : mn[1], pz = (c & 4) ? mx[2] : mn[2];
        double ex = mv[0] * px + mv[4] * py + mv[8] * pz + mv[12], ey = mv[1] * px + mv[5] * py + mv[9] * pz + mv[13],
               ez = mv[2] * px + mv[6] * py + mv[10] * pz + mv[14];
        double w[2] = {gViewInv[0] * ex + gViewInv[4] * ey + gViewInv[8] * ez + gViewInv[12],
                       gViewInv[1] * ex + gViewInv[5] * ey + gViewInv[9] * ez + gViewInv[13]};
        for (int k = 0; k < 2; k++) {
            if (w[k] < wmn[k]) wmn[k] = w[k];
            if (w[k] > wmx[k]) wmx[k] = w[k];
        }
    }
    double ext[2] = {wmx[0] - wmn[0], wmx[1] - wmn[1]};
    int a = ext[0] < ext[1] ? 0 : 1;      // thin axis in the world
    if (ext[a] > 0.3 || ext[1 - a] > 3.5) return false;
    double c[2] = {(wmn[0] + wmx[0]) * 0.5, (wmn[1] + wmx[1]) * 0.5};
    int cx = (int)floor(c[0]), cy = (int)floor(c[1]);
    if (!gDrawnPrev.count(cy * 65536 + cx)) return false;      // only fittings inside a wall tile
    double wc = (a == 0 ? cx : cy) + 0.5, off = c[a] - wc;
    if (fabs(off) < 0.02) return false;      // centred: no side to push to
    double n = off > 0 ? 1.0 : -1.0;
    double want = wc + n * (gCfg.wallThick * 0.5 + ext[a] * 0.5 + gCfg.fittingPoke);
    double s = want - c[a];
    if (s * n <= 0.0) return false;      // already proud of the wall
    double d[3] = {a == 0 ? s : 0.0, a == 1 ? s : 0.0, 0.0}, e[3];
    for (int i = 0; i < 3; i++) e[i] = gViewInv[i * 4 + 0] * d[0] + gViewInv[i * 4 + 1] * d[1] + gViewInv[i * 4 + 2] * d[2];
    glPushMatrix();
    glLoadIdentity();
    glTranslated(e[0], e[1], e[2]);
    glMultMatrixd(mv);
    oDrawArrays(mode, first, count);
    glPopMatrix();
    static int pl = 0;
    if (pl < 10) {
        pl++;
        Log("POKE fitting at (%.2f,%.2f) pushed %.2f out of the wall", c[0], c[1], s * n);
    }
    return true;
}
static bool HideCutFitting(GLenum mode, GLint first, GLsizei count) {
    if (gMenuWorld || gObjPersonExt) return false;
    if (!gCfg.hideCutFittings || !gCam.active || !gCurProg || mode != GL_TRIANGLES || count < 24 || count > 6000 || gObjDoor)
        return false;      // no program = the game's UI (text and icons): never a wall fitting
    const ArrState& AS = GetArr();
    void* vp = AS.vp;
    GLint sz = AS.sz, ty = AS.ty, st = AS.st, vbo = AS.vbo;
    int stride = st ? st : sz * 4;
    if (sz < 3 || ty != 0x1406 || vbo || !vp) return false;
    if (count > 900) return false;      // people and big models are never wall fittings
    // most draws are furniture: remember per model array whether it is a thin high plate at all, so they cost one hash lookup
    static std::unordered_map<unsigned long long, char> fitMemo;
    unsigned long long fk = ((unsigned long long)(uintptr_t)vp + (unsigned long long)first * stride) * 31ull + (unsigned long long)count;
    auto fm = fitMemo.find(fk);
    if (fm != fitMemo.end() && fm->second == 0) return false;
    if (!Readable((uint8_t*)vp + (size_t)first * stride, (size_t)count * stride)) return false;
    float mn[3] = {1e9f, 1e9f, 1e9f}, mx[3] = {-1e9f, -1e9f, -1e9f};
    for (int i = 0; i < count; i++) {
        const float* Q = (const float*)((uint8_t*)vp + (size_t)(first + i) * stride);
        for (int k = 0; k < 3; k++) {
            if (Q[k] < mn[k]) mn[k] = Q[k];
            if (Q[k] > mx[k]) mx[k] = Q[k];
        }
    }
    // models are in tile units, centred on their origin: a fitting is a thin plate (one side under a fifth of a tile and well under its longest side)
    float d0 = mx[0] - mn[0], d1 = mx[1] - mn[1], d2 = mx[2] - mn[2];
    float dimMin = std::min(d0, std::min(d1, d2)), dimMax = std::max(d0, std::max(d1, d2));
    bool thin = dimMin < 0.2f && dimMin < dimMax * 0.35f && dimMax < 3.5f;
    if (fitMemo.size() > 20000) fitMemo.clear();
    fitMemo[fk] = thin ? 1 : 0;
    if (!thin) return false;
    {
        GLdouble P[16];
        GetPR(P);
        for (int i = 0; i < 16; i++)
            if (fabs(P[i] - gCamP[i]) > 1e-9) return false;
    }      // only under the 3D camera
    GLdouble mv[16];
    GetMV(mv);
    double minH = 1e9;      // its lowest point above the floor, in tiles: fittings hang on the wall, things on the floor are not fittings
    for (int c = 0; c < 8; c++) {
        double px = (c & 1) ? mx[0] : mn[0], py = (c & 2) ? mx[1] : mn[1], pz = (c & 4) ? mx[2] : mn[2];
        double ex = mv[0] * px + mv[4] * py + mv[8] * pz + mv[12], ey = mv[1] * px + mv[5] * py + mv[9] * pz + mv[13],
               ez = mv[2] * px + mv[6] * py + mv[10] * pz + mv[14];
        double hh = gCfg.upSign * (gViewInv[2] * ex + gViewInv[6] * ey + gViewInv[10] * ez + gViewInv[14]);
        if (hh < minH) minH = hh;
    }
    if (minH < 0.25) return false;
    double cxw = MVWorldX(mv),
           cyw = MVWorldY(mv);
    // test the wall the fitting hangs on, not its origin: a painting's origin is the room tile in front of the wall, which read as "cut" and hid it
    double wmn[2] = {1e9, 1e9}, wmx[2] = {-1e9, -1e9};
    for (int c = 0; c < 8; c++) {
        double px = (c & 1) ? mx[0] : mn[0], py = (c & 2) ? mx[1] : mn[1], pz = (c & 4) ? mx[2] : mn[2];
        double ex = mv[0] * px + mv[4] * py + mv[8] * pz + mv[12], ey = mv[1] * px + mv[5] * py + mv[9] * pz + mv[13],
               ez = mv[2] * px + mv[6] * py + mv[10] * pz + mv[14];
        double w[2] = {gViewInv[0] * ex + gViewInv[4] * ey + gViewInv[8] * ez + gViewInv[12],
                       gViewInv[1] * ex + gViewInv[5] * ey + gViewInv[9] * ez + gViewInv[13]};
        for (int k = 0; k < 2; k++) {
            if (w[k] < wmn[k]) wmn[k] = w[k];
            if (w[k] > wmx[k]) wmx[k] = w[k];
        }
    }
    double fc[2] = {(wmn[0] + wmx[0]) * 0.5, (wmn[1] + wmx[1]) * 0.5};
    int ta = (wmx[0] - wmn[0]) < (wmx[1] - wmn[1]) ? 0 : 1;
    double org[2] = {cxw, cyw}, tw[2] = {fc[0], fc[1]};
    if (!gDrawnPrev.count((int)floor(tw[1]) * 65536 + (int)floor(tw[0])) && fabs(fc[ta] - org[ta]) > 0.05)
        tw[ta] += (fc[ta] > org[ta] ? 0.5 : -0.5);      // step onto the wall tile behind it
    float minY = (float)minH;
    if (!WallCutAt(tw[0], tw[1])) return PokeFitting(mode, first, count, mn, mx, mv);
    static int il = 0;
    if (il < 10) {
        il++;
        Log("HIDECUT fitting base %.2f at (%.2f, %.2f), wall tile (%d, %d), hidden with its wall", minY, cxw, cyw, (int)floor(tw[0]),
            (int)floor(tw[1]));
    }
    return true;
}
struct PerfAcc {
    double t[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};      // 5 loose items, 6 off-screen cull, 7 object notes, 8 whole draw hook (incl. the GL call), 9 SwapBuffers, 10 (spare), 11 outline pass
    unsigned draws = 0;      // 1.9.123: draw calls in the sampled frames
    int frames = 0;
};
static PerfAcc gPerf;
// Diagnostic for a wall cell the game builds no piece for: once in a while, log every draw whose model origin lies within a tile of the first missing cell.
static void NearCellDiag(GLenum mode, GLint first, GLsizei count) {
    if (!gCfg.diagLog) return;      // one-off diagnostic (Settings: diagLog)
    // Diagnostic: three times (20 s apart), log every thin, wide, non-wall model in one frame (flat things such as the white benches), with its world position.
    static double armedAt = 0;
    static int armedFrames = 0;
    static int armedFrameId = -1;
    static int lines = 0;
    extern int gWallFramesSeenExt;
    if (!gCam.active || armedFrames >= 4 || mode != GL_TRIANGLES || count < 6 || count > 3000 || gInWallPass) return;
    double now = NowSec();
    if (armedFrameId != gWallFramesSeenExt) {
        if (armedFrameId >= 0 && now - armedAt < 20.0) return;
        if (armedAt == 0 && gWallFramesSeenExt < 400) return;
        armedAt = now;
        armedFrameId = gWallFramesSeenExt;
        armedFrames++;
        lines = 0;
        Log("FLATDIAG frame %d start", armedFrames);
    }
    if (lines >= 120) return;
    const ArrState& AS = GetArr();
    void* vp = AS.vp;
    GLint sz = AS.sz, ty = AS.ty, st = AS.st, vbo = AS.vbo;
    int stride = st ? st : sz * 4;
    if (sz < 3 || ty != 0x1406 || vbo || !vp || !Readable((uint8_t*)vp + (size_t)first * stride, (size_t)count * stride)) return;
    float mn[3] = {1e9f, 1e9f, 1e9f}, mx[3] = {-1e9f, -1e9f, -1e9f};
    for (int i = 0; i < count; i++) {
        const float* Q = (const float*)((uint8_t*)vp + (size_t)(first + i) * stride);
        for (int k = 0; k < 3; k++) {
            if (Q[k] < mn[k]) mn[k] = Q[k];
            if (Q[k] > mx[k]) mx[k] = Q[k];
        }
    }
    float d[3] = {mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2]};
    float big = std::max(d[0], std::max(d[1], d[2])), small = std::min(d[0], std::min(d[1], d[2]));
    if (count == 900 || gCurProg == 0 || big < 2.0f || small > big * 0.03f)
        return;      // 900 = per-cell light / fog strips, program 0 = UI text: both flood the log                       // flat in any units (model space or tile space)
    static std::set<unsigned long long> seenSig;
    if (lines == 0) seenSig.clear();
    {
        unsigned long long sg = ((unsigned long long)count << 40) ^ ((unsigned long long)(int)(d[0] * 10) << 20) ^
                                ((unsigned long long)(int)(d[1] * 10) << 8) ^ gBound[0] ^ ((unsigned long long)gCurProg << 4);
        if (!seenSig.insert(sg).second) return;
    }
    GLdouble mv[16];
    GetMV(mv);
    double cxw = MVWorldX(mv),
           cyw = MVWorldY(mv);
    lines++;
    Log("FLATDIAG draw mode 0x%x count %d dims %.2f x %.2f x %.2f min (%.2f,%.2f,%.2f) at (%.1f,%.1f) prog %d tex0 %u person %d snap %d bench %d",
        (unsigned)mode, (int)count, d[0], d[1], d[2], mn[0], mn[1], mn[2], cxw, cyw, (int)gCurProg, gBound[0], (int)gObjPersonExt, (int)gObjSnapType,
        (int)gObjBench);
}
static double gQpcMs = 0;
static bool gPerfOn = false;      // timing is sampled one frame in 8 (two clock reads per check per draw cost real time on big prisons)
static inline double PerfNow() {
    LARGE_INTEGER q;
    if (gQpcMs == 0) {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        gQpcMs = 1000.0 / (double)f.QuadPart;
    }
    QueryPerformanceCounter(&q);
    return (double)q.QuadPart * gQpcMs;
}
#define PERF(slot, expr)                                                                                                                             \
    (gPerfOn ? [&] {                                                                                                                                 \
        double _t0 = PerfNow();                                                                                                                      \
        bool _r = (expr);                                                                                                                            \
        gPerf.t[slot] += PerfNow() - _t0;                                                                                                            \
        return _r;                                                                                                                                   \
    }()                                                                                                                                              \
             : (expr))
// Diagnostic for the missing UI text: in a few sampled frames, log every draw made under a non-camera (UI) projection with its screen bounds,
// and say which of our hooks (if any) dropped it.
static bool gUiTrace = false;
static void UiTraceArm() {
    if (!gCfg.diagLog) return;      // one-off diagnostic (Settings: diagLog)
    static double armedAt = 0;
    static int frames = 0;
    extern int gWallFramesSeenExt;
    static int lastFrame = -1;
    if (lastFrame == gWallFramesSeenExt) return;
    lastFrame = gWallFramesSeenExt;
    double now = NowSec();
    if (gUiTrace) { gUiTrace = false; }
    if (!gCam.active || gWallFramesSeenExt < 600 || frames >= 5 || now - armedAt < 20.0) return;
    armedAt = now;
    frames++;
    gUiTrace = true;
    Log("UITRACE frame %d start", frames);
}
static void UiTraceLog(const char* verdict, GLenum m, GLint f, GLsizei c) {
    if (!gCfg.diagLog) return;      // one-off diagnostic (Settings: diagLog)
    static int lines = 0;
    static int perFrame = 0;
    static double lastT = 0;
    double nw = NowSec();
    if (nw - lastT > 1.0) {
        perFrame = 0;
        lastT = nw;
    }
    if (lines > 700 || perFrame > 160) return;
    lines++;
    perFrame++;
    const ArrState& AS = GetArr();
    float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
    int stride = AS.st ? AS.st : AS.sz * 4;
    if (AS.sz >= 2 && AS.ty == 0x1406 && !AS.vbo && AS.vp && c > 0 && Readable((uint8_t*)AS.vp + (size_t)f * stride, (size_t)c * stride))
        for (int i = 0; i < c; i++) {
            const float* Q = (const float*)((uint8_t*)AS.vp + (size_t)(f + i) * stride);
            if (Q[0] < mnx) mnx = Q[0];
            if (Q[0] > mxx) mxx = Q[0];
            if (Q[1] < mny) mny = Q[1];
            if (Q[1] > mxy) mxy = Q[1];
        }
    Log("UIDRAW %s mode 0x%x count %d prog %d tex0 %u vbo %d bounds (%.0f,%.0f)-(%.0f,%.0f)", verdict, (unsigned)m, (int)c, (int)gCurProg, gBound[0],
        (int)AS.vbo, mnx, mny, mxx, mxy);
}
// Diagnostic for the alert beacon (green cone / halo around a selected or alerting person) that ends up hidden under 3D objects: in a few sampled frames,
// log every translucent, coloured world-space draw (blend on, current colour alpha < 1) with its colour, texture, program and bounds.
// Alert beacons (the halo, icon and beam around a person the game wants you to see) are small immediate-mode shapes drawn with the depth test off,
// early in the frame; the 3D objects and walls drawn afterwards then paint over them. Such draws are recorded into display lists and replayed once
// all the 3D drawing is done (when the first UI draw arrives), with the state they were drawn with.
struct DeferredOverlay {
    GLuint list;
    GLfloat col[4];
    GLuint tex;
    GLint src, dst;
    GLboolean tex2d;
    GLdouble mv[16], pr[16];
};
static std::vector<DeferredOverlay> gDeferred;
static DeferredOverlay gDeferPending;
static bool gDeferRec = false;
static int gTransFramesLeft = 0;
static const char* gFlushWhy = "end of frame";
static GLuint gFlushTex = 0;
static int gFlushCount = 0;
static void FlushOverlays() {
    if (gDeferred.empty()) return;
    {
        static int fl = 0;
        if (gTransFramesLeft > 0 && fl < 60) {
            fl++;
            Log("OVERLAYS replayed: %d lists, triggered by %s (tex %u, count %d)", (int)gDeferred.size(), gFlushWhy, gFlushTex, gFlushCount);
        }
    }
    GLint prog = 0, at = GL_TEXTURE0, mmode = GL_MODELVIEW;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &at);
    glGetIntegerv(GL_MATRIX_MODE, &mmode);
    PFN_UseProgram use = oUseProgram ? oUseProgram : oUseProgramObjectARB;
    if (prog && use) use(0);
    if (pActiveTexture) pActiveTexture(GL_TEXTURE0);
    GLint bt = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bt);
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    for (auto& d : gDeferred) {
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixd(d.pr);
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixd(d.mv);
        if (oBindTexture) oBindTexture(GL_TEXTURE_2D, d.tex);
        if (d.tex2d)
            glEnable(GL_TEXTURE_2D);
        else
            glDisable(GL_TEXTURE_2D);
        glBlendFunc((GLenum)d.src, (GLenum)d.dst);
        glColor4fv(d.col);
        glCallList(d.list);
        glDeleteLists(d.list, 1);
    }
    gDeferred.clear();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
    if (oBindTexture) oBindTexture(GL_TEXTURE_2D, (GLuint)bt);
    if (pActiveTexture) pActiveTexture((GLenum)at);
    glMatrixMode((GLenum)mmode);
    if (prog && use) use((GLuint)prog);
}
// 1.9.125 (user: "maybe add a darker outline to objects"): a dark line round the outer edges of objects, people and walls, like the 2D art.
// Once a frame, when the world is finished (the first UI draw), the depth buffer is copied to a texture and a full-screen pass darkens the
// pixels where the depth jumps (a second difference: flat and sloped surfaces give 0, only real edges show). The inner edges stay the per-object
// isoOutline's. Settings: silhouette (0 = off). The line is 1-2 screen pixels either side of the edge at 1080p, scaled with the window.
static const char* kSilVS = R"GLSL(#version 120
void main() { gl_Position = gl_Vertex; gl_TexCoord[0] = vec4(gl_Vertex.xy * 0.5 + 0.5, 0.0, 0.0); }
)GLSL";
static const char* kSilFS = R"GLSL(#version 120
uniform sampler2D dep;
uniform vec2 px;
uniform vec3 lin;      // projection [10], [14], 1 = orthographic
uniform float dark;
float D(vec2 uv) {      // distance from the eye (larger = farther) in both projections
    float z = texture2D(dep, uv).r * 2.0 - 1.0;
    return lin.z > 0.5 ? (lin.y - z) / lin.x : lin.y / (z + lin.x);
}
// 1.9.126 (user: dashed seams at wall joins): hairline cracks between pieces (the floor seen through a sub-pixel gap) were drawn as dashed
// lines. Each tap takes the nearest depth of its little cross, which closes gaps of a pixel or two before the edges are looked for.
float M(vec2 uv) {
    vec2 dx = vec2(px.x, 0.0), dy = vec2(0.0, px.y);
    return min(D(uv), min(min(D(uv - dx), D(uv + dx)), min(D(uv - dy), D(uv + dy))));
}
void main() {
    vec2 uv = gl_TexCoord[0].xy;
    float c = M(uv);
    float l = M(uv - vec2(px.x, 0.0)), r = M(uv + vec2(px.x, 0.0)), u = M(uv + vec2(0.0, px.y)), d = M(uv - vec2(0.0, px.y));
    // 1.9.128 (user: too thick zoomed out): only the pixels on the NEAR side of a depth step (the object's own edge) darken - the line was
    // drawn on both sides of the step, two to three pixels wide
    float e = max(l + r - 2.0 * c, u + d - 2.0 * c);      // > 0: this pixel is nearer than its neighbours (the edge of the front object)
    float a = dark * smoothstep(0.05, 0.16, e);
    if (a < 0.01) discard;
    gl_FragColor = vec4(0.07, 0.05, 0.04, a);
}
)GLSL";
static void DrawSilhouettes(const GLint* vp) {
    static bool failed = false;
    static GLuint prog = 0, tex = 0;
    static GLint locDep = -1, locPx = -1, locLin = -1, locDark = -1;
    static int tw = 0, th = 0, errChecks = 3;
    if (failed || vp[2] <= 0 || vp[3] <= 0) return;
    LoadGlFuncs();
    PFN_UseProgram use = oUseProgram ? oUseProgram : oUseProgramObjectARB;
    if (!use || !pActiveTexture || !pGetUniformLocation || !pUniform1i || !pUniform1f || !pUniform2f || !pUniform3f || !oWglGetProcAddress) return;
    if (!prog) {
        typedef GLuint(APIENTRY * PFN_CS)(GLenum);
        typedef void(APIENTRY * PFN_SS)(GLuint, GLsizei, const char* const*, const GLint*);
        typedef void(APIENTRY * PFN_U)(GLuint);
        typedef GLuint(APIENTRY * PFN_CP)(void);
        typedef void(APIENTRY * PFN_AS)(GLuint, GLuint);
        typedef void(APIENTRY * PFN_GI)(GLuint, GLenum, GLint*);
        typedef void(APIENTRY * PFN_GL)(GLuint, GLsizei, GLsizei*, char*);
        PFN_CS cs = (PFN_CS)oWglGetProcAddress("glCreateShader");
        PFN_SS ss = (PFN_SS)oWglGetProcAddress("glShaderSource");
        PFN_U comp = (PFN_U)oWglGetProcAddress("glCompileShader"), link = (PFN_U)oWglGetProcAddress("glLinkProgram");
        PFN_CP cp = (PFN_CP)oWglGetProcAddress("glCreateProgram");
        PFN_AS as = (PFN_AS)oWglGetProcAddress("glAttachShader");
        PFN_GI gsi = (PFN_GI)oWglGetProcAddress("glGetShaderiv"), gpi = (PFN_GI)oWglGetProcAddress("glGetProgramiv");
        PFN_GL gsl = (PFN_GL)oWglGetProcAddress("glGetShaderInfoLog");
        failed = true;
        if (!cs || !ss || !comp || !link || !cp || !as || !gsi || !gpi) return;
        GLuint sh[2] = {cs(0x8B31), cs(0x8B30)};
        const char* src[2] = {kSilVS, kSilFS};
        for (int i = 0; i < 2; i++) {
            ss(sh[i], 1, &src[i], nullptr);
            comp(sh[i]);
            GLint ok = 0;
            gsi(sh[i], 0x8B81, &ok);
            if (!ok) {
                char lg[512] = "";
                if (gsl) gsl(sh[i], sizeof lg, nullptr, lg);
                Log("ERROR outline pass shader %d: %s", i, lg);
                return;
            }
        }
        prog = cp();
        as(prog, sh[0]);
        as(prog, sh[1]);
        link(prog);
        GLint ok = 0;
        gpi(prog, 0x8B82, &ok);
        if (!ok) { Log("ERROR outline pass: link failed"); return; }
        locDep = pGetUniformLocation(prog, "dep");
        locPx = pGetUniformLocation(prog, "px");
        locLin = pGetUniformLocation(prog, "lin");
        locDark = pGetUniformLocation(prog, "dark");
        glGenTextures(1, &tex);
        failed = false;
        Log("Outline pass ready (program %u)", prog);
    }
    GLint act = GL_TEXTURE0, prevTex = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &act);
    pActiveTexture(GL_TEXTURE0 + 7);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
    if (oBindTexture) oBindTexture(GL_TEXTURE_2D, tex); else glBindTexture(GL_TEXTURE_2D, tex);
    if (tw != vp[2] || th != vp[3]) {
        tw = vp[2];
        th = vp[3];
        glTexImage2D(GL_TEXTURE_2D, 0, 0x81A6 /*DEPTH_COMPONENT24*/, tw, th, 0, 0x1902 /*DEPTH_COMPONENT*/, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
        glTexParameteri(GL_TEXTURE_2D, 0x884C /*COMPARE_MODE*/, GL_NONE);
        glTexParameteri(GL_TEXTURE_2D, 0x884B /*DEPTH_TEXTURE_MODE*/, GL_LUMINANCE);
    }
    if (errChecks > 0) while (glGetError() != GL_NO_ERROR) {}
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vp[0], vp[1], tw, th);
    if (errChecks > 0) {      // the first few frames: a multisampled screen cannot be copied (driver setting) - then the pass turns itself off
        errChecks--;
        GLenum er = glGetError();
        if (er != GL_NO_ERROR) {
            Log("WARN outline pass: the screen's depth cannot be copied (GL error 0x%x) - outline pass off", (unsigned)er);
            failed = true;
        }
    }
    if (!failed) {
        glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_CURRENT_BIT | GL_POLYGON_BIT);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        use(prog);
        const float s = std::max(1.0f, vp[3] / 1080.0f);
        pUniform1i(locDep, 7);
        pUniform2f(locPx, s / tw, s / th);
        pUniform3f(locLin, (float)gCamP[10], (float)gCamP[14], gCamP[15] == 1.0 ? 1.0f : 0.0f);
        {      // 1.9.128 (user: "when zoomed out it's too thick, when zoomed in it's just right"): the line is a pixel wide at any zoom, so far
               // out it covers more of each object - it fades from full at 60 pixels a tile to a quarter at 15
            double ppt = fabs(gCamP[5]) * vp[3] * 0.5 / s;      // pixels per tile (orthographic camera), in line widths (2 px at 4K)
            float zf = gCamP[15] == 1.0 ? (float)std::min(1.0, std::max(0.25, (ppt - 15.0) / 45.0 * 0.75 + 0.25)) : 1.0f;
            pUniform1f(locDark, std::min(1.0f, gCfg.silhouette) * zf);
        }
        glBegin(GL_TRIANGLE_STRIP);
        glVertex2f(-1, -1);
        glVertex2f(1, -1);
        glVertex2f(-1, 1);
        glVertex2f(1, 1);
        glEnd();
        use(gCurProg);
        glPopAttrib();
    }
    if (oBindTexture) oBindTexture(GL_TEXTURE_2D, (GLuint)prevTex); else glBindTexture(GL_TEXTURE_2D, (GLuint)prevTex);
    pActiveTexture((GLenum)act);
}
static int gSilFrame = -1, gCamDrawFrame = -1;
static GLuint gCamFbo = 0;      // 1.9.126: the frame buffer the world is drawn into (the outline pass looked for the screen's own, 0, only)
static unsigned gSilDiag[8] = {0};      // 1.9.126 (1.9.125's outline never ran): world draws, other projections, in the world's buffer, flat, full view, passes
static inline void FlushOverlaysIfUi() {      // the first draw under a non-camera projection after the world = the UI begins
    const bool silDue = gCfg.silhouette > 0.0f && gSilFrame != gSwapCount && !gMenuWorld;
    {
        static int at = 0; static const int when[3] = {600, 3000, 18000};
        if (at < 3 && gSwapCount >= when[at] && !gMenuWorld) {
            at++;
            Log("Outline check after %d frames: world drawn %u times (frame buffer %u), other projections %u, in the world's buffer %u, flat %u, "
                "full view %u, passes drawn %u (setting %.2f, camera on %d)", gSwapCount, gSilDiag[0], gCamFbo, gSilDiag[1], gSilDiag[2], gSilDiag[3],
                gSilDiag[4], gSilDiag[5], gCfg.silhouette, (int)gCam.active);
        }
    }
    if ((gDeferred.empty() && !silDue) || gInWallPass || !gCam.active) return;
    // 1.9.125: the verdict is kept until the projection or frame buffer changes - this runs before every draw
    static unsigned negGen = 0;
    static GLuint negFbo = ~0u;
    static int negFrame = -1;
    if (gMatMode != GL_PROJECTION && negGen == gProjGen && negFbo == gCurFbo && negFrame == gSwapCount) return;
    negGen = gProjGen;
    negFbo = gCurFbo;
    negFrame = gSwapCount;
    GLdouble P[16];
    GetPR(P);
    bool other = false;
    for (int i = 0; i < 16; i++)
        if (fabs(P[i] - gCamP[i]) > 1e-9) {
            other = true;
            break;
        }
    if (!other) {
        gSilDiag[0]++;
        gCamFbo = gCurFbo;      // the world is being drawn (to the screen or the game's main buffer) this frame
        gCamDrawFrame = gSwapCount;
        return;
    }
    gSilDiag[1]++;
    // mid-frame render-to-texture passes (light map, prerender groups) also use other projections: only the world's own buffer counts as the UI starting
    if (gCurFbo != gCamFbo) return;
    gSilDiag[2]++;
    if (P[15] != 1.0) return;      // the UI is orthographic
    gSilDiag[3]++;
    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);
    if (!IsMainViewport(vp[2], vp[3])) return;
    gSilDiag[4]++;
    negGen = 0;      // a UI draw: check again next time (until done)
    if (silDue && gCamDrawFrame == gSwapCount) {
        gSilDiag[5]++;
        gSilFrame = gSwapCount;
        double t0 = gPerfOn ? PerfNow() : 0;
        DrawSilhouettes(vp);
        if (gPerfOn) gPerf.t[11] += PerfNow() - t0;
    }
    FlushOverlays();
}      // (gTransFramesLeft is declared above)      // set by F11: capture the next few frames (the beacon only lasts a few seconds)
static std::set<unsigned long long> gTransSeen;
static void TransDiag(const char* kind, GLenum mode, GLint first, GLsizei count) {
    if (!gCfg.diagLog) return;      // one-off diagnostic (Settings: diagLog)
    static int lastFrame = -1, perFrame = 0, shot = 0;
    static bool on = false;
    extern int gWallFramesSeenExt;
    if (lastFrame != gWallFramesSeenExt) {
        lastFrame = gWallFramesSeenExt;
        on = false;
        perFrame = 0;
        if (gTransFramesLeft > 0 && gCam.active) {
            gTransFramesLeft--;
            on = true;
            if (gTransFramesLeft == 2) gTransSeen.clear();
            shot++;
            Log("TRANSDIAG capture frame %d (press F11 again for another)", shot);
        }
    }
    {
        static bool wasWall = false;
        if (on && gInWallPass != wasWall) {
            wasWall = gInWallPass;
            Log("TRANSDIAG ---- wall pass %s ----", gInWallPass ? "START" : "END");
        } else if (!on)
            wasWall = gInWallPass;
    }
    if (!on || gInWallPass || perFrame >= 150 || count == 900 || count > 400) return;      // 900 = the per-cell light strips, which flood the log
    GLfloat col[4];
    glGetFloatv(GL_CURRENT_COLOR, col);
    if (!glIsEnabled(GL_BLEND)) return;
    {
        auto& seenSig = gTransSeen;
        unsigned long long sg = ((unsigned long long)gCurProg << 40) ^ ((unsigned long long)gBound[0] << 20) ^ (unsigned long long)count ^
                                ((unsigned long long)(col[0] * 10) << 12) ^ ((unsigned long long)(col[1] * 10) << 8) ^
                                ((unsigned long long)(col[3] * 10) << 4) ^ ((unsigned long long)mode << 56);
        if (!seenSig.insert(sg).second) return;
    }
    GLdouble P[16];
    GetPR(P);
    bool cam3d = true;
    for (int i = 0; i < 16; i++)
        if (fabs(P[i] - gCamP[i]) > 1e-9) {
            cam3d = false;
            break;
        }
    if (!cam3d) return;
    GLboolean dt = glIsEnabled(GL_DEPTH_TEST), dm = 1;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &dm);
    GLint bs = 0, bd = 0;
    glGetIntegerv(0x80C9, &bs);
    glGetIntegerv(0x80C8, &bd);
    perFrame++;
    Log("TRANSDIAG %s mode 0x%x count %d prog %d tex0 %u colour %.2f %.2f %.2f %.2f depthTest %d depthWrite %d blend %d->%d", kind, (unsigned)mode,
        (int)count, (int)gCurProg, gBound[0], col[0], col[1], col[2], col[3], (int)dt, (int)dm, bs, bd);
}      // Objects whose origin is in a wall tile (shop fronts, wall-mounted things) took the wall tile's light, which is dark. Take it from the open tile they face.
static void ObjectLightAndShopFront(GLenum mode, GLint first, GLsizei count) {
    static float lastNudge[2] = {0, 0};
    static GLuint lastProg = 0;
    if (!gCam.active || gInWallPass || !gCurProg || gObjPersonExt || gMenuWorld) return;
    auto it = gLocs.find(gCurProg);
    if (it == gLocs.end() || it->second.nudge < 0 || !pUniform2f) return;
    float nx = 0, ny = 0;
    if ((unsigned)(gDrawSerial - gNoteSerial) <= 16 && !gDrawnPrev.empty()) {
        GLdouble mv[16];
        GetMV(mv);
        double ox = MVWorldX(mv),
               oy = MVWorldY(mv);
        int cx = (int)floor(ox), cy = (int)floor(oy);
        if (gObjShopFront) {      // remember the tiles it stands in (its width runs along model x)
            double ax = gViewInv[0] * mv[0] + gViewInv[4] * mv[1] + gViewInv[8] * mv[2],
                   ay = gViewInv[1] * mv[0] + gViewInv[5] * mv[1] + gViewInv[9] * mv[2];
            double L = sqrt(ax * ax + ay * ay);
            if (L > 1e-6) {
                ax /= L;
                ay /= L;
            }
            // only the counter's own length (it was a fixed +-1.4 tiles: a 2-tile counter took up to 4 wall tiles, the wall either side went missing)
            double half = 1.0;
            const ArrState& AS = GetArr();
            int stride = AS.st ? AS.st : AS.sz * 4;
            if (AS.vp && !AS.vbo && AS.ty == 0x1406 && AS.sz >= 3 && Readable((uint8_t*)AS.vp + (size_t)first * stride, (size_t)count * stride)) {
                double mx = 0;
                for (int i = 0; i < count; i++) mx = std::max(mx, (double)fabsf(((const float*)((uint8_t*)AS.vp + (size_t)(first + i) * stride))[0]));
                if (mx > 0) half = mx * L;      // model units -> tiles
            }
            for (double s = -half + 0.3; s <= half - 0.3 + 1e-6; s += 0.25) { int nk = (int)floor(oy + ay * s) * 65536 + (int)floor(ox + ax * s); gNoWallCur.insert(nk); if (gNoWallEver.size() < 20000) gNoWallEver.insert(nk); }
            { static int sl = 0; if (sl < 6) { sl++; Log("SHOPFRONT at (%.2f, %.2f): %.2f tiles long, wall cleared along it", ox, oy, half * 2); } }
        }
        if (gDrawnPrev.count(cy * 65536 + cx) || gNoWallPrev.count(cy * 65536 + cx)) {
            double fx = gViewInv[0] * mv[8] + gViewInv[4] * mv[9] + gViewInv[8] * mv[10],
                   fy = gViewInv[1] * mv[8] + gViewInv[5] * mv[9] + gViewInv[9] * mv[10];      // model +z (front) in the world
            int dx = fabs(fx) > fabs(fy) ? (fx > 0 ? 1 : -1) : 0, dy = dx == 0 ? (fy > 0 ? 1 : -1) : 0;
            auto open = [&](int x, int y) { return !gDrawnPrev.count(y * 65536 + x) && !gNoWallPrev.count(y * 65536 + x); };
            if (open(cx + dx, cy + dy)) {
                nx = (float)dx;
                ny = (float)dy;
            } else if (open(cx - dx, cy - dy)) {
                nx = (float)-dx;
                ny = (float)-dy;
            }
        }
    }
    if (lastProg != gCurProg || lastNudge[0] != nx || lastNudge[1] != ny) {
        pUniform2f(it->second.nudge, nx, ny);
        lastNudge[0] = nx;
        lastNudge[1] = ny;
        lastProg = gCurProg;
    }
}
// Off-screen culling. The game decides what to draw from its flat 2D view, which for the tilted 3D camera has to be generous, so on a big
// prison thousands of objects and wall pieces are drawn off screen. A draw under the 3D camera whose origin projects well outside the
// screen (with a margin of 1.5 tiles at its depth) is skipped. Big batches (floors, light strips) are never culled.
static unsigned gCulled = 0, gCullSeen = 0;
static bool CullOffscreen(GLsizei count) {
    if (!gCfg.cullOffscreen || !gCam.active || gMenuWorld || !gCurProg || count > 4000 || count < 6) return false;
    GLdouble P[16];
    GetPR(P);
    for (int i = 0; i < 16; i++)
        if (P[i] != gCamP[i]) return false;      // only the 3D camera's own draws
    GLdouble mv[16];
    GetMV(mv);
    gCullSeen++;
    {
        bool cam = true;
        for (int i = 0; i < 16; i++)
            if (fabs(mv[i] - gCamMV[i]) > 1e-6) {
                cam = false;
                break;
            }
        if (cam) return false;
    }      // vertices already in world space (stacks, food, item piles): the origin says nothing about where they are
    {
        double wx = MVWorldX(mv),
               wy = MVWorldY(mv);
        if (wx < 0.5 && wy < 0.5) return false;
    }      // drawn from the map origin: same
    double ex = mv[12], ey = mv[13], ez = mv[14];
    double cw = P[3] * ex + P[7] * ey + P[11] * ez + P[15];
    if (cw <= 1e-6) {
        gCulled++;
        return true;
    }      // behind the camera
    double nx = (P[0] * ex + P[4] * ey + P[8] * ez + P[12]) / cw, ny = (P[1] * ex + P[5] * ey + P[9] * ez + P[13]) / cw;
    double rx = gCfg.cullMargin * fabs(P[0]) / cw, ry = gCfg.cullMargin * fabs(P[5]) / cw;
    if (fabs(nx) > 1.0 + rx || fabs(ny) > 1.0 + ry) {
        gCulled++;
        return true;
    }
    // 1.9.82 (500-prisoner save: 25,000 draws a frame, 15-20 fps): pipes and cables under the floor are never seen; small props are
    // skipped once a tile is only a few pixels across (zoomed far out). Only the noted object's own first draws.
    if ((unsigned)(gDrawSerial - gNoteSerial) <= 4) {
        if (gObjUtil && gCfg.hideUtilities) { gCulled++; return true; }
        if (gObjNoteTiny && gCfg.tinyCullPx > 0 && count < 600) {
            double tilePx = fabs(P[0]) / cw * gVpW * 0.5;
            if (tilePx < gCfg.tinyCullPx) { gCulled++; return true; }
        }
    }
    return false;
}
// The game draws loose items (food crates, pots, trays...) only in its flat 2D sprite batches, which are hidden in 3D because every object
// with a model is in them too. Draw just the small sprites from those batches (items are smaller than a tile; furniture is not), lying on the floor.
// Loose items (stacks of ingredients, trays, goods, mail, books, wood...) exist only as small 2D sprites in a batch hidden in 3D.
// Recognise each sprite by where its texture coordinates sit in the sprite bank (items.h, from model-generator\pipeline\items.py)
// and draw a simple 3D item in its place. Not where a person stands (a cook carrying a crate already shows it).
#include "items.h"
struct ItemQ { float x, y, sx, sy; int kind; };
static std::vector<ItemQ> gItemQueue;      // this frame's loose items, drawn after the own floor (DrawQueuedItems)
static int gItemQueueFrame = -1;
static void DrawQueuedItems();
static void FoodFrameTick();
static std::unordered_set<int> gFoodCur, gFoodPrev, gTableCur, gTablePrev, gLidCur, gLidPrev;      // tiles (y * 65536 + x) this / last frame
// (gPersonSeen is declared above BubbleDraw, which uses it too)
extern int gWallFramesSeenExt;
static std::unordered_map<int, float> gSurfCur, gSurfPrev;      // tile (y * 65536 + x) -> surface height above the floor (tables, seats, fridges; 07_draw SurfaceRecord)
static float SurfHeightAt(float x, float y) {
    int k = (int)floor(y) * 65536 + (int)floor(x);
    float top = 0.0f;
    auto a = gSurfCur.find(k);
    if (a != gSurfCur.end()) top = a->second;
    auto b = gSurfPrev.find(k);
    if (b != gSurfPrev.end()) top = std::max(top, b->second);
    return top;
}
// 1.9.77 (user: "some objects appear offset by 1/2 a tile"): furniture against a wall is moved onto the wall line, but what lies on it stayed
// on its own tile and hung half off. Each surface also records its real outline; things on it are pushed inside it.
struct SurfBox { float x0, x1, y0, y1; };
static std::unordered_map<int, SurfBox> gSurfBoxCur, gSurfBoxPrev;
static bool SurfClamp(float& x, float& y, float hx, float hy) {
    int k = (int)floor(y) * 65536 + (int)floor(x);
    auto a = gSurfBoxCur.find(k);
    if (a == gSurfBoxCur.end()) { a = gSurfBoxPrev.find(k); if (a == gSurfBoxPrev.end()) return false; }
    const SurfBox& b = a->second;
    float nx = (b.x1 - b.x0 <= 2 * hx) ? (b.x0 + b.x1) * 0.5f : std::min(std::max(x, b.x0 + hx), b.x1 - hx);
    float ny = (b.y1 - b.y0 <= 2 * hy) ? (b.y0 + b.y1) * 0.5f : std::min(std::max(y, b.y0 + hy), b.y1 - hy);
    bool moved = nx != x || ny != y;
    x = nx; y = ny;
    return moved;
}
// 1.9.74 (user 9 Nov: "something for every sprite object, even a simple small extrusion, so nothing is invisible"): every small 2D sprite the
// 3D view hides and has no 3D item for is queued and drawn after the floor as a thin extrusion of its own art (stacked alpha-tested layers).
// Food in the objects_d11_2 sheet (texture 8 / hideTexB; from data\objects_d11_2.spritebank, cells of 32 px). Cooked first, then raw:
// the variants that share art (cells 117 / 123 / 125, 61,42) end up as ingredient crates; on a serving table anything counts as its food.
static const ItemSprite kItemSpritesB[] = {
    {73, 40, 2, 2, 2}, {73, 42, 2, 2, 2}, {73, 44, 2, 2, 2}, {73, 46, 2, 2, 2}, {68, 32, 2, 2, 2}, {76, 32, 2, 2, 2},      // StackMeals / MealsCooked
    {65, 44, 2, 2, 2}, {65, 46, 2, 2, 2}, {125, 10, 2, 2, 2}, {125, 12, 2, 2, 2}, {62, 32, 2, 2, 2}, {70, 32, 2, 2, 2},      // MealsCooked03/04, Food
    {115, 15, 2, 6, 2},                                                                                                      // RestaurantMealHQ
    {67, 44, 2, 2, 3}, {67, 46, 2, 2, 3}, {115, 13, 2, 2, 3},                                                                // FoodTray, dirty
    {73, 34, 2, 2, 1}, {73, 36, 2, 2, 1}, {67, 42, 2, 2, 1}, {73, 38, 2, 2, 1}, {123, 0, 2, 12, 1},                           // StackIngredients
    {125, 4, 2, 6, 1}, {61, 42, 2, 2, 1}, {117, 0, 2, 12, 1},                                                                // Ingredient
};
struct SpriteTri { GLuint tex; float p[3][2]; float uv[3][2]; };
static std::vector<SpriteTri> gSprCur, gSprPrev;
static int gSprFrame = -1;
static void DrawQueuedSprites();
static void QueueSpriteTri(const void* vp, int stride, const void* tp, int tstride, GLint i0) {
    if (gSprFrame != gWallFramesSeenExt) { gSprPrev.swap(gSprCur); gSprCur.clear(); gSprFrame = gWallFramesSeenExt; }
    if (gSprCur.size() >= 20000) return;
    SpriteTri t;
    t.tex = gBound[0];
    for (int k = 0; k < 3; k++) {
        const float* P = (const float*)((const uint8_t*)vp + (size_t)(i0 + k) * stride);
        const float* T = (const float*)((const uint8_t*)tp + (size_t)(i0 + k) * tstride);
        t.p[k][0] = P[0]; t.p[k][1] = P[1]; t.uv[k][0] = T[0]; t.uv[k][1] = T[1];
    }
    gSprCur.push_back(t);
}
static void DrawItems3D(GLint first, GLsizei count) {
    if (count < 6) return;      // 1.9.90: with items3D off every sprite takes the extruded-art path (the coloured boxes looked bad)
    const ArrState& AS = GetArr();
    int stride = AS.st ? AS.st : AS.sz * 4;
    if (AS.sz < 2 || AS.ty != 0x1406 || AS.vbo || !AS.vp || !Readable((uint8_t*)AS.vp + (size_t)first * stride, (size_t)count * stride)) return;
    void* tp = nullptr; GLint tsz = 0, tty = 0, tst = 0;
    glGetPointerv(0x8092 /* GL_TEXTURE_COORD_ARRAY_POINTER */, &tp);
    glGetIntegerv(0x8088 /* SIZE */, &tsz); glGetIntegerv(0x8089 /* TYPE */, &tty); glGetIntegerv(0x808A /* STRIDE */, &tst);
    if (!tp || tsz < 2 || tty != 0x1406) return;
    int tstride = tst ? tst : tsz * 4;
    if (!Readable((uint8_t*)tp + (size_t)first * tstride, (size_t)count * tstride)) return;
    GLint texW = 4096; glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, 0x1000 /* GL_TEXTURE_WIDTH */, &texW);
    const int cells = std::max(1, texW / 32);
    static std::unordered_map<int, int> cellMap;      // cell (y * 4096 + x) -> kItemSprites index
    if (cellMap.empty())
        for (int i = 0; i < (int)(sizeof kItemSprites / sizeof kItemSprites[0]); i++)
            for (int yy = 0; yy < kItemSprites[i].h; yy++)
                for (int xx = 0; xx < kItemSprites[i].w; xx++) cellMap[(kItemSprites[i].y + yy) * 4096 + kItemSprites[i].x + xx] = i;
    static std::vector<ItemQ> items; items.clear();
    static int logged = 0, missLogged = 0;
    for (int i = 0; i + 5 < count; i += 6) {
        float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f, mnu = 1e9f, mxu = -1e9f, mnv = 1e9f, mxv = -1e9f;
        for (int k = 0; k < 6; k++) {
            const float* P = (const float*)((uint8_t*)AS.vp + (size_t)(first + i + k) * stride);
            const float* T = (const float*)((uint8_t*)tp + (size_t)(first + i + k) * tstride);
            mnx = std::min(mnx, P[0]); mxx = std::max(mxx, P[0]); mny = std::min(mny, P[1]); mxy = std::max(mxy, P[1]);
            mnu = std::min(mnu, T[0]); mxu = std::max(mxu, T[0]); mnv = std::min(mnv, T[1]); mxv = std::max(mxv, T[1]);
        }
        if (mxx - mnx > gCfg.itemMaxSize || mxy - mny > gCfg.itemMaxSize) continue;
        int cx = (int)floor((mnu + mxu) * 0.5f * cells), cyA = (int)floor((mnv + mxv) * 0.5f * cells), cyB = cells - 1 - cyA;      // v may run either way
        auto f1 = cellMap.find(cyA * 4096 + cx), f2 = cellMap.find(cyB * 4096 + cx);
        int si = !gCfg.items3D ? -1 : (f1 != cellMap.end() ? f1->second : (f2 != cellMap.end() ? f2->second : -1));
        if (si < 0 && cx >= 16 && cx < 24 && ((cyA >= 40 && cyA < 42) || (cyB >= 40 && cyB < 42))) {      // ServingTableLid (bank 16,40 8x2): 2D only, noted for the table
            FoodFrameTick(); gLidCur.insert((int)floor((mny + mxy) * 0.5f) * 65536 + (int)floor((mnx + mxx) * 0.5f));
            continue;
        }
        if (si < 0) {
            {
                int tk = (int)floor((mny + mxy) * 0.5f) * 65536 + (int)floor((mnx + mxx) * 0.5f);
                if (gTableCur.count(tk) || gTablePrev.count(tk)) { FoodFrameTick(); gFoodCur.insert(tk); continue; }      // on a serving table: its food
            }
            int hx = (int)floor(mnx + mxx), hy = (int)floor(mny + mxy);      // centre, in half tiles
            bool model = (gModelTilesPrev.count(hy * 65536 + hx + 0x40000000) || gModelTilesCur.count(hy * 65536 + hx + 0x40000000)) &&
                         (SurfHeightAt((mnx + mxx) * 0.5f, (mny + mxy) * 0.5f) <= 0.0f || gThingTilesCur.count(hy * 65536 + hx) || gThingTilesPrev.count(hy * 65536 + hx));
                         // a 3D model stands there: its flat twin. On a table / fridge only when that model is a thing on it (a stack of logs or
                         // plates has its own model: drawing its sprite too made the two fight - user 9 Nov: "flickering logs, licence plates")
            auto pc = gPersonSeen.find((int)floor((mny + mxy) * 0.5f * 2) * 65536 + (int)floor((mnx + mxx) * 0.5f * 2));
            bool carried = pc != gPersonSeen.end() && gWallFramesSeenExt - pc->second <= 2;
            if (!model && !carried && gCfg.spriteFallback) { QueueSpriteTri(AS.vp, stride, tp, tstride, first + i); QueueSpriteTri(AS.vp, stride, tp, tstride, first + i + 3); }
            if (missLogged < 12) { missLogged++; Log("ITEMS3D sprite at bank cell (%d, %d or %d) is not an item: %s", cx, cyA, cyB, model ? "a model stands there" : (carried ? "carried" : "drawn as an extruded sprite")); }
            continue;
        }
        float x = (mnx + mxx) * 0.5f, y = (mny + mxy) * 0.5f;
        {      // 1.9.90: the game's own stack / crate model stands here (vanilla stacks are back): no second item on top of it (flicker)
            int hk = (int)floor(y * 2) * 65536 + (int)floor(x * 2);
            if (gThingTilesCur.count(hk) || gThingTilesPrev.count(hk)) continue;
        }
        auto ps = gPersonSeen.find((int)floor(y * 2) * 65536 + (int)floor(x * 2));
        if (ps != gPersonSeen.end() && gWallFramesSeenExt - ps->second <= 2) continue;      // carried by the person standing there
        items.push_back({x, y, (mxx - mnx) * 0.42f, (mxy - mny) * 0.42f, kItemSprites[si].kind});
        if (kItemSprites[si].kind == 2) { FoodFrameTick(); gFoodCur.insert((int)floor(y) * 65536 + (int)floor(x)); }
        if (logged < 12) { logged++; Log("ITEMS3D kind %d at (%.2f, %.2f) size %.2f x %.2f from bank cell (%d, %d)", kItemSprites[si].kind, x, y, mxx - mnx, mxy - mny, cx, f1 != cellMap.end() ? cyA : cyB); }
    }
    // queue them: the own 3D floor is drawn without depth testing and may come later in the frame, over them - they are drawn right after it
    if (gItemQueueFrame != gWallFramesSeenExt) { gItemQueue.clear(); gItemQueueFrame = gWallFramesSeenExt; }
    gItemQueue.insert(gItemQueue.end(), items.begin(), items.end());
    if (!gCfg.ownFloor) DrawQueuedItems();
}
// Serving tables: servingtable.obj carries its food last (pipeline\foodtail.py writes "# PA3aiD food tail N" = food triangles). The
// game puts cooked food on a table as a 2D item sprite (kind 2); a table with none of it on its tiles is drawn without the food.
static int gFoodFrame = -1;
static void FoodFrameTick() {
    if (gFoodFrame == gWallFramesSeenExt) return;
    gFoodFrame = gWallFramesSeenExt;
    gFoodPrev.swap(gFoodCur); gFoodCur.clear(); gTablePrev.swap(gTableCur); gTableCur.clear(); gLidPrev.swap(gLidCur); gLidCur.clear();
}
static GLsizei ServingTableCount(GLint first, GLsizei count) {
    static int tail = -1;      // food vertices at the end of the model, from the game's own file
    if (tail < 0) {
        tail = 0;
        FILE* fp = fopen((gDir + "data\\shapes\\servingtable.obj").c_str(), "r");
        char line[128];
        if (fp) {
            int n = 0;
            if (fgets(line, sizeof line, fp) && sscanf(line, "# PA3aiD food tail %d", &n) == 1 && n > 0) tail = n * 3;
            fclose(fp);
        }
        Log("SERVING food tail %d vertices", tail);
    }
    if (!tail || !gCfg.items3D || count <= tail) return count;
    FoodFrameTick();
    GLdouble mv[16];
    GetMV(mv);
    double wx = MVWorldX(mv),
           wy = MVWorldY(mv);
    double ax = gViewInv[0] * mv[0] + gViewInv[4] * mv[1] + gViewInv[8] * mv[2], ay = gViewInv[1] * mv[0] + gViewInv[5] * mv[1] + gViewInv[9] * mv[2];
    static double maxX = 0;      // the model's half length in model units (same model every time)
    if (maxX <= 0) {
        const ArrState& AS = GetArr();
        int st = AS.st ? AS.st : AS.sz * 4;
        if (!AS.vp || AS.vbo || AS.ty != 0x1406 || AS.sz < 3 || !Readable((uint8_t*)AS.vp + (size_t)first * st, (size_t)count * st)) return count;
        for (int i = 0; i < count; i++) maxX = std::max(maxX, (double)fabsf(((const float*)((uint8_t*)AS.vp + (size_t)(first + i) * st))[0]));
    }
    // before the 3D camera is set up (first frames after loading) the table sits at the origin: leave it as the game draws it
    if (fabs(wx) < 0.05 && fabs(wy) < 0.05) return count;
    bool food = false, lid = false;
    for (double s = -maxX * 0.9; s <= maxX * 0.9 + 1e-9; s += maxX * 0.15) {
        int tx = (int)floor(wx + ax * s), ty = (int)floor(wy + ay * s), k = ty * 65536 + tx;
        gTableCur.insert(k);
        if (gFoodCur.count(k) || gFoodPrev.count(k)) food = true;
        if (gLidCur.count(k) || gLidPrev.count(k)) lid = true;
    }
    // log each table (by tile) when its state changes, at most 40 lines a session: food item / lid sprite on it
    static std::unordered_map<int, int> lastState;
    static int sl = 0;
    int key = (int)floor(wy) * 65536 + (int)floor(wx), state = (food ? 1 : 0) | (lid ? 2 : 0);
    auto it = lastState.find(key);
    if ((it == lastState.end() || it->second != state) && sl < 40) {
        sl++;
        Log("SERVING table at (%.2f, %.2f): cooked-food item %s, lid sprite %s -> drawn %s", wx, wy, food ? "yes" : "no", lid ? "yes" : "no", food ? "with food" : "empty");
    }
    lastState[key] = state;
    // diagnostic: dump the table object (if rsi holds it: Pos +0x48/+0x4C = where it is drawn) for 4 tables, at the first sight and 3 minutes later,
    // to find the slots (save: Slot0..n = ids of the food stacks on it) by matching the save's ids
    {
        static std::map<int, DWORD> dumped;
        static int nd = 0;
        uint8_t* o = gServObj;
        DWORD now = GetTickCount();
        auto di = dumped.find(key);
        bool want = (di == dumped.end() && dumped.size() < 4) || (di != dumped.end() && di->second && now - di->second > 180000);
        if (want && nd < 8 && o && (uintptr_t)o >= 0x10000 && !((uintptr_t)o & 7) && Readable(o, 0x600)) {
            nd++;
            dumped[key] = di == dumped.end() ? now : 0;
            Log("SERVDUMP table at (%.2f, %.2f) rsi %p vtable %llx, its +0x48/+0x4C reads (%.2f, %.2f) (ints, offset in hex):", wx, wy, o,
                (unsigned long long)*(uint64_t*)o, *(float*)(o + 0x48), *(float*)(o + 0x4c));
            for (int r = 0; r < 0x600; r += 0x40) {
                const int* q = (const int*)(o + r);
                Log("SERVDUMP +%03x: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", r, q[0], q[1], q[2], q[3], q[4], q[5], q[6], q[7], q[8], q[9], q[10], q[11], q[12], q[13], q[14], q[15]);
            }
        }
    }
    return food ? count : count - tail;
}
// ---------------------------------------------------------------------------------------------------------------------------------
// 1.9.128 (user: "the food / ingredients now need doing properly"; 1.9.127's models never showed): the game draws a stack's DLC food
// (objects_d11_2: crates of ingredients, pots, pans, trays, bowls, single items) only as 2D sprites, never with a model - so the mod
// draws a 3D model of its own wherever one of those sprites is drawn. Models and their texture sheet come from
// model-generator\pipeline\food.py (PA3aiD\food3d\food.txt + food.rgba); each model knows its sprite's place in the sheet.
struct FoodModel { int bx, by, bw, bh; std::vector<float> v; };      // v: per vertex u v nx ny nz x y z (model units: a tile is 100, y up)
static std::vector<FoodModel> gFood;
static int gFoodLoad = 0;      // 0 not tried, 1 loaded, -1 failed
static std::vector<unsigned char> gFoodPix;
static int gFoodW = 0, gFoodH = 0;
static GLuint gFoodTex = 0;
struct FoodQ { float x, y, h; int m; };
static std::vector<FoodQ> gFoodQCur, gFoodQPrev;
static int gFoodQFrame = -1;
static bool FoodLoad() {
    if (gFoodLoad) return gFoodLoad > 0;
    gFoodLoad = -1;
    FILE* f = fopen((gDir + "PA3D Expanded\\food3d\\food.txt").c_str(), "r");
    if (!f) { Log("Food models: PA3D Expanded\\food3d\\food.txt not found - kitchen food stays flat"); return false; }
    static char line[2048];
    FoodModel* cur = nullptr; int left = 0; size_t tris = 0;
    while (fgets(line, sizeof line, f)) {
        if (left > 0 && cur) {
            const char* p = line; char* e = nullptr;
            float t[24]; int n = 0;
            for (; n < 24; n++) { t[n] = strtof(p, &e); if (e == p) break; p = e; }
            if (n == 24) { cur->v.insert(cur->v.end(), t, t + 24); tris++; }
            left--;
            continue;
        }
        char nm[128]; int bx, by, bw, bh, nt;
        if (sscanf(line, "atlas %d %d", &gFoodW, &gFoodH) == 2) continue;
        if (sscanf(line, "model %127s %d %d %d %d %d", nm, &bx, &by, &bw, &bh, &nt) == 6) {
            gFood.push_back(FoodModel{bx, by, bw, bh, {}});
            cur = &gFood.back(); cur->v.reserve((size_t)nt * 24); left = nt;
        }
    }
    fclose(f);
    FILE* r = fopen((gDir + "PA3D Expanded\\food3d\\food.rgba").c_str(), "rb");
    if (r && gFoodW > 0 && gFoodH > 0 && gFoodW <= 4096 && gFoodH <= 4096) {
        gFoodPix.resize((size_t)gFoodW * gFoodH * 4);
        if (fread(gFoodPix.data(), 1, gFoodPix.size(), r) != gFoodPix.size()) gFoodPix.clear();
    }
    if (r) fclose(r);
    if (gFood.empty() || gFoodPix.empty()) { Log("Food models: could not read PA3D Expanded\\food3d (models %d, texture %s) - kitchen food stays flat", (int)gFood.size(), gFoodPix.empty() ? "missing" : "ok"); gFood.clear(); return false; }
    gFoodLoad = 1;
    Log("Food models: %d models, %u triangles, texture %dx%d (kitchen food drawn in 3D)", (int)gFood.size(), (unsigned)tris, gFoodW, gFoodH);
    return true;
}
// the food model for a sprite of the objects_d11_2 sheet at cell (cx, cy) - cy either way up; -1 = not food
static int FoodFind(int cx, int cyA, int cyB) {
    if (!gCfg.foodModels || !FoodLoad()) return -1;
    for (size_t i = 0; i < gFood.size(); i++) {
        const FoodModel& m = gFood[i];
        if (cx < m.bx || cx > m.bx + m.bw - 1) continue;
        if ((cyB >= m.by && cyB <= m.by + m.bh - 1) || (cyA >= m.by && cyA <= m.by + m.bh - 1)) return (int)i;
    }
    return -1;
}
static void FoodRecord(float x, float y, int m) {
    if (gFoodQFrame != gSwapCount) { gFoodQPrev.swap(gFoodQCur); gFoodQCur.clear(); gFoodQFrame = gSwapCount; }
    if (!gFoodQCur.empty() && gFoodQCur.back().m == m && fabsf(gFoodQCur.back().x - x) < 0.05f && fabsf(gFoodQCur.back().y - y) < 0.05f) return;      // the quad's second triangle
    float h = SurfHeightAt(x, y);      // on a freezer, a table, a cooker
    auto ps = gPersonSeen.find((int)floor(y * 2) * 65536 + (int)floor(x * 2));
    if (ps != gPersonSeen.end() && gWallFramesSeenExt - ps->second <= 2) h = std::max(h, 0.40f);      // carried: in the hands
    if (gFoodQCur.size() < 4000) gFoodQCur.push_back({x, y, h, m});
}
static void DrawQueuedFood() {
    const std::vector<FoodQ>& Q = (gFoodQFrame == gSwapCount ? gFoodQPrev : gFoodQCur);
    if (Q.empty() || gFoodLoad <= 0) return;
    PFN_UseProgram use = oUseProgram ? oUseProgram : oUseProgramObjectARB;
    if (!use || !pActiveTexture) return;
    typedef void(APIENTRY * PFN_BindBuf)(GLenum, GLuint);
    typedef void(APIENTRY * PFN_ClientAct)(GLenum);
    static PFN_BindBuf bindBuf = oWglGetProcAddress ? (PFN_BindBuf)oWglGetProcAddress("glBindBuffer") : nullptr;
    static PFN_ClientAct clientAct = oWglGetProcAddress ? (PFN_ClientAct)oWglGetProcAddress("glClientActiveTexture") : nullptr;
    GLint prevProg = 0, act = 0, prevBuf = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &act);
    glGetIntegerv(0x8894 /* ARRAY_BUFFER_BINDING */, &prevBuf);
    use(0);
    pActiveTexture(GL_TEXTURE0);
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_LIGHTING_BIT | GL_TEXTURE_BIT | GL_COLOR_BUFFER_BIT | GL_POLYGON_BIT);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    if (!gFoodTex) {
        glGenTextures(1, &gFoodTex);
        glBindTexture(GL_TEXTURE_2D, gFoodTex);
        glTexParameteri(GL_TEXTURE_2D, 0x8191 /* GENERATE_MIPMAP */, GL_TRUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gFoodW, gFoodH, 0, GL_RGBA, GL_UNSIGNED_BYTE, gFoodPix.data());
    }
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gFoodTex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST); glDisable(GL_CULL_FACE); glDisable(GL_FOG);
    glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    const float up = (float)gCfg.upSign, dim = 1.0f - 0.55f * gCellsDark;
    // lit from above and a little to one side, in the world (the modelview is the camera here)
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL); glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    const float lp[4] = {0.35f, 0.55f, up * 1.0f, 0.0f}, dif[4] = {0.50f, 0.50f, 0.48f, 1.0f}, amb[4] = {0.58f, 0.58f, 0.60f, 1.0f}, zero[4] = {0, 0, 0, 1};
    glLightfv(GL_LIGHT0, GL_POSITION, lp); glLightfv(GL_LIGHT0, GL_DIFFUSE, dif); glLightfv(GL_LIGHT0, GL_AMBIENT, amb); glLightfv(GL_LIGHT0, GL_SPECULAR, zero);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zero);
    glColor4f(dim, dim, dim, 1.0f);
    if (bindBuf) bindBuf(0x8892 /* ARRAY_BUFFER */, 0);
    if (clientAct) clientAct(GL_TEXTURE0);
    glDisableClientState(GL_COLOR_ARRAY); glDisableClientState(0x8457 /* FOG_COORD_ARRAY */); glDisableClientState(0x845E /* SECONDARY_COLOR_ARRAY */);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_NORMAL_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    // model x -> world x, model z (its front) -> world +y (south, down the screen in the 2D game), model y -> up; 100 model units a tile
    const GLfloat M[16] = {0.01f, 0, 0, 0, 0, 0, 0.01f * up, 0, 0, 0.01f, 0, 0, 0, 0, 0, 1};
    for (const FoodQ& q : Q) {
        if (q.m < 0 || q.m >= (int)gFood.size()) continue;
        const std::vector<float>& v = gFood[q.m].v;
        if (v.empty()) continue;
        glPushMatrix();
        glTranslatef(q.x, q.y, up * (q.h + 0.002f));
        glMultMatrixf(M);
        glTexCoordPointer(2, GL_FLOAT, 32, v.data());
        glNormalPointer(GL_FLOAT, 32, v.data() + 2);
        glVertexPointer(3, GL_FLOAT, 32, v.data() + 5);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 8));
        glPopMatrix();
    }
    glPopClientAttrib();
    glPopAttrib();
    if (bindBuf) bindBuf(0x8892, (GLuint)prevBuf);
    pActiveTexture((GLenum)act);
    use((GLuint)prevProg);
}
static void DrawQueuedEquip();
static void DrawQueuedItems() {
    DrawQueuedEquip();      // people's hands, cuffs, weapons and shadows (07_draw): after the floor too
    DrawQueuedSprites();      // every other small sprite, as a thin extrusion of its art (07_draw)
    DrawQueuedFood();      // 1.9.128: the kitchen food's own 3D models
    const std::vector<ItemQ>& items = gItemQueue;
    if (items.empty() || !gCfg.items3D) return;
    GLint prevProg = 0, act = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &act);
    PFN_UseProgram use = oUseProgram ? oUseProgram : oUseProgramObjectARB;
    if (!use) return;
    use(0);
    pActiveTexture(GL_TEXTURE0);
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_BLEND); glDisable(GL_CULL_FACE); glDisable(GL_ALPHA_TEST);
    glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
    float up = (float)gCfg.upSign, dim = 1.0f - 0.55f * gCellsDark;
    // world space: x, y on the floor, height along z (upSign); a box helper in that frame
    float base = 0.0f;      // 1.9.76: on a table / fridge / seat top, not inside it
    auto box = [&](float x0, float x1, float y0, float y1, float h0, float h1, float r, float g, float b) {
        h0 += base; h1 += base;
        const float sh[6] = {1.0f, 0.5f, 0.82f, 0.82f, 0.68f, 0.68f};
        const float V[6][4][3] = {{{x0, y0, h1}, {x1, y0, h1}, {x1, y1, h1}, {x0, y1, h1}}, {{x0, y0, h0}, {x0, y1, h0}, {x1, y1, h0}, {x1, y0, h0}},
                                  {{x0, y0, h0}, {x1, y0, h0}, {x1, y0, h1}, {x0, y0, h1}}, {{x0, y1, h0}, {x0, y1, h1}, {x1, y1, h1}, {x1, y1, h0}},
                                  {{x1, y0, h0}, {x1, y1, h0}, {x1, y1, h1}, {x1, y0, h1}}, {{x0, y0, h0}, {x0, y0, h1}, {x0, y1, h1}, {x0, y1, h0}}};
        for (int k = 0; k < 6; k++) { glColor3f(r * sh[k] * dim, g * sh[k] * dim, b * sh[k] * dim); for (int v = 0; v < 4; v++) glVertex3f(V[k][v][0], V[k][v][1], up * V[k][v][2]); }
    };
    glBegin(GL_QUADS);
    for (const ItemQ& it : items) {
        if (it.kind == 2) {      // cooked food on a serving table: the table's own pans show it
            int k = (int)floor(it.y) * 65536 + (int)floor(it.x);
            if (gTableCur.count(k) || gTablePrev.count(k)) continue;
        }
        float sx = std::max(0.12f, std::min(it.sx, 0.45f)), sy = std::max(0.12f, std::min(it.sy, 0.45f));
        float ix = it.x, iy = it.y;
        SurfClamp(ix, iy, sx, sy);      // on furniture: inside its real outline (1.9.77)
        float x0 = ix - sx, x1 = ix + sx, y0 = iy - sy, y1 = iy + sy;
        unsigned hsh = (unsigned)(it.x * 7.0f) * 31u + (unsigned)(it.y * 13.0f);
        base = SurfHeightAt(it.x, it.y);
        switch (it.kind) {
            case 1: {      // a cardboard crate of ingredients, open at the top
                box(x0, x1, y0, y1, 0.0f, 0.30f, 0.70f, 0.53f, 0.33f);
                const float pile[3][3] = {{0.40f, 0.66f, 0.24f}, {0.85f, 0.52f, 0.18f}, {0.80f, 0.30f, 0.26f}};
                const float* pc = pile[hsh % 3];
                box(x0 + 0.03f, x1 - 0.03f, y0 + 0.03f, y1 - 0.03f, 0.30f, 0.34f, pc[0], pc[1], pc[2]);
                break; }
            case 2: box(x0 + 0.04f, x1 - 0.04f, y0 + 0.04f, y1 - 0.04f, 0.0f, 0.24f, 0.62f, 0.64f, 0.68f); box(x0 + 0.07f, x1 - 0.07f, y0 + 0.07f, y1 - 0.07f, 0.24f, 0.27f, 0.62f, 0.40f, 0.20f); break;
            case 3: box(x0, x1, y0, y1, 0.0f, 0.14f, 0.78f, 0.80f, 0.82f); break;
            case 4: box(x0, x1, y0, y1, 0.0f, 0.28f, 0.80f, 0.66f, 0.40f); box(x0 + 0.04f, x0 + 0.12f, y0 - 0.005f, y1 + 0.005f, 0.0f, 0.285f, 0.85f, 0.25f, 0.22f); break;
            case 5: box(x0, x1, y0, y1, 0.0f, 0.12f, 0.92f, 0.48f, 0.16f); break;
            case 6: box(x0, x1, y0, y1, 0.0f, 0.10f, 0.94f, 0.93f, 0.88f); break;
            case 7: box(x0, x1, y0, y1, 0.0f, 0.08f, 0.70f, 0.20f, 0.20f); box(x0 + 0.02f, x1 - 0.02f, y0 + 0.02f, y1 - 0.02f, 0.08f, 0.16f, 0.22f, 0.40f, 0.70f); break;
            case 8: box(x0, x1, y0, y1, 0.0f, 0.22f, 0.50f, 0.34f, 0.20f); break;
            case 9: box(x0, x1, y0, y1, 0.0f, 0.08f, 0.66f, 0.68f, 0.72f); break;
            case 10: box(x0, x1, y0, y1, 0.0f, 0.24f, 0.66f, 0.36f, 0.28f); break;
        }
    }
    glEnd();
    glPopAttrib();
    pActiveTexture((GLenum)act);
    use((GLuint)prevProg);
}static void DrawSmallSprites(GLenum mode, GLint first, GLsizei count) {
    if (!(gCfg.showItems || gCfg.spriteFallback) || mode != GL_TRIANGLES || count < 3) return;
    const ArrState& AS = GetArr();
    void* vp = AS.vp;
    int stride = AS.st ? AS.st : AS.sz * 4;
    if (AS.sz < 2 || AS.ty != 0x1406 || AS.vbo || !vp || !Readable((uint8_t*)vp + (size_t)first * stride, (size_t)count * stride)) return;
    static std::vector<GLuint> idx;
    idx.clear();
    for (int i = 0; i + 2 < count; i += 3) {
        float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
        for (int k = 0; k < 3; k++) {
            const float* P = (const float*)((uint8_t*)vp + (size_t)(first + i + k) * stride);
            mnx = std::min(mnx, P[0]);
            mxx = std::max(mxx, P[0]);
            mny = std::min(mny, P[1]);
            mxy = std::max(mxy, P[1]);
        }
        if (mxx - mnx < gCfg.itemMaxSize && mxy - mny < gCfg.itemMaxSize) {
            int hx = (int)floor((mnx + mxx)), hy = (int)floor((mny + mxy));         // centre, in half tiles
            if (gModelTilesPrev.count(hy * 65536 + hx + 0x40000000) &&
                (SurfHeightAt((mnx + mxx) * 0.5f, (mny + mxy) * 0.5f) <= 0.0f || gThingTilesPrev.count(hy * 65536 + hx) || gThingTilesCur.count(hy * 65536 + hx))) continue;      // a 3D model stands there: this sprite is its flat twin (things lying on tables / fridges stay)
            idx.push_back(first + i);
            idx.push_back(first + i + 1);
            idx.push_back(first + i + 2);
        }
    }
    if (idx.empty()) return;
    {      // 1.9.74 diagnostic: where in this sheet each kind of small sprite sits (the newer food / ingredient art is here, not in the main
           // sheet), with where it lies in the world, so the food sprites can be mapped. 80 distinct cells a session.
        void* tp = nullptr; GLint tsz = 0, tty = 0, tst = 0;
        glGetPointerv(0x8092, &tp);
        glGetIntegerv(0x8088, &tsz); glGetIntegerv(0x8089, &tty); glGetIntegerv(0x808A, &tst);
        int tstride = tst ? tst : tsz * 4;
        static std::set<int> seenCell;
        if (tp && tsz >= 2 && tty == 0x1406 && seenCell.size() < 80 && Readable((uint8_t*)tp + (size_t)first * tstride, (size_t)count * tstride)) {
            GLint texW = 0, texH = 0;
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, 0x1000, &texW);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, 0x1001, &texH);
            for (size_t j = 0; j + 2 < idx.size() && seenCell.size() < 80; j += 3) {
                float mu = 1e9f, xu = -1e9f, mv_ = 1e9f, xv = -1e9f, px = 0, py = 0;
                for (int k = 0; k < 3; k++) {
                    const float* T = (const float*)((uint8_t*)tp + (size_t)idx[j + k] * tstride);
                    const float* P = (const float*)((uint8_t*)vp + (size_t)idx[j + k] * stride);
                    mu = std::min(mu, T[0]); xu = std::max(xu, T[0]); mv_ = std::min(mv_, T[1]); xv = std::max(xv, T[1]);
                    px += P[0] / 3; py += P[1] / 3;
                }
                int cx = (int)floor((mu + xu) * 0.5f * texW / 32.0f), cy = (int)floor((mv_ + xv) * 0.5f * texH / 32.0f);
                if (seenCell.insert(cy * 4096 + cx).second)
                    Log("SMALLSPRITE tex %u (%dx%d) cell (%d, %d) [uv %.4f..%.4f, %.4f..%.4f] at world (%.1f, %.1f)", gBound[0], (int)texW, (int)texH, cx, cy, mu, xu, mv_, xv, px, py);
            }
        }
        if (gCfg.spriteFallback && tp && tsz >= 2 && tty == 0x1406 && Readable((uint8_t*)tp + (size_t)first * tstride, (size_t)count * tstride)) {
            // 1.9.75: this sheet is objects_d11_2 (the 1.9.74 log: StackIngredients01 at cell (74, 93) = bank (73, 34) with v flipped), where the
            // game's ingredients and cooked meals are drawn. Those become 3D items; anything on a serving table is its food (the table shows it).
            static std::unordered_map<int, int> cellMapB;
            if (cellMapB.empty())
                for (int i = 0; i < (int)(sizeof kItemSpritesB / sizeof kItemSpritesB[0]); i++)
                    for (int yy = 0; yy < kItemSpritesB[i].h; yy++)
                        for (int xx = 0; xx < kItemSpritesB[i].w; xx++) cellMapB[(kItemSpritesB[i].y + yy) * 4096 + kItemSpritesB[i].x + xx] = i;
            GLint texW = 4096; glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, 0x1000, &texW);
            const int cells = std::max(1, texW / 32);
            static std::vector<ItemQ> itemsB; itemsB.clear();
            static int bl = 0;
            for (size_t j = 0; j + 2 < idx.size(); j += 3) {
                float mu = 1e9f, xu = -1e9f, mv_ = 1e9f, xv = -1e9f, mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
                for (int k = 0; k < 3; k++) {
                    const float* T = (const float*)((uint8_t*)tp + (size_t)idx[j + k] * tstride);
                    const float* P = (const float*)((uint8_t*)vp + (size_t)idx[j + k] * stride);
                    mu = std::min(mu, T[0]); xu = std::max(xu, T[0]); mv_ = std::min(mv_, T[1]); xv = std::max(xv, T[1]);
                    mnx = std::min(mnx, P[0]); mxx = std::max(mxx, P[0]); mny = std::min(mny, P[1]); mxy = std::max(mxy, P[1]);
                }
                float x = (mnx + mxx) * 0.5f, y = (mny + mxy) * 0.5f;
                { int hk = (int)floor(y * 2) * 65536 + (int)floor(x * 2); if (gThingTilesCur.count(hk) || gThingTilesPrev.count(hk)) continue; }      // 1.9.90: a model stands here
                int tk = (int)floor(y) * 65536 + (int)floor(x);
                if (gTableCur.count(tk) || gTablePrev.count(tk)) {      // on a serving table: its food
                    FoodFrameTick(); gFoodCur.insert(tk);
                    continue;
                }
                int cx = (int)floor((mu + xu) * 0.5f * cells), cyA = (int)floor((mv_ + xv) * 0.5f * cells), cyB = cells - 1 - cyA;
                {      // 1.9.128: kitchen food - its own 3D model instead of the flat art
                    int fm = FoodFind(cx, cyA, cyB);
                    if (fm >= 0) {
                        FoodRecord(x, y, fm);
                        static int fl = 0;
                        if (fl < 6) { fl++; Log("Food model %d at (%.2f, %.2f) for sprite cell (%d, %d)", fm, x, y, cx, cyB); }
                        continue;
                    }
                }
                auto f1 = cellMapB.find(cyB * 4096 + cx), f2 = cellMapB.find(cyA * 4096 + cx);
                int si = !gCfg.items3D ? -1 : (f1 != cellMapB.end() ? f1->second : (f2 != cellMapB.end() ? f2->second : -1));
                if (si >= 0) {
                    bool firstOfQuad = itemsB.empty() || fabs(itemsB.back().x - x) > 0.05f || fabs(itemsB.back().y - y) > 0.05f;
                    if (firstOfQuad) {
                        itemsB.push_back({x, y, (mxx - mnx) * 0.42f, (mxy - mny) * 0.42f, kItemSpritesB[si].kind});
                        if (bl < 12) { bl++; Log("ITEMS3D-B kind %d at (%.2f, %.2f) from sheet cell (%d, %d)", kItemSpritesB[si].kind, x, y, cx, f1 != cellMapB.end() ? cyB : cyA); }
                    }
                    continue;
                }
                {      // 1.9.127 (user: the saloon door's 2D version lies on the floor): door leaves of this sheet (d_SaloonDoor / DoubleDoor, bank 88..91 x 73..74)
                       // have their own 3D model - their flat art is never extruded
                    auto doorArt = [](int x, int y) { return x >= 88 && x <= 91 && y >= 73 && y <= 74; };
                    if (doorArt(cx, cyB) || doorArt(cx, cyA)) {
                        static int sl = 0;
                        if (sl < 3) { sl++; Log("Saloon door art at (%.2f, %.2f) not drawn as a flat piece (its model shows the door)", x, y); }
                        continue;
                    }
                }
                QueueSpriteTri(vp, stride, tp, tstride, (GLint)idx[j]);
            }
            if (!itemsB.empty()) {
                if (gItemQueueFrame != gWallFramesSeenExt) { gItemQueue.clear(); gItemQueueFrame = gWallFramesSeenExt; }
                gItemQueue.insert(gItemQueue.end(), itemsB.begin(), itemsB.end());
            }
            static int ql = 0;
            if (ql < 4) { ql++; Log("ITEMS %d small sprites queued as extrusions from a hidden 2D batch of %d", (int)idx.size() / 6, (int)count / 6); }
            return;
        }
    }
    if (!gCfg.showItems) return;
    GLdouble mv[16];
    GetMV(mv);
    double d = 0.02 * gCfg.upSign, e[3];
    for (int i = 0; i < 3; i++) e[i] = gViewInv[i * 4 + 2] * d;
    GLdouble M2[16];
    memcpy(M2, mv, sizeof M2);
    M2[12] += e[0];
    M2[13] += e[1];
    M2[14] += e[2];
    glPushMatrix();
    glLoadMatrixd(M2);
    oDrawElements(GL_TRIANGLES, (GLsizei)idx.size(), GL_UNSIGNED_INT, idx.data());
    glPopMatrix();
    static int lines = 0;
    if (lines < 4) {
        lines++;
        Log("ITEMS %d small sprites drawn from a hidden 2D batch of %d", (int)idx.size() / 6, (int)count / 6);
    }
}
// Prisoner heads are an overlay, as in 2D: the body model's head gets the prisoner's own head sprite (PA3aiD\heads.rgba, made by heads.py:
// 8 x 8 cells of 32 px, HeadN in cell N) painted on the front. The stock bodies (picked by vertex count) and where their heads sit, in OBJ units:
