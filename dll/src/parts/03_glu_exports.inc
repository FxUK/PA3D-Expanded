// parts/03_glu_exports.inc - the GLU functions the game imports. Part of main.cpp (one translation unit, included in order): it uses everything defined in the parts before it.
// ---------------------------------------------------------------------------
// GLU exports
// ---------------------------------------------------------------------------
extern "C" {

void APIENTRY gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar) {
    LoadRealGlu();
    R.Perspective(fovy, aspect, zNear, zFar);      // iso projection is applied in gluLookAt
    gMvGen++;
    gProjGen++;
}

void APIENTRY gluLookAt(GLdouble ex, GLdouble ey, GLdouble ez, GLdouble cx, GLdouble cy, GLdouble cz, GLdouble ux, GLdouble uy, GLdouble uz) {
    LoadRealGlu();
    gMvGen++;      // the real GLU (and our camera) change the modelview without our macros
    struct MvBump { ~MvBump() { gMvGen++; } } mvBump;
    uintptr_t ret = (uintptr_t)__builtin_return_address(0);
    if (gBuildOK && ret == gExe + pa::kRva_Ret_Camera_gluLookAt) {
        gCam.gameEye[0] = ex;
        gCam.gameEye[1] = ey;
        gCam.gameEye[2] = ez;
        gCam.gameCenter[0] = cx;
        gCam.gameCenter[1] = cy;
        gCam.gameCenter[2] = cz;
        gCam.gameUp[0] = ux;
        gCam.gameUp[1] = uy;
        gCam.gameUp[2] = uz;
        if (gCfg.verboseLog && gCam.samplesLogged < 5) {
            gCam.samplesLogged++;
            float vx = 0, vy = 0, vs = 0;
            if (uint8_t* app = App()) {
                vx = *(float*)(app + pa::kApp_ViewRect + pa::kView_CenterX);
                vy = *(float*)(app + pa::kApp_ViewRect + pa::kView_CenterY);
                vs = *(float*)(app + pa::kApp_ViewRect + pa::kView_Scale);
            }
            Log("Camera sample: eye(%.3f %.3f %.3f) center(%.3f %.3f %.3f) up(%.3f %.3f %.3f) view(%.3f %.3f scale %.5f)", ex, ey, ez, cx, cy, cz, ux,
                uy, uz, vx, vy, vs);
        }
        if (gCfg.isoEnabled && WorldLoaded()) {
            // a second render pass (an offscreen texture) uses another viewport: leave that to the game's own camera
            GLint vp[4] = {0, 0, 0, 0};
            glGetIntegerv(GL_VIEWPORT, vp);
            RECT rc = {0, 0, 0, 0};
            bool fullView = true;
            if (gWnd && GetClientRect(gWnd, &rc)) fullView = IsMainViewport(vp[2], vp[3]);
            static int vl = 0;
            if (!fullView && vl < 12) {
                vl++;
                Log("Camera: a %dx%d view (window %ldx%ld) is not the main view - the game's own camera draws it", vp[2], vp[3], rc.right - rc.left,
                    rc.bottom - rc.top);
            }
            // The game also renders the world into an extra frame buffer every second or two (game time). That pass must keep the game's own camera:
            // the 3D camera there replaced the main view's matrices for the rest of the frame, and walls and objects vanished for a frame (the flicker).
            if (fullView) {
                static std::map<GLuint, int> fboUse;
                int& cnt = fboUse[gCurFbo];
                if (cnt < 1000000) cnt++;
                GLuint mainFbo = gCurFbo;
                int best = -1;
                for (auto& kv : fboUse)
                    if (kv.second > best) {
                        best = kv.second;
                        mainFbo = kv.first;
                    }
                if (gCurFbo != mainFbo && best > 60) {
                    static int fl = 0;
                    if (fl < 10) {
                        fl++;
                        Log("CAMERA pass in frame buffer %u (main view is %u): using the game's own camera", gCurFbo, mainFbo);
                    }
                } else {
                    ApplyIsoCamera(ex, ey, ez);
                    return;
                }
            }
        }
    }
    R.LookAt(ex, ey, ez, cx, cy, cz, ux, uy, uz);
}

int APIENTRY gluUnProject(GLdouble wx, GLdouble wy, GLdouble wz, const GLdouble* model, const GLdouble* proj, const GLint* view, GLdouble* ox,
                          GLdouble* oy, GLdouble* oz) {
    LoadRealGlu();
    // Mouse picking: the game unprojects the cursor with its own camera matrices, which no longer match the isometric / perspective camera we draw with.
    // Hand it ours (only for the full-window 3D view) so the tile under the cursor is the tile you see.
    bool ours = false;
    if (gCfg.pickFix && gCam.active && view && gWnd) {
        RECT rc = {0, 0, 0, 0};
        GetClientRect(gWnd, &rc);
        if (IsMainViewport(view[2], view[3])) ours = true;
    }
    static int ul = 0;
    if (ul < 12) {
        ul++;
        bool mvSame = true, pSame = true;
        if (model && proj)
            for (int i = 0; i < 16; i++) {
                if (fabs(model[i] - gCamMV[i]) > 1e-6) mvSame = false;
                if (fabs(proj[i] - gCamP[i]) > 1e-6) pSame = false;
            }
        Log("UNPROJECT win(%.0f,%.0f,%.2f) viewport %d,%d  game matrices == camera: model %d proj %d  -> using %s", wx, wy, wz, view ? view[2] : 0,
            view ? view[3] : 0, (int)mvSame, (int)pSame, ours ? "our camera" : "the game's");
    }
    if (ours) return R.UnProject(wx, wy, wz, gCamMV, gCamP, view, ox, oy, oz);
    return R.UnProject(wx, wy, wz, model, proj, view, ox, oy, oz);
}
void APIENTRY gluOrtho2D(GLdouble l, GLdouble r, GLdouble b, GLdouble t) {
    LoadRealGlu();
    R.Ortho2D(l, r, b, t);
    gMvGen++;
    gProjGen++;
}
// The game's tileset (floors / terrain) is a 4608 x 4608 RGBA texture. A loose data\tilesetpadded.png is not read by the
// game, so swap the pixels as they are uploaded: PAIso\tileset_modern.rgba (made by modernise_floor.py) replaces them.
static std::vector<unsigned char> gTilesetModern;
static bool TilesetModernLoad() {
    static int tried = 0;
    if (tried) return !gTilesetModern.empty();
    tried = 1;
    std::string p = gDir + "PA3D Expanded\\tileset_modern.rgba";
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) {
        Log("Modern tileset not found (%s): floors use the game's own art", p.c_str());
        return false;
    }
    size_t need = (size_t)4608 * 4608 * 4;
    gTilesetModern.resize(need);
    size_t got = fread(gTilesetModern.data(), 1, need, f);
    fclose(f);
    if (got != need) {
        Log("Modern tileset file is the wrong size (%zu bytes)", got);
        gTilesetModern.clear();
        return false;
    }
    Log("Modern tileset loaded (%zu bytes)", got);
    return true;
}
int APIENTRY gluBuild2DMipmaps(GLenum tg, GLint ifmt, GLsizei w, GLsizei h, GLenum fmt, GLenum ty, const void* d) {
    LoadRealGlu();
    if (w >= 2048 && h >= 2048) Log("gluBuild2DMipmaps %dx%d fmt=0x%x type=0x%x", (int)w, (int)h, (unsigned)fmt, (unsigned)ty);
    if (w == 4608 && h == 4608 && fmt == 0x1908 /*GL_RGBA*/ && ty == 0x1401 /*GL_UNSIGNED_BYTE*/ && TilesetModernLoad()) {
        Log("Tileset upload replaced with the modernised art");
        return R.Build2DMipmaps(tg, ifmt, w, h, fmt, ty, gTilesetModern.data());
    }
    return R.Build2DMipmaps(tg, ifmt, w, h, fmt, ty, d);
}
const GLubyte* APIENTRY gluErrorString(GLenum e) {
    LoadRealGlu();
    return R.ErrorString(e);
}

}      // extern "C"
