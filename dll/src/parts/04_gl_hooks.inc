// parts/04_gl_hooks.inc - shader interception and uniforms, texture hooks. Part of main.cpp (one translation unit, included in order): it uses everything defined in the parts before it.
// ---------------------------------------------------------------------------
// Shader interception
// ---------------------------------------------------------------------------
typedef void(APIENTRY* PFN_ShaderSource)(GLuint, GLsizei, const char* const*, const GLint*);
typedef void(APIENTRY* PFN_AttachShader)(GLuint, GLuint);
typedef void(APIENTRY* PFN_UseProgram)(GLuint);
typedef GLint(APIENTRY* PFN_GetUniformLocation)(GLuint, const char*);
typedef void(APIENTRY* PFN_Uniform1i)(GLint, GLint);
typedef void(APIENTRY* PFN_Uniform1f)(GLint, GLfloat);
typedef void(APIENTRY* PFN_Uniform2f)(GLint, GLfloat, GLfloat);
typedef void(APIENTRY* PFN_Uniform3f)(GLint, GLfloat, GLfloat, GLfloat);
typedef void(APIENTRY* PFN_UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
typedef void(APIENTRY* PFN_ActiveTexture)(GLenum);
typedef PROC(WINAPI* PFN_wglGetProcAddress)(LPCSTR);
typedef void(APIENTRY* PFN_GetUniformiv)(GLuint, GLint, GLint*);
typedef void(APIENTRY* PFN_BindTexture)(GLenum, GLuint);
typedef void(APIENTRY* PFN_DrawArrays)(GLenum, GLint, GLsizei);
typedef void(APIENTRY* PFN_Begin)(GLenum);
typedef void(APIENTRY* PFN_DrawElements)(GLenum, GLsizei, GLenum, const void*);
typedef BOOL(WINAPI* PFN_SwapBuffers)(HDC);

static PFN_wglGetProcAddress oWglGetProcAddress = nullptr;
static PFN_SwapBuffers oSwapBuffers = nullptr;
static PFN_ShaderSource oShaderSource = nullptr, oShaderSourceARB = nullptr;
static PFN_AttachShader oAttachShader = nullptr, oAttachObjectARB = nullptr;
static PFN_UseProgram oUseProgram = nullptr, oUseProgramObjectARB = nullptr;
static PFN_GetUniformLocation pGetUniformLocation = nullptr;
static PFN_Uniform1i pUniform1i = nullptr;
static PFN_Uniform1f pUniform1f = nullptr;
static PFN_Uniform2f pUniform2f = nullptr;
typedef void(APIENTRY* PFN_Uniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
static PFN_Uniform4f pUniform4f = nullptr;
static PFN_Uniform3f pUniform3f = nullptr;
static PFN_UniformMatrix4fv pUniformMatrix4fv = nullptr;
static PFN_ActiveTexture pActiveTexture = nullptr;
static PFN_ActiveTexture oActiveTexture = nullptr, oActiveTextureARB = nullptr;
static PFN_GetUniformiv pGetUniformiv = nullptr;
static PFN_BindTexture oBindTexture = nullptr;
static PFN_DrawArrays oDrawArrays = nullptr;
static PFN_Begin oBegin = nullptr;
static PFN_DrawElements oDrawElements = nullptr;

static std::set<GLuint> gPatchedShaders;
static bool gInWallPass = false;
static bool gGfxReload = false;
static int gGfxReloads = 0;      // 1.9.130: how many times the game rebuilt its graphics (the wall batch is rebuilt after each)      // 1.9.128: the game rebuilt its graphics (resolution change): 09_frame refreshes 3D mode
static GLuint gCurProg = 0;
static GLuint gOutdoorsTex = 0;
static GLenum gActiveUnit = GL_TEXTURE0;
static GLuint gBound[32] = {0};
static void DrawOwnFloor();
static void CaptureTerrain(GLint first, GLsizei count);
static std::map<GLuint, GLint> gOutdoorsUnit;      // program -> sampler unit (-1 none)
static const GLenum kIsoUnit = GL_TEXTURE0 + 7;
static std::set<GLuint> gPatchedPrograms;
static std::set<GLuint> gSunShadowShaders, gSunShadowPrograms;      // the game's flat sun-shadow layer (its fragment shader is recognisable)
static int gShadersPatched = 0;

struct Locs {
    bool init = false;
    float lastCells = -1.0f;      // the isoCellsDark value this program last got
    GLint utint = -1, uskin = -1, uhair = -1, uheads = -1, uheadgeo = -1, uheadcell = -1, udoor = -1, udoorclip = -1, wext = -1, ucells = -1, nudge = -1, cellA = -1, cellB = -1, tiles = -1, wart = -1;
    GLint cscr = -1, ubulbs = -1, unight = -1, ubon = -1, uoutl = -1;
    GLint warm, en, mode, style, focus, dir, rad, wh, stub, grit, sty, cart, toon, sat, tint, wpass, vinv, rcut, otex, lift, base, fat, thick;
};
static std::map<GLuint, Locs> gLocs;

static void LoadGlFuncs() {
    if (pGetUniformLocation) return;
    auto get = [](const char* a, const char* b) -> PROC {
        PROC p = oWglGetProcAddress ? oWglGetProcAddress(a) : nullptr;
        if (!p && b && oWglGetProcAddress) p = oWglGetProcAddress(b);
        return p;
    };
    pGetUniformLocation = (PFN_GetUniformLocation)get("glGetUniformLocation", "glGetUniformLocationARB");
    pUniform1i = (PFN_Uniform1i)get("glUniform1i", "glUniform1iARB");
    pUniform1f = (PFN_Uniform1f)get("glUniform1f", "glUniform1fARB");
    pUniform2f = (PFN_Uniform2f)get("glUniform2f", "glUniform2fARB");
    pUniform3f = (PFN_Uniform3f)get("glUniform3f", "glUniform3fARB");
    pUniform4f = (PFN_Uniform4f)get("glUniform4f", "glUniform4fARB");
    pUniformMatrix4fv = (PFN_UniformMatrix4fv)get("glUniformMatrix4fv", "glUniformMatrix4fvARB");
    pActiveTexture = (PFN_ActiveTexture)get("glActiveTexture", "glActiveTextureARB");
    pGetUniformiv = (PFN_GetUniformiv)get("glGetUniformiv", "glGetUniformivARB");
}

static unsigned gUniEpoch = 0;      // bumped whenever a program's uniforms are reset, so cached per-piece values are re-sent
static void SetIsoUniforms(GLuint prog) {
    gUniEpoch++;
    LoadGlFuncs();
    if (!pGetUniformLocation || !pUniform1i) return;
    Locs& L = gLocs[prog];
    if (!L.init) {
        L.init = true;
        L.en = pGetUniformLocation(prog, "isoEnabled");
        L.mode = pGetUniformLocation(prog, "isoWallMode");
        L.style = pGetUniformLocation(prog, "isoCutStyle");
        L.cscr = pGetUniformLocation(prog, "isoCutScreen");
        L.ubulbs = pGetUniformLocation(prog, "isoBulbs");
        L.unight = pGetUniformLocation(prog, "isoNight"); L.uoutl = pGetUniformLocation(prog, "isoOutline");
        L.ubon = pGetUniformLocation(prog, "isoBulbOn");
        L.focus = pGetUniformLocation(prog, "isoFocus");
        L.dir = pGetUniformLocation(prog, "isoCamDir");
        L.rad = pGetUniformLocation(prog, "isoCutRadius");
        L.wh = pGetUniformLocation(prog, "isoWallHeight");
        L.wpass = pGetUniformLocation(prog, "isoWallPass");
        L.vinv = pGetUniformLocation(prog, "isoViewInv");
        L.rcut = pGetUniformLocation(prog, "isoRoomCut");
        L.otex = pGetUniformLocation(prog, "isoOutdoors");
        L.cart = pGetUniformLocation(prog, "isoCartoon");
        L.toon = pGetUniformLocation(prog, "isoToon");
        L.sat = pGetUniformLocation(prog, "isoSat");
        L.tint = pGetUniformLocation(prog, "isoWallTint");
        L.stub = pGetUniformLocation(prog, "isoStub");
        L.grit = pGetUniformLocation(prog, "isoGrit");
        L.sty = pGetUniformLocation(prog, "isoStyle");
        L.warm = pGetUniformLocation(prog, "isoWarm");
        L.lift = pGetUniformLocation(prog, "isoWallLift");
        L.base = pGetUniformLocation(prog, "isoWallBase");
        L.fat = pGetUniformLocation(prog, "isoWallFat");
        L.thick = pGetUniformLocation(prog, "isoWallThick");
        L.wext = pGetUniformLocation(prog, "isoWallExt");
        L.nudge = pGetUniformLocation(prog, "isoLightNudge");
        L.utint = pGetUniformLocation(prog, "isoUniformTint");
        L.uskin = pGetUniformLocation(prog, "isoSkinTint");
        L.uhair = pGetUniformLocation(prog, "isoHairTint");
        L.uheads = pGetUniformLocation(prog, "isoHeads"); L.udoor = pGetUniformLocation(prog, "isoDoorSplit"); L.udoorclip = pGetUniformLocation(prog, "isoDoorClip"); L.ucells = pGetUniformLocation(prog, "isoCellsDark"); L.lastCells = -1.0f;
        L.uheadgeo = pGetUniformLocation(prog, "isoHeadGeo");
        L.uheadcell = pGetUniformLocation(prog, "isoHeadCell");
        L.cellA = pGetUniformLocation(prog, "isoCellA");
        L.cellB = pGetUniformLocation(prog, "isoCellB");
        L.tiles = pGetUniformLocation(prog, "isoTiles");
        L.wart = pGetUniformLocation(prog, "isoWallArt");
        Log("Program %u uniforms: en=%d mode=%d focus=%d grit=%d", prog, L.en, L.mode, L.focus, L.grit);
    }
    bool on = gCfg.isoEnabled && gCam.active;
    if (L.en >= 0) pUniform1i(L.en, on ? 1 : 0);
    if (L.mode >= 0) pUniform1i(L.mode, gCfg.wallMode);
    if (L.style >= 0) pUniform1i(L.style, 0);      // the dither fade is dropped (user 8 Nov): always shrink
    if (L.cscr >= 0) pUniform1f(L.cscr, gCfg.cutScreen);
    if (L.ubulbs >= 0) pUniform1i(L.ubulbs, 14);
    if (L.unight >= 0) pUniform1f(L.unight, gNight * gCfg.nightDark);
    if (L.uoutl >= 0) pUniform1f(L.uoutl, gCfg.outline);
    if (L.ubon >= 0) pUniform1f(L.ubon, (gCfg.bulbPools && gBulbReady) ? 1.0f : 0.0f);
    if (L.focus >= 0) pUniform2f(L.focus, gCam.focus[0], gCam.focus[1]);
    if (L.dir >= 0) pUniform2f(L.dir, gCam.camDir[0], gCam.camDir[1]);
    if (L.rad >= 0) pUniform1f(L.rad, gCfg.cutRadius);
    if (L.vinv >= 0 && pUniformMatrix4fv) pUniformMatrix4fv(L.vinv, 1, GL_FALSE, gViewInv);
    if (L.rcut >= 0) pUniform1i(L.rcut, (gCfg.roomCut && gOutdoorsTex) ? 1 : 0);
    if (L.otex >= 0) pUniform1i(L.otex, 7);
    if (L.wpass >= 0) pUniform1i(L.wpass, gInWallPass ? 1 : 0);
    if (L.cart >= 0) pUniform1i(L.cart, gCfg.cartoon ? 1 : 0);
    if (L.toon >= 0) pUniform1f(L.toon, gCfg.toon);
    if (L.sat >= 0) pUniform1f(L.sat, gCfg.sat);
    if (L.tint >= 0 && pUniform3f) pUniform3f(L.tint, gCfg.wallTint[0], gCfg.wallTint[1], gCfg.wallTint[2]);
    if (L.wh >= 0) pUniform1f(L.wh, gCfg.wallHeight);
    if (L.stub >= 0) pUniform1f(L.stub, gCfg.wallMode == 2 ? gCfg.downStub : gCfg.stub);      // walls down has its own (lower) height
    if (L.grit >= 0) pUniform1f(L.grit, gCfg.grit);
    if (L.warm >= 0) pUniform1f(L.warm, gCfg.warm);
    if (L.sty >= 0) pUniform1f(L.sty, gCfg.style);
    if (L.lift >= 0) pUniform1f(L.lift, gCfg.wallLift);
    if (L.base >= 0) pUniform1f(L.base, -0.989f);
    if (L.fat >= 0) pUniform1f(L.fat, gCfg.wallThick * 2.0f);
    if (L.thick >= 0) pUniform2f(L.thick, 0.0f, 0.0f);
    // (1.9.121: isoWallExt is not reset here - only wall pieces read it, and 05_walls sends it when it changes)
    if (L.nudge >= 0) pUniform2f(L.nudge, 0.0f, 0.0f);
    if (L.cellA >= 0) pUniform1i(L.cellA, 9);
    if (L.cellB >= 0) pUniform1i(L.cellB, 10);
    if (L.tiles >= 0) pUniform1i(L.tiles, 11);
    if (L.wart >= 0) pUniform1f(L.wart, (gCfg.wallArt && gCellTexA && gTilesetTex) ? 1.0f : 0.0f);
}

// The game binds its programs rarely, so push our uniforms to every patched
// program once per frame (restoring whatever program was bound).
static void RefreshAllIsoUniforms() {
    PFN_UseProgram use = oUseProgram ? oUseProgram : oUseProgramObjectARB;
    if (!use || gPatchedPrograms.empty()) return;
    GLint cur = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &cur);
    for (GLuint p : gPatchedPrograms) {
        use(p);
        SetIsoUniforms(p);
    }
    use((GLuint)cur);
    while (glGetError() != GL_NO_ERROR) {}
}

static const char* PickReplacement(const std::string& src) {
    bool vs = src.find("worldPosition") != std::string::npos && src.find("ftransform") != std::string::npos &&
              src.find("currentPosition") != std::string::npos;
    bool fs = src.find("vismapPrevious") != std::string::npos && src.find("DoLighting") != std::string::npos;
    if (vs) return kIsoVS;
    if (fs) return kIsoFS;
    return nullptr;
}

// Splice our code into the LightMap* shaders. Returns empty string if not applicable.
static std::string PatchLightMap(const std::string& src) {
    if (src.find("isoStylize") != std::string::npos || src.find("isoCutVertex") != std::string::npos) return "";
    // vertex: lightmap.vs
    const char* vsLine = "gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;";
    if (src.find("currentPosition = vec2( gl_Vertex.x/worldSize.x") != std::string::npos && src.find("worldPosition") == std::string::npos) {
        size_t p = src.find(vsLine);
        size_t m = src.find("void main");
        if (p == std::string::npos || m == std::string::npos) return "";
        std::string out = src;
        out.replace(p, strlen(vsLine), "gl_Position = gl_ModelViewProjectionMatrix * isoCutVertex(gl_Vertex);");
        out.insert(m, kLmVsHeader);
        return out;
    }
    // fragment: lightmap.fs family
    const char* fsLine = "gl_FragColor = finalColor;";
    if (src.find("lightmap") != std::string::npos && src.find("currentPosition") != std::string::npos &&
        src.find("DoLighting") == std::string::npos) {
        size_t p = src.rfind(fsLine);
        size_t m = src.find("void main");
        if (p == std::string::npos || m == std::string::npos) return "";
        std::string out = src;
        out.replace(p, strlen(fsLine), "gl_FragColor = isoStylize(finalColor);");
        {      // route the tileset / sprite sample through the softening helper
            const char* smp = "texture2D( sampler0, gl_TexCoord[0].xy )";
            size_t q = out.find(smp);
            if (q != std::string::npos) out.replace(q, strlen(smp), "isoTex( gl_TexCoord[0].xy )");
        }
        out.insert(m, kLmFsHeader);
        return out;
    }
    return "";
}

static void ShaderSourceCommon(PFN_ShaderSource real, GLuint sh, GLsizei n, const char* const* s, const GLint* len) {
    std::string all;
    for (GLsizei i = 0; i < n; i++) {
        if (!s[i]) continue;
        if (len && len[i] >= 0)
            all.append(s[i], len[i]);
        else
            all.append(s[i]);
    }
    if (all.find("darker shadows at midday") != std::string::npos) {
        gSunShadowShaders.insert(sh);
        Log("Found the sun-shadow fragment shader (id %u)", sh);
    }
    std::string lm = PatchLightMap(all);
    if (!lm.empty()) {
        gPatchedShaders.insert(sh);
        gShadersPatched++;
        Log("Patched LightMap-family shader (id %u)", sh);
        const char* p = lm.c_str();
        real(sh, 1, &p, nullptr);
        return;
    }
    const char* rep = PickReplacement(all);
    if (rep) {
        gPatchedShaders.insert(sh);
        gShadersPatched++;
        Log("Replaced 3D-mode %s shader (id %u)", rep == kIsoVS ? "vertex" : "fragment", sh);
        real(sh, 1, &rep, nullptr);
        return;
    }
    real(sh, n, s, len);
}
static void APIENTRY hkShaderSource(GLuint sh, GLsizei n, const char* const* s, const GLint* l) { ShaderSourceCommon(oShaderSource, sh, n, s, l); }
static void APIENTRY hkShaderSourceARB(GLuint sh, GLsizei n, const char* const* s, const GLint* l) {
    ShaderSourceCommon(oShaderSourceARB, sh, n, s, l);
}
static void APIENTRY hkAttachShader(GLuint p, GLuint sh) {
    if (gPatchedShaders.count(sh)) gPatchedPrograms.insert(p);
    if (gSunShadowShaders.count(sh)) gSunShadowPrograms.insert(p);
    oAttachShader(p, sh);
}
static void APIENTRY hkAttachObjectARB(GLuint p, GLuint sh) {
    if (gPatchedShaders.count(sh)) gPatchedPrograms.insert(p);
    if (gSunShadowShaders.count(sh)) gSunShadowPrograms.insert(p);
    oAttachObjectARB(p, sh);
}
static int gFrProgCalls = 0;
static void ResetPrisonerUniforms();      // parts/07_draw.inc: clears a prisoner's colours / head left on the program
static void APIENTRY hkUseProgram(GLuint p) {
    gFrProgCalls++;
    if (p != gCurProg) ResetPrisonerUniforms();
    gCurProg = p;
    oUseProgram(p);
    if (p && gPatchedPrograms.count(p)) SetIsoUniforms(p);
}
static void APIENTRY hkUseProgramObjectARB(GLuint p) {
    if (p != gCurProg) ResetPrisonerUniforms();
    gCurProg = p;
    oUseProgramObjectARB(p);
    if (p && gPatchedPrograms.count(p)) SetIsoUniforms(p);
}

static void APIENTRY hkActiveTexture(GLenum u) {
    gActiveUnit = u;
    oActiveTexture(u);
}
static void APIENTRY hkActiveTextureARB(GLenum u) {
    gActiveUnit = u;
    oActiveTextureARB(u);
}
static void APIENTRY hkBindTexture(GLenum target, GLuint tex) {
    if (target == GL_TEXTURE_2D) {
        unsigned i = gActiveUnit - GL_TEXTURE0;
        if (i < 32) gBound[i] = tex;
    }
    oBindTexture(target, tex);
}
// The game uploads its tileset with glTexImage2D: swap in the modernised pixels (see TilesetModernLoad).
typedef void(APIENTRY* PFN_TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
static PFN_TexImage2D oTexImage2D = nullptr;
static void APIENTRY hkTexImage2D(GLenum target, GLint level, GLint ifmt, GLsizei w, GLsizei h, GLint border, GLenum fmt, GLenum type,
                                  const void* px) {
    if (level == 0 && w == 400 && h == 486) {
        GLint au = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &au);
        Log("TEXUPLOAD 400x486 (periodic image) active unit %d bound0 %u bound1 %u cam %d", (int)(au - GL_TEXTURE0), gBound[0], gBound[1],
            (int)gCam.active);
    }
    if (level == 0 && w >= 2048 && h >= 2048)
        Log("glTexImage2D %dx%d fmt=0x%x type=0x%x data=%s", (int)w, (int)h, (unsigned)fmt, (unsigned)type, px ? "yes" : "null");
    if (target == GL_TEXTURE_2D && level == 0 && w == 4608 && h == 4608 && fmt == 0x1908 && type == 0x1401 && px && TilesetModernLoad()) {
        Log("Tileset upload replaced with the modernised art");
        px = gTilesetModern.data();
    }
    if (target == GL_TEXTURE_2D && level == 0 && w == 4608 && h == 4608 && fmt == 0x1908 && type == 0x1401) {
        unsigned ui = gActiveUnit - GL_TEXTURE0;
        if (ui < 32) gTilesetTex = gBound[ui];
        Log("Tileset texture id = %u", gTilesetTex);
        // 1.9.128 (user: switched resolution and the game went black): a second tileset upload = the game rebuilt its graphics (it does on a
        // resolution / window change). Its 3D-mode shaders are only rebuilt when 3D mode is switched on, and our per-program uniform
        // locations are stale: drop them and switch 3D mode off and on again (09_frame), as after a world load
        static int uploads = 0;
        if (++uploads >= 2) {
            gLocs.clear();
            gGfxReloads++;
            gGfxReload = true;
            Log("The game reloaded its graphics (resolution or window change): shader settings re-read, 3D mode refreshed");
        }
    }
    oTexImage2D(target, level, ifmt, w, h, border, fmt, type, px);
}
// Flicker diagnostic: texture updates (glTexSubImage2D / glCopyTexSubImage2D). Rare ones are logged with a timestamp so a periodic one lines up with the flicker.
typedef void(APIENTRY* PFN_TexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
typedef void(APIENTRY* PFN_CopyTexSubImage2D)(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
static PFN_TexSubImage2D oTexSubImage2D = nullptr;
static PFN_CopyTexSubImage2D oCopyTexSubImage2D = nullptr;
static void NoteTexUpdate(const char* kind, int w, int h) {
    static std::map<long long, int> perSize;
    static double t0 = 0;
    static int lines = 0;
    double now = NowSec();
    if (t0 == 0) t0 = now;
    long long key = ((long long)w << 20) ^ ((long long)h << 1) ^ (kind[0] == 'c');
    int& n = perSize[key];
    n++;
    if (lines < 300 && n <= 3 && gCam.active) {
        lines++;
        Log("TEXUPD %s %dx%d unit %u tex %u", kind, w, h, gActiveUnit - GL_TEXTURE0, gBound[(gActiveUnit - GL_TEXTURE0) & 31]);
    }
    if (now - t0 > 10.0) {
        t0 = now;
        std::string s;
        for (auto& kv : perSize) {
            char b[64];
            snprintf(b, sizeof b, " %s%dx%d:%d", (kv.first & 1) ? "copy " : "", (int)(kv.first >> 20), (int)((kv.first >> 1) & 0x7FFFF), kv.second);
            s += b;
        }
        if (lines < 300) {
            lines++;
            Log("TEXUPD 10 s totals:%s", s.c_str());
        }
        perSize.clear();
    }
}
static void APIENTRY hkTexSubImage2D(GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, const void* px) {
    if (l == 0) NoteTexUpdate("sub", w, h);
    oTexSubImage2D(t, l, x, y, w, h, f, ty, px);
}
static void APIENTRY hkCopyTexSubImage2D(GLenum t, GLint l, GLint xo, GLint yo, GLint x, GLint y, GLsizei w, GLsizei h) {
    if (l == 0) NoteTexUpdate("copy", w, h);
    oCopyTexSubImage2D(t, l, xo, yo, x, y, w, h);
}
// When a patched LightMap program draws, note which texture sits on its "outdoors" unit.
