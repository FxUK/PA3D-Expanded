// parts/11_own_floor.inc - the 3D floor drawn from the game's own 2D floor art. Part of main.cpp (one translation unit, included in order): it uses everything defined in the parts before it.
// ---------------------------------------------------------------------------
// Indoor map read straight from the game's cell grid:
//   world = *(app+0x198); width @+0x60, height @+0x64, cells @+0x48e8,
//   cell stride 0xe0, Indoors flag (bool) @ cell+0x30.
// ---------------------------------------------------------------------------
// Own floor: a procedural, stylised floor drawn over the game's terrain (see FloorHook). One quad over the map; the
// fragment shader looks up each cell's material id (a byte texture built from the cell grid) and paints that material,
// blending soft edges between natural materials. The game's own lightmap is applied on top.
// ---------------------------------------------------------------------------
static const char* kFloorVS = R"GLSL(
varying vec2 wp;
void main(void) { wp = gl_Vertex.xy; gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex; }
)GLSL";
static const char* kFloorFS = R"GLSL(
#extension GL_ARB_shader_texture_lod : enable
uniform sampler2D isoMat;
uniform sampler2D lightmap;
uniform sampler2D isoCellA;      // per cell: tileset uv at the cell's corner (xy) and one tile along x (zw)
uniform sampler2D isoCellB;      // per cell: uv one tile along y (xy), b = mapped
uniform sampler2D isoTiles;      // the game's tileset
uniform float haveCells;
uniform float tileSoft;         // mip bias: softer, low-poly look
uniform float tileFlat;         // how much each tile is pulled towards its own average colour
uniform vec2  worldSize;
uniform float haveLight;
uniform float floorBright;      // < 1 tones down near-white floors (white tile)
uniform float isoWarm;
uniform sampler2D isoRooms;      // room map (a = 255: a cell)
uniform float isoCellsDark;      // 0..1: cells' lights off (Sleep regime)
uniform sampler2D isoBulbs;      // lamp pools: r = indoor bulbs, g = outdoor lamps, b = near an indoor bulb
uniform float isoNight;          // 0 day .. 1 full night
uniform float isoBulbOn;
varying vec2 wp;

// The light on a spot: the game's lightmap, a moonlit floor at night instead of the old fixed grey, and each lamp's own pool.
vec3 isoNightLight(vec3 lm, vec2 p, float indoor, float dayMin)
{
    vec3 lmv = mix(vec3(dot(lm, vec3(0.333))), lm, 0.5);
    vec3 moon = vec3(0.13, 0.16, 0.26);
    vec3 L = max(lmv, mix(vec3(dayMin, dayMin, dayMin + 0.06), moon, isoNight));
    if (isoBulbOn > 0.5) {
        vec4 b = texture2D(isoBulbs, p / worldSize);
        float cov = b.b * step(0.5, indoor);
        vec3 bulb = mix(vec3(1.0), vec3(1.05, 0.98, 0.88), b.r);      // warm incandescent under the bulb
        L *= mix(vec3(1.0), (0.84 + 0.24 * b.r) * bulb, cov);          // many bulbs, not one even panel: brighter under each, dimmer between
        float o = (1.0 - step(0.5, indoor)) * isoNight;
        L = L * (1.0 - 0.35 * o * (1.0 - b.g)) + o * b.g * vec3(1.0, 0.85, 0.60) * 0.80;      // outdoor lamps light real pools on the dark ground
    }
    L = min(L, vec3(1.0));      // 1.9.76: never brighter than the game's full light (user: overexposed)
    return L;
}

float h21(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
vec2  h22(vec2 p) { return vec2(h21(p), h21(p + vec2(17.3, 5.1))); }
float vn(vec2 p) { vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
    return mix(mix(h21(i), h21(i + vec2(1.0, 0.0)), f.x), mix(h21(i + vec2(0.0, 1.0)), h21(i + vec2(1.0, 1.0)), f.x), f.y); }
float fbm(vec2 p) { return vn(p) * 0.55 + vn(p * 2.03 + 7.1) * 0.30 + vn(p * 4.1 + 3.7) * 0.15; }
float matAt(vec2 cell) { return floor(texture2D(isoMat, (cell + 0.5) / worldSize).r * 255.0 + 0.5); }

vec3 tilesF(vec2 p, float s, vec3 a, vec3 b, vec3 gr) {
    vec2 q = p / s; vec2 id = floor(q); vec2 f = fract(q);
    float g = min(min(f.x, 1.0 - f.x), min(f.y, 1.0 - f.y));
    float ln = 1.0 - smoothstep(0.015, 0.05, g);
    vec3 c = mix(a, b, h21(id) * 0.7 + 0.3 * fbm(p * 2.0)); c *= 0.95 + 0.10 * fbm(p * 8.0);
    return mix(c, gr, ln * 0.75);
}
vec3 planksF(vec2 p, vec3 a, vec3 b) {
    float row = floor(p.y / 0.25); float off = h21(vec2(row, 3.0)) * 3.0;
    float u = (p.x + off) / 1.2; float id = floor(u); float fx = fract(u); float fy = fract(p.y / 0.25);
    float ln = (1.0 - smoothstep(0.0, 0.04, fx)) + (1.0 - smoothstep(0.0, 0.08, fy));
    vec3 c = mix(a, b, h21(vec2(id, row))); c *= 0.92 + 0.16 * vn(vec2(p.x * 3.0, p.y * 40.0));
    return mix(c, c * 0.55, clamp(ln, 0.0, 1.0) * 0.6);
}
vec3 cobblesF(vec2 p, vec3 a, vec3 b) {
    vec2 q = p * 2.0; vec2 i = floor(q); vec2 f = fract(q);
    float d1 = 9.0, d2 = 9.0; vec2 id = vec2(0.0);
    for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
        vec2 g = vec2(float(x), float(y)); vec2 o = h22(i + g); vec2 r = g + o - f; float d = dot(r, r);
        if (d < d1) { d2 = d1; d1 = d; id = i + g; } else if (d < d2) d2 = d;
    }
    float edge = sqrt(d2) - sqrt(d1); vec3 c = mix(a, b, h21(id)); c *= 0.9 + 0.2 * fbm(p * 6.0);
    return mix(c * 0.5, c, smoothstep(0.0, 0.12, edge));
}
vec3 grassF(vec2 p) { float n = fbm(p * 1.3); float blade = vn(vec2(p.x * 14.0, p.y * 3.0));
    return mix(vec3(0.34, 0.60, 0.15), vec3(0.50, 0.74, 0.20), n) * (0.88 + 0.20 * blade); }
vec3 dirtF(vec2 p) { float n = fbm(p * 1.1); vec3 c = mix(vec3(0.50, 0.35, 0.20), vec3(0.64, 0.46, 0.26), n); return c * (0.90 + 0.20 * vn(p * 9.0)); }
vec3 concreteF(vec2 p) { return vec3(0.58, 0.57, 0.55) * (0.90 + 0.18 * fbm(p * 3.0)); }

bool soft(float m) { return m < 22.5 || (m > 29.5 && m < 30.5); }

vec3 floorColour(float m, vec2 p) {
    if (m > 7.5 && m < 8.5) return dirtF(p);
    if (m > 8.5 && m < 9.5) return mix(vec3(0.88, 0.91, 0.95), vec3(0.97, 0.98, 1.0), fbm(p * 2.0));
    if (m > 9.5 && m < 10.5) return tilesF(p, 0.5, vec3(0.50, 0.47, 0.50), vec3(0.62, 0.58, 0.60), vec3(0.30, 0.29, 0.31));
    if (m > 10.5 && m < 11.5) return grassF(p);
    if (m > 11.5 && m < 12.5) return grassF(p * 0.8) * vec3(0.86, 0.92, 0.72);
    if (m > 12.5 && m < 13.5) return dirtF(p * 0.7) * vec3(0.58, 0.50, 0.42);
    if (m > 13.5 && m < 14.5) return cobblesF(p * 1.6, vec3(0.55, 0.52, 0.46), vec3(0.66, 0.62, 0.55));
    if (m > 14.5 && m < 15.5) return vec3(0.27, 0.27, 0.29) * (0.9 + 0.2 * fbm(p * 5.0));
    if (m > 15.5 && m < 21.5) { vec3 r = vec3(0.27, 0.27, 0.29) * (0.9 + 0.2 * fbm(p * 5.0)); float l = step(abs(fract(p.x * 2.0) - 0.5), 0.07) * step(0.2, fract(p.y * 2.0)); return mix(r, vec3(0.92, 0.9, 0.7), l * 0.6); }
    if (m > 21.5 && m < 22.5) return cobblesF(p, vec3(0.50, 0.50, 0.52), vec3(0.62, 0.62, 0.64));
    if (m > 23.5 && m < 24.5) return tilesF(p, 0.5, vec3(0.62, 0.61, 0.60), vec3(0.70, 0.69, 0.67), vec3(0.38, 0.37, 0.36));
    if (m > 24.5 && m < 25.5) return concreteF(p);
    if (m > 25.5 && m < 26.5) return planksF(p, vec3(0.66, 0.44, 0.22), vec3(0.78, 0.55, 0.30));
    if (m > 26.5 && m < 27.5) return tilesF(p, 0.5, vec3(0.90, 0.88, 0.84), vec3(0.97, 0.95, 0.91), vec3(0.62, 0.61, 0.58));
    if (m > 27.5 && m < 28.5) return tilesF(p, 0.25, vec3(0.30, 0.46, 0.62), vec3(0.78, 0.74, 0.62), vec3(0.85, 0.85, 0.85));
    if (m > 28.5 && m < 29.5) return tilesF(p, 0.5, vec3(0.50, 0.53, 0.57), vec3(0.58, 0.61, 0.65), vec3(0.30, 0.32, 0.35));
    if (m > 29.5 && m < 30.5) return mix(vec3(0.82, 0.72, 0.50), vec3(0.90, 0.80, 0.58), fbm(p * 2.5)) * (0.94 + 0.08 * vn(p * 20.0));
    if (m > 30.5 && m < 31.5) return tilesF(p, 1.0, vec3(0.90, 0.89, 0.87), vec3(0.97, 0.96, 0.94), vec3(0.70, 0.69, 0.68));
    if (m > 31.5 && m < 32.5) return tilesF(p, 0.5, vec3(0.93, 0.93, 0.92), vec3(0.98, 0.98, 0.97), vec3(0.72, 0.72, 0.72));
    if (m > 32.5 && m < 33.5) { vec3 c = tilesF(p, 0.5, vec3(0.78, 0.66, 0.50), vec3(0.88, 0.76, 0.58), vec3(0.45, 0.35, 0.25)); return c; }
    if (m > 33.5 && m < 34.5) return tilesF(p, 0.5, vec3(0.36, 0.39, 0.43), vec3(0.45, 0.48, 0.52), vec3(0.20, 0.22, 0.25));
    if (m > 34.5 && m < 35.5) return tilesF(p, 0.5, vec3(0.45, 0.44, 0.40), vec3(0.54, 0.52, 0.47), vec3(0.25, 0.25, 0.22));
    if (m > 35.5 && m < 36.5) { vec2 c = floor(p * 2.0); float ck = mod(c.x + c.y, 2.0); return mix(vec3(0.88, 0.86, 0.80), vec3(0.34, 0.36, 0.40), ck) * (0.94 + 0.10 * fbm(p * 7.0)); }
    if (m > 39.5 && m < 40.5) return tilesF(p, 0.5, vec3(0.93, 0.93, 0.92), vec3(0.98, 0.98, 0.97), vec3(0.72, 0.72, 0.72));   // ids 40/41 are big indoor floors in real maps (kitchen), not water
    if (m > 40.5 && m < 41.5) return tilesF(p, 0.5, vec3(0.50, 0.53, 0.57), vec3(0.58, 0.61, 0.65), vec3(0.30, 0.32, 0.35));
    if ((m > 36.5 && m < 39.5) || (m > 41.5 && m < 254.5 && false) || m > 254.5) {   // wall cells (learned per save: marked 255): a darker brick footing, so the strip beside the thin wall reads as part of it
        vec2 bq = p * vec2(2.0, 4.0); float brw = floor(bq.y); float bo = mod(brw, 2.0) * 0.5;
        float bx = fract(bq.x + bo), by = fract(bq.y);
        float ml = clamp(step(by, 0.12) + step(bx, 0.07), 0.0, 1.0);
        vec3 bc = vec3(0.40, 0.36, 0.32) * (0.85 + 0.25 * h21(vec2(floor(bq.x + bo), brw)));
        return mix(bc, vec3(0.24, 0.22, 0.20), ml * 0.8);
    }
    return concreteF(p);          // anything else: plain concrete
}

void main(void)
{
    vec2 cell = floor(wp); vec2 f = wp - cell;
    float m = matAt(cell);
    vec2 src = cell;
    bool wallUnder = false; vec3 wallCol = vec3(0.0);
    if (m > 254.5 && haveCells > 0.5) {      // 1.9.113 (user: the strip under a thinner wall should match the wall, not the next tile): the wall's own
        vec4 wb = texture2D(isoCellB, (cell + 0.5) / worldSize);      // colour, from its 2D art's average (fences keep the floor: their art is see-through)
        if (wb.b > 0.5) {
            vec4 wa = texture2D(isoCellA, (cell + 0.5) / worldSize);
            bool fence = wa.x < 0.07 && wa.y > 0.695 && wa.y < 0.765;
            if (!fence) { wallUnder = true; wallCol = texture2D(isoTiles, wa.xy + 0.5 * (wa.zw - wa.xy) + 0.5 * (wb.xy - wa.xy), 7.0).rgb; }
        }
    }
    if (m > 254.5 && !wallUnder) {      // a wall tile: the floor runs on under the wall - the nearest neighbouring floor (sides first, then corners), never the wall's 2D art
        // (1.9.97, user: the wall tiles' own art / brick footing looked worse than the surrounding floor)
        vec2 nb[8]; nb[0] = vec2(1.0, 0.0); nb[1] = vec2(-1.0, 0.0); nb[2] = vec2(0.0, 1.0); nb[3] = vec2(0.0, -1.0);
        nb[4] = vec2(1.0, 1.0); nb[5] = vec2(-1.0, 1.0); nb[6] = vec2(1.0, -1.0); nb[7] = vec2(-1.0, -1.0);
        bool found = false;
        for (int k = 0; k < 8; k++) {      // first a neighbour with the game's own floor art
            vec2 c2 = cell + nb[k];
            if (!found && haveCells > 0.5 && matAt(c2) < 254.5 && texture2D(isoCellB, (c2 + 0.5) / worldSize).b > 0.5) { src = c2; m = matAt(c2); found = true; }
        }
        for (int k = 0; k < 8; k++) {      // else any neighbouring floor material
            vec2 c2 = cell + nb[k];
            if (!found && matAt(c2) < 254.5) { src = c2; m = matAt(c2); found = true; }
        }
        if (!found) m = 25.0;      // walls all round: plain concrete
    }
    vec4 cb = texture2D(isoCellB, (src + 0.5) / worldSize);
    bool mapped = haveCells > 0.5 && cb.b > 0.5 && m < 254.5;
    if (!mapped && m > 14.5 && m < 21.5) discard;     // roads and their markings: leave the game's own terrain (it has the correct centre dashes)
    vec3 col;
    if (mapped) {      // the game's own 2D floor art for this cell, softened a little
        vec4 ca = texture2D(isoCellA, (src + 0.5) / worldSize);
        vec2 uv = ca.xy + f.x * (ca.zw - ca.xy) + f.y * (cb.xy - ca.xy);
        vec2 ex = ca.zw - ca.xy, ey = cb.xy - ca.xy; float sc = exp2(tileSoft);
#ifdef GL_ARB_shader_texture_lod
        vec2 dwx = dFdx(wp), dwy = dFdy(wp);      // gradients from the continuous world position: no seams where one cell's mapping meets the next
        vec3 t = texture2DGradARB(isoTiles, uv, (ex * dwx.x + ey * dwx.y) * sc, (ex * dwy.x + ey * dwy.y) * sc).rgb;
#else
        vec3 t = texture2D(isoTiles, uv, tileSoft).rgb;
#endif
        vec3 avg = texture2D(isoTiles, ca.xy + 0.5 * (ca.zw - ca.xy) + 0.5 * (cb.xy - ca.xy), 7.0).rgb;
        col = mix(t, avg, tileFlat);
    } else if (wallUnder) col = wallCol * 1.15 * (floorColour(255.0, wp) / vec3(0.40, 0.36, 0.32));      // the wall's colour with the footing's brick courses
    else col = floorColour(m, wp);
    if (!mapped && soft(m)) {
        float wob = 0.22 + 0.18 * fbm(wp * 3.0);
        float mr = matAt(cell + vec2(1.0, 0.0)); float ml = matAt(cell - vec2(1.0, 0.0));
        float mu = matAt(cell + vec2(0.0, 1.0)); float md = matAt(cell - vec2(0.0, 1.0));
        if (mr != m && soft(mr)) col = mix(col, floorColour(mr, wp), (1.0 - smoothstep(0.0, wob, 1.0 - f.x)) * 0.5);
        if (ml != m && soft(ml)) col = mix(col, floorColour(ml, wp), (1.0 - smoothstep(0.0, wob, f.x)) * 0.5);
        if (mu != m && soft(mu)) col = mix(col, floorColour(mu, wp), (1.0 - smoothstep(0.0, wob, 1.0 - f.y)) * 0.5);
        if (md != m && soft(md)) col = mix(col, floorColour(md, wp), (1.0 - smoothstep(0.0, wob, f.y)) * 0.5);
    }
    // slow, large-scale variation so wide areas do not look stamped
    col *= 0.93 + 0.14 * fbm(wp * 0.09);
    if (haveLight > 0.5) {
        vec3 lm = texture2D(lightmap, wp / worldSize).rgb;
        float indoor = texture2D(isoRooms, (floor(wp) + 0.5) / worldSize).r;
        vec3 L = isoNightLight(lm, wp, indoor, 0.52);
        L = mix(L, L * vec3(1.06, 0.98, 0.86), isoWarm);                       // the pools of light turn orange
        col *= L;
    }
    float lumf = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(lumf), col, 0.86) * 0.93;            // duller
    col *= mix(1.0, floorBright, smoothstep(0.55, 0.85, dot(col, vec3(0.299, 0.587, 0.114))));      // near-white tiles are too bright: pull them down, leave dark floors alone
    col += (h21(gl_FragCoord.xy) - 0.5) * 0.045;        // grittier: fine grain
    col *= mix(vec3(1.0), vec3(1.0, 0.91, 0.78), isoWarm);
    if (isoCellsDark > 0.0) col *= mix(1.0, 0.70, isoCellsDark * step(0.9, texture2D(isoRooms, (floor(wp) + 0.5) / worldSize).a));      // lights off in cells at Sleep
    gl_FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
)GLSL";

typedef GLuint(APIENTRY* PFN_CreateShader)(GLenum);
typedef void(APIENTRY* PFN_ShaderSrc)(GLuint, GLsizei, const char* const*, const GLint*);
typedef void(APIENTRY* PFN_CompileShader)(GLuint);
typedef GLuint(APIENTRY* PFN_CreateProgram)(void);
typedef void(APIENTRY* PFN_AttachShader)(GLuint, GLuint);
typedef void(APIENTRY* PFN_LinkProgram)(GLuint);
typedef void(APIENTRY* PFN_GetShaderiv)(GLuint, GLenum, GLint*);
typedef void(APIENTRY* PFN_GetInfoLog)(GLuint, GLsizei, GLsizei*, char*);
typedef void(APIENTRY* PFN_GetProgramiv)(GLuint, GLenum, GLint*);

static std::vector<unsigned char> gMatBuf;
// The 3D floor is drawn from the game's own 2D floor art: while the game draws its terrain (tileset bound), each cell's texture mapping is
// read back from the triangles (an affine map from world position to tileset uv) and stored per cell; the own-floor shader then samples the
// tileset with it. So wood, concrete, paths and grass look exactly as in the 2D view, on every save.

static uint32_t gCaptureId = 0;
static int gCaptureFrame = -1;
static void CaptureTerrain(GLint first, GLsizei count) {
    if (!gCfg.floorFromTileset || !gCam.active || gInWallPass || count < 3) return;
    // a capture covers one WHOLE frame, armed at the swap: starting part-way through a frame let a later overlay layer (dirt, blood) claim cells first
    static std::unordered_map<uint32_t, int> freqPrev, freqCur;
    static std::vector<uint32_t> cellKey;      // how common each tileset tile is: floors repeat, stains and overlays are rare
    if (gCaptureFrame != gSwapCount) {
        if (!gCaptureArmed) return;
        gCaptureArmed = false;
        gCaptureFrame = gSwapCount;
        gCaptureId++;
        if (!freqCur.empty()) {
            long long tot = 0;
            for (auto& kv : freqCur) { freqPrev[kv.first] += kv.second; }
            for (auto& kv : freqPrev) tot += kv.second;
            if (tot > 20000000)
                for (auto& kv : freqPrev) kv.second /= 2;
            freqCur.clear();
        }
    }
    uint8_t* app = App();
    if (!app) return;
    uint8_t* world = *(uint8_t**)(app + 0x198);
    if (!Readable(world, 0x48f0)) return;
    int w = *(int*)(world + 0x60), h = *(int*)(world + 0x64);
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return;
    if (w != gCellW || h != gCellH) {
        gCellW = w;
        gCellH = h;
        gCellA.assign((size_t)w * h * 4, 0);
        gCellB.assign((size_t)w * h * 4, 0);
        gCellStamp.assign((size_t)w * h, 0);
    }
    if (cellKey.size() != (size_t)w * h) cellKey.assign((size_t)w * h, 0);
    static std::vector<uint32_t> frameKey, pendKey, pendCap;
    static std::vector<uint8_t> pendN;      // a tile only changes art after winning 3 captures in a row: a one-off overlay never flips it
    if (frameKey.size() != (size_t)w * h) {
        frameKey.assign((size_t)w * h, 0);
        pendKey.assign((size_t)w * h, 0);
        pendCap.assign((size_t)w * h, 0);
        pendN.assign((size_t)w * h, 0);
    }
    void* vp = nullptr;
    void* tp = nullptr;
    glGetPointerv(0x808E, &vp);
    glGetPointerv(0x8092, &tp);
    GLint vsz = 0, tsz = 0, vst = 0, tst = 0, vty = 0, tty = 0;
    glGetIntegerv(0x807A, &vsz);
    glGetIntegerv(0x8088, &tsz);
    glGetIntegerv(0x807C, &vst);
    glGetIntegerv(0x808A, &tst);
    glGetIntegerv(0x807B, &vty);
    glGetIntegerv(0x8089, &tty);
    if (!vp || !tp || vsz < 2 || tsz < 2 || vty != 0x1406 || tty != 0x1406 || gArrayVbo) return;
    {      // the game's terrain comes in 3 layers: cell types and 2nd layer (vertex + uv) and "details" (vertex + uv + COLOUR: dirt, stains,
        // overlays). Details never decide a tile's floor art - but only once plain layers have been seen, so a build without them still works.
        static unsigned plainSeen = 0;
        bool col = glIsEnabled(0x8076) != 0;
        static int dl = 0;
        if (dl < 6) {
            dl++;
            Log("FLOORLAYER draw of %d vertices, colour array %d", (int)count, (int)col);
        }
        if (!col)
            plainSeen = gCaptureId;
        else if (plainSeen && gCaptureId - plainSeen < 20)
            return;
    }
    int vs = vst ? vst : vsz * 4, ts = tst ? tst : tsz * 4;
    if (!Readable((uint8_t*)vp + (size_t)first * vs, (size_t)count * vs) || !Readable((uint8_t*)tp + (size_t)first * ts, (size_t)count * ts)) return;
    {
        static std::set<uintptr_t> bases;
        uintptr_t bb = (uintptr_t)vp >> 23;
        if (bases.size() < 8 && bases.insert(bb).second)
            Log("FLOORBUF terrain vertices from buffer %p (first %d count %d)", vp, (int)first, (int)count);
    }
    int stored = 0;
    for (int i = 0; i + 2 < count; i += 3) {
        double X[3], Y[3], U[3], V[3];
        for (int k = 0; k < 3; k++) {
            const float* P = (const float*)((uint8_t*)vp + (size_t)(first + i + k) * vs);
            const float* T = (const float*)((uint8_t*)tp + (size_t)(first + i + k) * ts);
            X[k] = P[0];
            Y[k] = P[1];
            U[k] = T[0];
            V[k] = T[1];
        }
        double det = (X[1] - X[0]) * (Y[2] - Y[0]) - (X[2] - X[0]) * (Y[1] - Y[0]);
        if (fabs(det) < 1e-9) continue;
        int cx = (int)floor((X[0] + X[1] + X[2]) / 3.0), cy = (int)floor((Y[0] + Y[1] + Y[2]) / 3.0);
        if (cx < 0 || cy < 0 || cx >= w || cy >= h) continue;
        if (fabs(X[1] - X[0]) > 1.01 || fabs(X[2] - X[0]) > 1.01 || fabs(Y[1] - Y[0]) > 1.01 || fabs(Y[2] - Y[0]) > 1.01)
            continue;      // one cell per quad only
        size_t ci = (size_t)cy * w + cx;
        double bu = ((U[1] - U[0]) * (Y[2] - Y[0]) - (U[2] - U[0]) * (Y[1] - Y[0])) / det,
               cu = ((X[1] - X[0]) * (U[2] - U[0]) - (X[2] - X[0]) * (U[1] - U[0])) / det;
        double bv = ((V[1] - V[0]) * (Y[2] - Y[0]) - (V[2] - V[0]) * (Y[1] - Y[0])) / det,
               cv = ((X[1] - X[0]) * (V[2] - V[0]) - (X[2] - X[0]) * (V[1] - V[0])) / det;
        double u00 = U[0] + bu * (cx - X[0]) + cu * (cy - Y[0]), v00 = V[0] + bv * (cx - X[0]) + cv * (cy - Y[0]);
        auto q = [](double t) {
            t = t < 0 ? 0 : (t > 1 ? 1 : t);
            return (uint16_t)lround(t * 65535.0);
        };
        uint32_t tk = ((uint32_t)(q(u00) >> 9) << 16) | (uint32_t)(q(v00) >> 9);
        freqCur[tk]++;
        if (gCellStamp[ci] ==
            gCaptureId) {      // a second layer on this tile: keep whichever art is more common (the floor, not a stain or shadow on it)
            auto fa = freqPrev.find(frameKey[ci]), fb = freqPrev.find(tk);
            int na = fa == freqPrev.end() ? 0 : fa->second, nb = fb == freqPrev.end() ? 0 : fb->second;
            if (nb <= na + na / 4) continue;      // only a clearly more common tile takes a cell over: no flip-flopping
        }
        frameKey[ci] = tk;
        gCellStamp[ci] = gCaptureId;
        if (cellKey[ci] != 0 && tk != cellKey[ci]) {      // different art from what the tile shows: wait until it wins 3 captures in a row
            if (pendKey[ci] == tk && pendCap[ci] == gCaptureId) continue;
            if (pendKey[ci] == tk && pendCap[ci] + 1 == gCaptureId)
                pendN[ci]++;
            else {
                pendKey[ci] = tk;
                pendN[ci] = 1;
            }
            pendCap[ci] = gCaptureId;
            if (pendN[ci] < 3) continue;
        }
        if (cellKey[ci] != 0 && tk != cellKey[ci]) {
            static int cl = 0;
            if (cl < 60) {
                cl++;
                auto fo = freqPrev.find(cellKey[ci]), fn = freqPrev.find(tk);
                Log("FLOORSWAP cell (%d,%d) buffer %p art (%.3f,%.3f) -> (%.3f,%.3f) counts %d -> %d", cx, cy, vp,
                    (cellKey[ci] >> 16) * 512 / 65535.0, (cellKey[ci] & 0xffff) * 512 / 65535.0, u00, v00, fo == freqPrev.end() ? 0 : fo->second,
                    fn == freqPrev.end() ? 0 : fn->second);
            }
        }
        pendN[ci] = 0;
        cellKey[ci] = tk;
        uint16_t* A = &gCellA[ci * 4];
        uint16_t* B = &gCellB[ci * 4];
        uint16_t na[4] = {q(u00), q(v00), q(u00 + bu), q(v00 + bv)}, nb[4] = {q(u00 + cu), q(v00 + cv), 65535, 0};
        if (memcmp(A, na, sizeof na) || memcmp(B, nb, sizeof nb)) {
            memcpy(A, na, sizeof na);
            memcpy(B, nb, sizeof nb);
            gCellDirty = true;
        }
        gCellStamp[ci] = gCaptureId;
        stored++;
    }
    static int lines = 0;
    if (lines < 3 && stored > 0) {
        lines++;
        Log("FLOORCAP %d cells mapped from a terrain draw of %d vertices", stored, (int)count);
    }
}
static void DrawOwnFloor() {
    static bool failed = false, ready = false;
    static GLuint prog = 0, matTex = 0;
    static int matW = 0, matH = 0;
    static double nextUpload = 0;
    static PFN_CreateShader glCreateShader_ = nullptr;
    static PFN_ShaderSrc glShaderSource_ = nullptr;
    static PFN_CompileShader glCompileShader_ = nullptr;
    static PFN_CreateProgram glCreateProgram_ = nullptr;
    static PFN_AttachShader glAttachShader_ = nullptr;
    static PFN_LinkProgram glLinkProgram_ = nullptr;
    static PFN_GetShaderiv glGetShaderiv_ = nullptr;
    static PFN_GetInfoLog glGetShaderInfoLog_ = nullptr, glGetProgramInfoLog_ = nullptr;
    static PFN_GetProgramiv glGetProgramiv_ = nullptr;
    if (failed || !gCfg.ownFloor || !gCam.active) return;
    LoadGlFuncs();
    if (!pGetUniformLocation || !pUniform1i || !pActiveTexture || !oWglGetProcAddress) return;
    PFN_UseProgram use = oUseProgram ? oUseProgram : oUseProgramObjectARB;
    if (!use) return;
    uint8_t* app = App();
    if (!app) return;
    uint8_t* world = *(uint8_t**)(app + 0x198);
    if (!Readable(world, 0x48f0)) return;
    int w = *(int*)(world + 0x60), h = *(int*)(world + 0x64);
    uint8_t* cells = *(uint8_t**)(world + 0x48e8);
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || !Readable(cells, (size_t)w * h * 0xe0)) return;
    if (!ready) {
        auto get = [](const char* n) { return oWglGetProcAddress(n); };
        glCreateShader_ = (PFN_CreateShader)get("glCreateShader");
        glShaderSource_ = (PFN_ShaderSrc)get("glShaderSource");
        glCompileShader_ = (PFN_CompileShader)get("glCompileShader");
        glCreateProgram_ = (PFN_CreateProgram)get("glCreateProgram");
        glAttachShader_ = (PFN_AttachShader)get("glAttachShader");
        glLinkProgram_ = (PFN_LinkProgram)get("glLinkProgram");
        glGetShaderiv_ = (PFN_GetShaderiv)get("glGetShaderiv");
        glGetShaderInfoLog_ = (PFN_GetInfoLog)get("glGetShaderInfoLog");
        glGetProgramiv_ = (PFN_GetProgramiv)get("glGetProgramiv");
        glGetProgramInfoLog_ = (PFN_GetInfoLog)get("glGetProgramInfoLog");
        if (!glCreateShader_ || !glShaderSource_ || !glCompileShader_ || !glCreateProgram_ || !glAttachShader_ || !glLinkProgram_) {
            Log("Own floor: GL shader functions missing");
            failed = true;
            return;
        }
        GLuint vs = glCreateShader_(0x8B31), fs = glCreateShader_(0x8B30);
        GLint ok = 0;
        char logb[1024];
        glShaderSource_(vs, 1, &kFloorVS, nullptr);
        glCompileShader_(vs);
        if (glGetShaderiv_) glGetShaderiv_(vs, 0x8B81, &ok);
        if (!ok) {
            if (glGetShaderInfoLog_) {
                glGetShaderInfoLog_(vs, sizeof logb, nullptr, logb);
                Log("Own floor VS error: %s", logb);
            }
            failed = true;
            return;
        }
        glShaderSource_(fs, 1, &kFloorFS, nullptr);
        glCompileShader_(fs);
        ok = 0;
        if (glGetShaderiv_) glGetShaderiv_(fs, 0x8B81, &ok);
        if (!ok) {
            if (glGetShaderInfoLog_) {
                glGetShaderInfoLog_(fs, sizeof logb, nullptr, logb);
                Log("Own floor FS error: %s", logb);
            }
            failed = true;
            return;
        }
        prog = glCreateProgram_();
        glAttachShader_(prog, vs);
        glAttachShader_(prog, fs);
        glLinkProgram_(prog);
        ok = 0;
        if (glGetProgramiv_) glGetProgramiv_(prog, 0x8B82, &ok);
        if (!ok) {
            if (glGetProgramInfoLog_) {
                glGetProgramInfoLog_(prog, sizeof logb, nullptr, logb);
                Log("Own floor link error: %s", logb);
            }
            failed = true;
            return;
        }
        glGenTextures(1, &matTex);
        ready = true;
        Log("Own floor: shader ready (program %u)", prog);
    }
    GLint prevProg = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    GLint prevActive = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActive);
    // the game's lightmap sits on whatever unit its own terrain program uses: ask that program
    GLint lmUnit = -1;
    if (pGetUniformiv) {      // any of the game's terrain programs: they all read the lightmap from the same unit
        std::vector<GLuint> cand;
        if (prevProg) cand.push_back((GLuint)prevProg);
        for (GLuint p : gPatchedPrograms) cand.push_back(p);
        for (GLuint p : cand) {
            GLint loc = pGetUniformLocation(p, "lightmap");
            GLint u = -1;
            if (loc >= 0) pGetUniformiv(p, loc, &u);
            if (loc >= 0 && u > 0) {
                lmUnit = u;
                break;
            }
        }
        static int once = 0;
        if (once++ == 0) Log("Own floor: lightmap unit %d (from %d candidate programs)", (int)lmUnit, (int)cand.size());
    }
    NightTick(lmUnit);
    // material ids -> byte texture on unit 6 (re-read once a second)
    pActiveTexture(GL_TEXTURE0 + 6);
    GLint prevTex6 = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex6);
    glBindTexture(GL_TEXTURE_2D, matTex);
    double now = NowSec();
    if (w != matW || h != matH || now >= nextUpload) {
        nextUpload = now + 1.0;
        matW = w;
        matH = h;
        gMatBuf.resize((size_t)w * h);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int v = *(const int32_t*)(cells + ((size_t)y * w + x) * 0xe0 + 0x18);
                gMatBuf[(size_t)y * w + x] = IsWallMat(v) ? 255 : (unsigned char)(v < 0 ? 0 : (v > 254 ? 254 : v));
            }
        GLint pa = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &pa);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, gMatBuf.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, pa);
    }
    // the per-cell tileset mapping and the tileset itself, on units 9-11
    GLuint& cellTexA = gCellTexA;
    GLuint& cellTexB = gCellTexB;
    bool haveCells = gCfg.floorFromTileset && gTilesetTex && gCellW == w && gCellH == h && !gCellA.empty();
    GLint prevT9 = 0, prevT10 = 0, prevT11 = 0;
    if (haveCells) {
        if (!cellTexA) {
            glGenTextures(1, &cellTexA);
            glGenTextures(1, &cellTexB);
            gCellDirty = true;
        }
        GLint pa2 = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &pa2);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
        const GLuint tx[2] = {cellTexA, cellTexB};
        const std::vector<uint16_t>* bufs[2] = {&gCellA, &gCellB};
        GLint* prevs[2] = {&prevT9, &prevT10};
        for (int k = 0; k < 2; k++) {
            pActiveTexture(GL_TEXTURE0 + 9 + k);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, prevs[k]);
            glBindTexture(GL_TEXTURE_2D, tx[k]);
            if (gCellDirty) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
                glTexImage2D(GL_TEXTURE_2D, 0, 0x805B /* GL_RGBA16 */, w, h, 0, GL_RGBA, GL_UNSIGNED_SHORT, bufs[k]->data());
            }
        }
        gCellDirty = false;
        glPixelStorei(GL_UNPACK_ALIGNMENT, pa2);
        pActiveTexture(GL_TEXTURE0 + 11);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevT11);
        glBindTexture(GL_TEXTURE_2D, gTilesetTex);
        pActiveTexture(GL_TEXTURE0 + 6);
    }
    use(prog);
    pUniform1i(pGetUniformLocation(prog, "isoCellA"), 9);
    pUniform1i(pGetUniformLocation(prog, "isoCellB"), 10);
    pUniform1i(pGetUniformLocation(prog, "isoTiles"), 11);
    if (pUniform1f) {
        pUniform1f(pGetUniformLocation(prog, "haveCells"), haveCells ? 1.0f : 0.0f);
        pUniform1f(pGetUniformLocation(prog, "tileSoft"), gCfg.tileSoft);
        pUniform1f(pGetUniformLocation(prog, "tileFlat"), gCfg.tileFlat);
    }
    pUniform1i(pGetUniformLocation(prog, "isoMat"), 6);
    pUniform1i(pGetUniformLocation(prog, "lightmap"), lmUnit >= 0 ? lmUnit : 0);
    if (pUniform2f) pUniform2f(pGetUniformLocation(prog, "worldSize"), (float)w, (float)h);
    if (pUniform1f) pUniform1f(pGetUniformLocation(prog, "haveLight"), lmUnit >= 0 ? 1.0f : 0.0f);
    if (pUniform1f) pUniform1f(pGetUniformLocation(prog, "floorBright"), gCfg.floorBright);
    if (pUniform1f) pUniform1f(pGetUniformLocation(prog, "isoWarm"), gCfg.warm);
    GLint prevT12 = 0;      // the room map on unit 12, for the cells' lights
    pActiveTexture(GL_TEXTURE0 + 12); glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevT12); glBindTexture(GL_TEXTURE_2D, gRoomTex); pActiveTexture(GL_TEXTURE0 + 6);
    pUniform1i(pGetUniformLocation(prog, "isoRooms"), 12);
    if (pUniform1f) pUniform1f(pGetUniformLocation(prog, "isoCellsDark"), gRoomTex ? gCellsDark : 0.0f);
    pUniform1i(pGetUniformLocation(prog, "isoBulbs"), 14);
    if (pUniform1f) {
        pUniform1f(pGetUniformLocation(prog, "isoNight"), gNight * gCfg.nightDark);
        pUniform1f(pGetUniformLocation(prog, "isoBulbOn"), (gCfg.bulbPools && gBulbReady) ? 1.0f : 0.0f);
    }
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    float R = 100.0f, cx = gCam.T[0], cy = gCam.T[1];
    float x0 = cx - R < 0 ? 0 : cx - R, x1 = cx + R > w ? (float)w : cx + R, y0 = cy - R < 0 ? 0 : cy - R, y1 = cy + R > h ? (float)h : cy + R;
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glVertex3f(x0, y0, 0);
    glVertex3f(x1, y0, 0);
    glVertex3f(x1, y1, 0);
    glVertex3f(x0, y1, 0);
    glEnd();
    glPopAttrib();
    use((GLuint)prevProg);
    GLCHK("before queued items"); { double _q0 = gPerfOn ? PerfNow() : 0; DrawQueuedItems(); if (gPerfOn) gPerf.t[5] += PerfNow() - _q0; } GLCHK("DrawQueuedItems");      // loose items stand on this floor: after it, so it cannot paint over them
    if (haveCells) {
        pActiveTexture(GL_TEXTURE0 + 9);
        glBindTexture(GL_TEXTURE_2D, (GLuint)prevT9);
        pActiveTexture(GL_TEXTURE0 + 10);
        glBindTexture(GL_TEXTURE_2D, (GLuint)prevT10);
        pActiveTexture(GL_TEXTURE0 + 11);
        glBindTexture(GL_TEXTURE_2D, (GLuint)prevT11);
        pActiveTexture(GL_TEXTURE0 + 6);
    }
    pActiveTexture(GL_TEXTURE0 + 12); glBindTexture(GL_TEXTURE_2D, (GLuint)prevT12); pActiveTexture(GL_TEXTURE0 + 6);
    glBindTexture(GL_TEXTURE_2D, (GLuint)prevTex6);
    pActiveTexture((GLenum)prevActive);
    gFloorDrawsFrame++;
    static int logged = 0;
    if (logged++ == 0) Log("Own floor: first pass drawn (%dx%d cells, lightmap unit %d)", w, h, (int)lmUnit);
}
