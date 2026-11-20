// parts/05_walls.inc - indoor map, cell / material dumps, wall pieces. Part of main.cpp (one translation unit, included in order): it uses everything defined in the parts before it.
// ---- Lights off in cells during the Sleep regime (visual only: the game's own lights and behaviour are unchanged) ----
// SleepNow: the prisoners' regime says Sleep this hour. RoomIsCell: a room id (cell grid +0x50) is a Cell / Dormitory.
// Both read the live game (world time and regime, the room list); the offsets come from dll\tools\pamem.py. Until found they answer false.
// Found in the running game with dll\tools\pamem.py (6 Nov 2026):
//   world (app+0x198) +0x80   TimeIndex, double, game minutes (hour = TimeIndex mod 1440 / 60)
//   world +0x5E8              table of int[24] schedules, one per prisoner category (1 MinSec, 2 MedSec ...); activity 1 = Sleep,
//                             a schedule that is all one value is a category with no regime of its own
//   world +0x44A0             Room* per room index; room +0x34 = index, +0x38 = RoomType (1 Cell, 5 Dormitory), +0x40 its name
//   cell grid +0x54           the tile's room index (+0x50 is the room's unique id)
static uint8_t* WorldPtr() {
    uint8_t* app = App();
    if (!app) return nullptr;
    uint8_t* w = *(uint8_t**)(app + 0x198);
    return Readable(w, 0x48f0) ? w : nullptr;
}
static bool SleepNow() {
    static double nextCheck = 0; static bool cached = false;
    double now = NowSec();
    if (now < nextCheck) return cached;
    nextCheck = now + 1.0;
    cached = false;
    uint8_t* w = WorldPtr();
    if (!w) return false;
    double t = *(double*)(w + 0x80);
    if (!(t > 0.0 && t < 1e9)) return false;
    int hour = (int)(fmod(t, 1440.0) / 60.0);
    if (hour < 0 || hour > 23) return false;
    int** tbl = *(int***)(w + 0x5e8);
    if (!Readable(tbl, 8 * 8)) return false;
    int sleep = 0, other = 0;
    for (int cat = 1; cat < 8; cat++) {
        int* s = tbl[cat];
        if (!Readable(s, 24 * 4)) continue;
        bool flat = true;
        for (int h = 1; h < 24; h++) if (s[h] != s[0]) { flat = false; break; }
        if (flat) continue;      // no regime of its own
        if (s[hour] == 1) sleep++; else other++;
    }
    cached = sleep > 0 && sleep >= other;
    static int lastLogged = -1;
    if ((int)cached != lastLogged) { lastLogged = (int)cached; Log("CELLLIGHTS hour %d: %s (%d regimes sleep, %d not)", hour, cached ? "Sleep - cell lights off" : "lights on", sleep, other); }
    return cached;
}
static bool RoomIsCell(int roomIndex) {
    if (roomIndex < 0 || roomIndex > 100000) return false;
    uint8_t* w = WorldPtr();
    if (!w) return false;
    uint8_t** rooms = *(uint8_t***)(w + 0x44a0);
    if (!Readable(rooms + roomIndex, 8)) return false;
    uint8_t* r = rooms[roomIndex];
    if (!Readable(r, 0x40) || *(int*)(r + 0x34) != roomIndex) return false;
    int type = *(int*)(r + 0x38);
    return type == 1 || type == 5;      // Cell, Dormitory: where prisoners sleep
}static void NoteOutdoors() {
    static int diag = 0;
    if (false && diag < 40 && gCurProg) {
        diag++;
        Log("draw with program %u (patched=%d)", gCurProg, (int)gPatchedPrograms.count(gCurProg));
    }
    if (!gCurProg || !gPatchedPrograms.count(gCurProg) || !pGetUniformLocation || !pGetUniformiv) return;
    auto it = gOutdoorsUnit.find(gCurProg);
    if (it == gOutdoorsUnit.end()) {
        GLint loc = pGetUniformLocation(gCurProg, "outdoors"), unit = -1;
        if (loc >= 0) {
            pGetUniformiv(gCurProg, loc, &unit);
            if (unit <= 0) unit = -2;
        }      // -2: retry later
        if (unit == -2) return;
        it = gOutdoorsUnit.emplace(gCurProg, unit).first;
        Log("Program %u 'outdoors' sampler on unit %d", gCurProg, unit);
    }
    if (it->second >= 0 && it->second < 32 && gBound[it->second]) {
        {
            static GLuint lastLogged = 0;
            if (lastLogged != gBound[it->second]) {
                lastLogged = gBound[it->second];
                Log("Indoor/outdoor map texture = %u", lastLogged);
            }
        }      // gOutdoorsTex is cleared elsewhere: log real changes only
        gOutdoorsTex = gBound[it->second];
    }
}
// Floor-rebuild groundwork: dump the first vertices / texcoords of the game's big terrain batch
// and a histogram of the cell material ids, so the game's tiles can be mapped to cells.
typedef void(APIENTRY* PFN_GetBufferSubData)(GLenum, ptrdiff_t, ptrdiff_t, void*);
static void DumpArrayHead(const char* nm, GLenum pn, GLenum sn, GLenum tn, GLenum stn, GLint first) {
    if (!gCfg.diagLog) return;      // one-off diagnostic (Settings: diagLog)
    void* p = nullptr;
    glGetPointerv(pn, &p);
    GLint sz = 0, ty = 0, st = 0, vbo = 0;
    glGetIntegerv(sn, &sz);
    glGetIntegerv(tn, &ty);
    glGetIntegerv(stn, &st);
    glGetIntegerv(0x8894, &vbo);
    int stride = st ? st : sz * 4;
    if (stride <= 0 || stride > 128) {
        Log("CELLDUMP %s size=%d type=0x%x stride=%d (skipped)", nm, sz, (unsigned)ty, st);
        return;
    }
    Log("CELLDUMP %s size=%d type=0x%x stride=%d vbo=%d ptr=%p", nm, sz, (unsigned)ty, st, vbo, p);
    unsigned char tmp[1024];
    int bytes = stride * 12;
    if (bytes > (int)sizeof tmp) bytes = sizeof tmp;
    bool ok = false;
    if (vbo) {
        auto gbs = (PFN_GetBufferSubData)(oWglGetProcAddress ? oWglGetProcAddress("glGetBufferSubData") : nullptr);
        if (gbs) {
            gbs(0x8892, (ptrdiff_t)p + (ptrdiff_t)first * stride, bytes, tmp);
            ok = true;
        }
    } else if (p && Readable((uint8_t*)p + (size_t)first * stride, bytes)) {
        memcpy(tmp, (uint8_t*)p + (size_t)first * stride, bytes);
        ok = true;
    }
    if (!ok || ty != 0x1406) {
        Log("CELLDUMP %s: not readable as float", nm);
        return;
    }
    for (int v = 0; v < 12; v++) {
        const float* f = (const float*)(tmp + v * stride);
        char line[200];
        int n = 0;
        for (int k = 0; k < stride / 4 && k < 8; k++) n += snprintf(line + n, sizeof line - n, " %.4f", f[k]);
        Log("CELLDUMP %s[%d]%s", nm, v, line);
    }
}
// Correlate every terrain triangle with the material of the cell under it: tells which tileset tiles
// each material (walls included) is drawn with.
static void MapFloorTiles(GLint first, GLsizei count) {
    void* vp = nullptr;
    void* tp = nullptr;
    glGetPointerv(0x808E, &vp);
    glGetPointerv(0x8092, &tp);
    GLint vsz = 0, tsz = 0, vst = 0, tst = 0;
    glGetIntegerv(0x807A, &vsz);
    glGetIntegerv(0x8088, &tsz);
    glGetIntegerv(0x807C, &vst);
    glGetIntegerv(0x808A, &tst);
    int vs = vst ? vst : vsz * 4, ts = tst ? tst : tsz * 4;
    uint8_t* app = App();
    if (!app || !vp || !tp || vsz < 2 || tsz < 2) {
        Log("FLOORMAP: no client arrays");
        return;
    }
    uint8_t* world = *(uint8_t**)(app + 0x198);
    if (!Readable(world, 0x48f0)) return;
    int w = *(int*)(world + 0x60), h = *(int*)(world + 0x64);
    uint8_t* cells = *(uint8_t**)(world + 0x48e8);
    if (w <= 0 || h <= 0 || !Readable(cells, (size_t)w * h * 0xe0)) return;
    if (!Readable((uint8_t*)vp + (size_t)first * vs, (size_t)count * vs) || !Readable((uint8_t*)tp + (size_t)first * ts, (size_t)count * ts)) {
        Log("FLOORMAP: arrays not readable");
        return;
    }
    std::map<std::tuple<int, int, int>, int> agg;
    std::map<std::tuple<int, int, int>, std::pair<float, float>> sample;
    for (int i = 0; i + 2 < count; i += 3) {
        float cx = 0, cy = 0, u = 0, v = 0;
        for (int k = 0; k < 3; k++) {
            const float* P = (const float*)((uint8_t*)vp + (size_t)(first + i + k) * vs);
            const float* T = (const float*)((uint8_t*)tp + (size_t)(first + i + k) * ts);
            cx += P[0] / 3;
            cy += P[1] / 3;
            u += T[0] / 3;
            v += T[1] / 3;
        }
        int x = (int)floorf(cx), y = (int)floorf(cy);
        if (x < 0 || y < 0 || x >= w || y >= h) continue;
        int mat = *(const int32_t*)(cells + ((size_t)y * w + x) * 0xe0 + 0x18);
        auto key = std::make_tuple(mat, (int)(u * 128), (int)(v * 128));
        if (!agg[key]++) sample[key] = {u, v};
    }
    int lines = 0;
    for (auto& kv : agg) {
        if (lines++ > 400) break;
        auto k = kv.first;
        Log("FLOORMAP mat=%d tile128=(%d,%d) n=%d u=%.4f v=%.4f", std::get<0>(k), std::get<1>(k), std::get<2>(k), kv.second, sample[k].first,
            sample[k].second);
    }
    Log("FLOORMAP %d distinct (mat,tile) pairs from %d triangles", (int)agg.size(), count / 3);
}
static void DumpCellLayer(GLint first, GLsizei count) {
    if (!gCfg.diagLog) return;      // one-off diagnostic (Settings: diagLog)
    static int done = 0;
    if (done >= 2) return;
    done++;
    if (done == 1 && count > 2000) MapFloorTiles(first, count);
    Log("CELLDUMP prog=%u tex0=%u first=%d count=%d", gCurProg, gBound[0], (int)first, (int)count);
    DumpArrayHead("vertex", 0x808E, 0x807A, 0x807B, 0x807C, first);
    DumpArrayHead("texcoord", 0x8092, 0x8088, 0x8089, 0x808A, first);
}
static void DumpMaterialHistogram() {
    if (!gCfg.diagLog) return;      // one-off diagnostic (Settings: diagLog)
    static bool done = false;
    if (done) return;
    uint8_t* app = App();
    if (!app) return;
    uint8_t* world = *(uint8_t**)(app + 0x198);
    if (!Readable(world, 0x48f0)) return;
    int w = *(int*)(world + 0x60), h = *(int*)(world + 0x64);
    uint8_t* cells = *(uint8_t**)(world + 0x48e8);
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || !Readable(cells, (size_t)w * h * 0xe0)) return;
    done = true;
    std::map<int, int> hist, firstAt;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int m = *(const int32_t*)(cells + ((size_t)y * w + x) * 0xe0 + 0x18);
            if (!hist[m]++) firstAt[m] = y * w + x;
        }
    Log("MATDUMP world %dx%d, %d distinct values at cell+0x18", w, h, (int)hist.size());
    for (auto& kv : hist) Log("MATDUMP mat=%d count=%d first at x=%d y=%d", kv.first, kv.second, firstAt[kv.first] % w, firstAt[kv.first] / w);
    for (int y = 0; y < 3 && y < h; y++)
        for (int x = 0; x < 6 && x < w; x++) {
            const uint8_t* c = cells + ((size_t)y * w + x) * 0xe0;
            Log("MATDUMP cell(%d,%d) +0x18=%d +0x1c=%d +0x20=%d +0x24=%d +0x28=%d", x, y, *(const int32_t*)(c + 0x18), *(const int32_t*)(c + 0x1c),
                *(const int32_t*)(c + 0x20), *(const int32_t*)(c + 0x24), *(const int32_t*)(c + 0x28));
        }
}

// Model-space extent of the first few wall pieces, to see where the wall mesh sits relative to the floor.
// Every wall piece is drawn on its own: find its lowest model-space Y and hand it to the shader, so a cut piece
// shrinks towards ITS base (pieces differ: full walls, single-tile posts, door frames...). Distinct extents are logged.
// The game builds a 3D piece only for wall cells that show a face; a one-tile wall squeezed between two doors
// shows none, so it vanishes. We remember which cells were drawn last frame and re-draw the missing concrete /
// brick / perimeter wall cells ourselves, using a full-cube piece (36 vertices) as the template.
static bool gObjShopFront = false;
static int gObjSurface = 0;      // the object drawn is furniture things are put on: 1 table / desk top, 2 sofa / bench seat
static unsigned gPersonNote = 0;      // gNoteSerial of the last person (not animal) noted; its body may come after other objects' notes
static bool gPersonPrisoner = false;
static uint32_t gPersonSkin = 0;
static bool gObjAnimal = false;      // the person-type object is an animal (dog): no hands
static bool gObjRot90 = false;      // turned a quarter round about its upright axis (water boiler)
// Early cull (1.9.83): an object whose position (read from the object at the note, path-checked against where it is really drawn) lies
// off screen is told "no model" in the draw-path stub, so the game skips its whole draw, not just the GL call.
extern "C" int paiso_path;
static bool gNoteSkip = false;      // this note's object is off screen: paiso_def_to_id returns -3
static bool gNotePosOk = false; static float gNotePos[2] = {0, 0}; static int gNotePath = 0;
static unsigned gEarlyAgree[3] = {0, 0, 0}, gEarlyDisagree[3] = {0, 0, 0};
static bool gEarlyOn[3] = {false, false, false};
static unsigned gEarlyCulled = 0, gCalNote = 0;
// 1.9.84: where the position is is found, not assumed (1.9.83's rsi + 0x48 never matched): per draw path, every register x offset
// 0..0x1FC whose float pair matched the drawn model origin gets a vote; the winner is then checked as before
static uint64_t gNoteRegs[16];
static int gPosReg[3] = {-1, -1, -1}, gPosOff[3] = {0, 0, 0};
static unsigned gPosVotes[3][16][128];
static unsigned gPosSamples[3] = {0, 0, 0};
static bool gPersonPosOk = false; static float gPersonPos[2] = {0, 0};      // the last person's position at its note (1.9.87: hands only on a body drawn there)
extern "C" int paiso_tree_note;      // dlc_models.S: set by paiso_note while a tree is being drawn
static float gWallWorldH = 0.0f; static int gWallHFrame = -1;      // a full wall piece's height in world units (measured in the wall pass)
// 1.9.106 (user: "pan of cooked ingredients :(" - the flat extruded sprite): each cooked-ingredients object's spot, drawn as a real pot (07_draw)
static float gWextLast[4] = {-1, -1, -1, -1}; static GLuint gWextProg = 0; static int gWextFrame = -1;      // 1.9.121: last isoWallExt sent
static long long gNoteSpriteId = -1;      // 1.9.111: the sprite index the game passed with the last note (0 = the accountant: no sprite of its own)
static bool gPersonHands = false;      // 1.9.111: the last person noted is one that gets hands (prisoner, guard, staff, worker)
struct EarlyStat { uint16_t ok = 0, bad = 0; };      // 1.9.107: per object type: drawn where its note said / elsewhere
static std::unordered_map<const void*, EarlyStat> gEarlyStat;
static EarlyStat* gNoteES = nullptr;
static std::vector<long long> gCaps;      // 1.9.124: wall edges that meet a fence (cell key * 4 + side), closed by WallPiece
// the cell's 2D art is in the game's fence sets (the same test as the wall shader's isoFenceV: where the art sits in the tileset)
static bool IsFenceCell(int x, int y) {
    if (gCellA.empty() || x < 0 || y < 0 || x >= gCellW || y >= gCellH) return false;
    const size_t ci = ((size_t)y * gCellW + x) * 4;
    if (gCellB[ci + 2] < 32768) return false;      // no 2D art mapped for this cell
    const double u = gCellA[ci] / 65535.0, v = gCellA[ci + 1] / 65535.0;
    return u < 0.07 && v > 0.695 && v < 0.765;
}
static std::unordered_map<int, int> gDoorTiles;      // 1.9.106: tile (y*65536+x) of each door / metal detector -> wall frame last drawn
// 1.9.122: the tiles of something standing in the wall line (door, detector, booth) at world (x, y): +-0.3 tile covers a 2-tile door's centre
static void MarkDoorTiles(double x, double y) {
    extern int gWallFramesSeenExt;
    if (gDoorTiles.size() > 20000) gDoorTiles.clear();
    for (int a = -1; a <= 1; a += 2)
        for (int b = -1; b <= 1; b += 2) gDoorTiles[(int)floor(y + b * 0.3) * 65536 + (int)floor(x + a * 0.3)] = gWallFramesSeenExt;
}
struct PotQ { float x, y, h; };
static std::vector<PotQ> gPotCur, gPotPrev;
static int gPotFrame = -1;
static bool gObjStack = false;      // 1.9.103: a "stack" object (wood, logs, plates...): stock models ~2 tiles across, shrunk to its tile
static bool gObjDetector = false;      // a metal detector (or door frame): drawn as thick as the walls, like doors (DoorDepthScale)
static bool gObjStructural = false;      // a gate, fence, bars, barrier or window: never a person's body (equipment / hands are not put on it)
static bool gObjUtil = false;      // the object is a pipe / cable / valve (under the floor: not drawn while hideUtilities)
static bool gObjNoteTiny = false;      // an ordinary object that may be skipped when it is only a few pixels big on screen
static int gObjLight = 0;      // the object is a lamp: 1 ceiling light, 2 outdoor lamp, 3 floodlight, 4 wall light (its position feeds the light-pool map)
static bool gObjLift = false;      // a small thing (food tray, meal, bowl, stack) the game draws flat on the floor: lifted onto the furniture under it
static uint8_t* gServObj = nullptr;      // rsi at the serving table's note (its object, to find the slots holding its food)
static bool gObjServing = false;      // a canteen serving table is being drawn (its food comes and goes)
static std::unordered_set<int> gNoWallCur, gNoWallPrev, gModelTilesCur,
    gModelTilesPrev;      // gModelTiles: half-tiles where a 3D model was drawn      // tiles a shop front stands in: no wall is drawn there
static std::unordered_set<int> gThingTilesCur, gThingTilesPrev;      // half-tiles where a 3D model that is NOT furniture (a stack, a crate...) was drawn
static int gSwapCount = 0;
static bool gCaptureArmed = false;      // floor capture: armed at the swap so it covers a whole frame
static std::unordered_set<int> gDrawnPrev, gDrawnCur;
static struct DrawnInit {
    DrawnInit() {
        gDrawnPrev.reserve(16384);
        gDrawnCur.reserve(16384);
    }
} gDrawnInit;
static std::vector<int> gMissing;
static int gWallFrames = 0, gMissW = 0;
static bool gExtraDone = false;
// 1.9.130 (user: "the fps is still terrible"; ~10,500 wall pieces a frame, each its own draw call): the wall batch. One normal wall pass
// is recorded (every piece's vertices, its placement in the world and its per-piece shader values); later frames skip the game's wall
// loop and draw the whole record in one call per texture. Rebuilt when walls, doors, shop fronts or wall settings change (12_rooms WbDecide).
static bool gWbCapture = false;      // this wall pass records every piece (and culls none)
static bool gWbWide = false;      // WallFrameBegin: fillers and fence caps for the whole map, not 60 tiles round the camera
static int gWbOther = 0;      // draws in a recorded pass that are not glDrawArrays (cannot be batched)
static float gPcBase = -0.989f, gPcLift = 0.0f, gPcThick[2] = {0, 0}, gPcExt[4] = {0, 0, 0, 0};      // the last piece's shader values
static std::unordered_set<int> gNoWallEver;      // every tile a shop front has been seen in (gNoWallPrev only holds the ones on screen)
// Which cell materials are walls differs from save to save (the ids follow each save's own material list), so learn it: a material whose cells nearly all get a wall piece is a wall.
static bool gWallMat[512] = {false};
static bool gWallMatKnown = false;
static inline bool IsWallMat(int m) { return m >= 0 && m < 512 && gWallMat[m]; }
static void LearnWallMats(const uint8_t* cells, int w, int h) {
    static int cnt[512], drawn[512];
    memset(cnt, 0, sizeof cnt);
    memset(drawn, 0, sizeof drawn);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int mt = *(const int32_t*)(cells + ((size_t)y * w + x) * 0xe0 + 0x18);
            if (mt < 0 || mt >= 512) continue;
            cnt[mt]++;
            if (gDrawnPrev.count(y * 65536 + x)) drawn[mt]++;
        }
    std::string s;
    bool any = false;
    for (int i = 0; i < 512; i++) {
        bool wl = cnt[i] >= 12 && drawn[i] >= cnt[i] * 0.85;
        if (wl != gWallMat[i]) {
            gWallMat[i] = wl;
            any = true;
        }
        if (wl) {
            char b[48];
            snprintf(b, sizeof b, " %d (%d/%d)", i, drawn[i], cnt[i]);
            s += b;
        }
    }
    if (any || !gWallMatKnown) Log("WALLMATS learned:%s", s.empty() ? " none" : s.c_str());
    {      // which 2D art each wall material is drawn with (tileset uv of the tile's corner, most common), so fence art can be told from wall art
        static int artLines = 0;
        if (artLines < 60 && !gCellA.empty() && gCellW == w && gCellH == h) {
            std::map<int, std::map<long long, int>> hist;
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    int mt = *(const int32_t*)(cells + ((size_t)y * w + x) * 0xe0 + 0x18);
                    if (mt < 0 || mt >= 512 || !gWallMat[mt]) continue;
                    size_t ci = (size_t)y * w + x;
                    if (gCellB[ci * 4 + 2] == 0) continue;
                    long long k = ((long long)(gCellA[ci * 4] >> 6) << 20) | (gCellA[ci * 4 + 1] >> 6);
                    hist[mt][k]++;
                }
            for (auto& hm : hist) {
                std::vector<std::pair<int, long long>> v;
                for (auto& kv : hm.second) v.push_back({kv.second, kv.first});
                std::sort(v.rbegin(), v.rend());
                std::string t;
                for (size_t i = 0; i < v.size() && i < 4; i++) {
                    char b[64];
                    snprintf(b, sizeof b, " (%.4f,%.4f)x%d", (double)((v[i].second >> 20) << 6) / 65535.0,
                             (double)((v[i].second & 0xFFFFF) << 6) / 65535.0, v[i].first);
                    t += b;
                }
                if (artLines < 60) {
                    artLines++;
                    Log("WALLART mat=%d%s", hm.first, t.c_str());
                }
            }
        }
    }
    gWallMatKnown = true;
}
int gWallFramesSeenExt = 0;
static void WallFrameBegin() {
    if (gDrawnCur.size() > 20)
        gDrawnPrev.swap(gDrawnCur);      // keep the last good wall list: an empty or partial frame must not make snapped objects jump back
    if (!gNoWallCur.empty() || !gNoWallPrev.empty()) {
        gNoWallPrev.swap(gNoWallCur);
        gNoWallCur.clear();
    }
    gModelTilesPrev.swap(gModelTilesCur);
    gModelTilesCur.clear();
    gThingTilesPrev.swap(gThingTilesCur);
    gThingTilesCur.clear();
    gDrawnCur.clear();
    gWallFrames++;
    gWallFramesSeenExt = gWallFrames;
    gExtraDone = false;
    gMissing.clear();
    if (gWallFrames < 4 || gDrawnPrev.empty() || !gCam.active) return;
    uint8_t* app = App();
    if (!app) return;
    uint8_t* world = *(uint8_t**)(app + 0x198);
    if (!Readable(world, 0x48f0)) return;
    int w = *(int*)(world + 0x60), h = *(int*)(world + 0x64);
    uint8_t* cells = *(uint8_t**)(world + 0x48e8);
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || !Readable(cells, (size_t)w * h * 0xe0)) return;
    gMissW = w;
    {
        static int lastLearn = -1000;
        if (gWallFrames - lastLearn > 120) {
            lastLearn = gWallFrames;
            LearnWallMats(cells, w, h);
        }
    }
    int cx = (int)gCam.T[0], cy = (int)gCam.T[1], R = gWbWide ? 4096 : 60;
    for (int y = (cy - R < 0 ? 0 : cy - R); y < h && y <= cy + R; y++)
        for (int x = (cx - R < 0 ? 0 : cx - R); x < w && x <= cx + R; x++) {
            int mat = *(const int32_t*)(cells + ((size_t)y * w + x) * 0xe0 + 0x18);
            if (IsWallMat(mat) &&
                !gDrawnPrev.count(
                    y * 65536 +
                    x)) {      // only a gap in a wall line (walls on two opposite sides): a cell with no wall around it is something built in (kitchen sinks), not a missing wall
                // 1.9.108 (user: walls between visitor booths missing): a door or a secure visitor booth in the wall line counts as wall on that side
                auto occ = [&](int xx, int yy) {
                    int k = yy * 65536 + xx;
                    if (gDrawnPrev.count(k)) return true;
                    auto dt = gDoorTiles.find(k);
                    return dt != gDoorTiles.end() && gWallFramesSeenExt - dt->second <= (gWbWide ? 36000 : 30);      // recorded: doors off screen too
                };
                bool ew = occ(x - 1, y) && occ(x + 1, y), ns = occ(x, y - 1) && occ(x, y + 1);
                if (ew || ns) gMissing.push_back(y * 65536 + x);
            }
        }
    // 1.9.124 (wall end caps): a wall that runs into a fence is left open at that edge (the game builds the joint as one run, and the fence is
    // drawn thin). List those edges - cell key * 4 + side (0 = -x, 1 = +x, 2 = -y, 3 = +y) - every 8 wall frames; WallPiece closes them.
    static int capFrame = -100;
    if (gWallFrames - capFrame >= 8 || gWbWide) {
        capFrame = gWallFrames;
        gCaps.clear();
        for (int y = (cy - R < 1 ? 1 : cy - R); y < h - 1 && y <= cy + R; y++)
            for (int x = (cx - R < 1 ? 1 : cx - R); x < w - 1 && x <= cx + R; x++) {
                if (!gDrawnPrev.count(y * 65536 + x) || IsFenceCell(x, y)) continue;
                const int nx[4] = {x - 1, x + 1, x, x}, ny[4] = {y, y, y - 1, y + 1};
                for (int s = 0; s < 4; s++)
                    if (gDrawnPrev.count(ny[s] * 65536 + nx[s]) && IsFenceCell(nx[s], ny[s]) && gCaps.size() < 2000) gCaps.push_back((y * 65536 + x) * 4 + s);
            }
    }
}
// ---- 1.9.130 wall batch: recording, upload, drawing ----------------------------------------------------------------------------------------
struct WbVert { float p[3], n[3], t[2]; uint8_t c[4]; float idx; };      // 40 bytes: model-space vertex + the piece it belongs to
struct WbGroup { GLuint prog, tex; bool nocull; std::vector<WbVert> v; GLuint vbo; GLsizei n; };
struct WbLay {      // where a piece's normals, texcoords and colours are, relative to its vertex pointer (the game interleaves them)
    bool nOn, tOn, cOn;
    ptrdiff_t nOff, tOff, cOff;
    GLint nSt, nTy, tSt, tSz, tTy, cSt, cSz, cTy;
    float curN[3], curT[4], curC[4];
};
static struct WbState {
    int state = 0;      // 0 next pass normal (refreshes the wall list), 1 next pass recorded, 2 batch in use, 5 recording failed for this signature
    unsigned long long sig = 0, failSig = 0, sigSoft = 0, failSigSoft = 0;
    double builtAt = 0;
    std::vector<WbGroup> groups;
    std::vector<float> pieces;      // 8 RGBA texels a piece: model -> world rows 0..2, (base, lift, thick x, thick z), ext
    int nPieces = 0, texH = 0;
    GLuint pieceTex = 0, fbo = 0;
    bool haveState = false, bad = false, layOk = false, perPiece = false;
    const char* why = "";
    GLboolean dt, bl, cull, at, dm;
    GLint bs, bd, cullMode, front, afunc;
    GLfloat aref;
    GLuint units[9];
    WbLay lay;
    double capT0 = 0;
} gWb;
typedef void(APIENTRY* PFN_WbGenBuf)(GLsizei, GLuint*);
typedef void(APIENTRY* PFN_WbBindBuf)(GLenum, GLuint);
typedef void(APIENTRY* PFN_WbBufData)(GLenum, ptrdiff_t, const void*, GLenum);
typedef void(APIENTRY* PFN_WbVAP)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef void(APIENTRY* PFN_WbVAA)(GLuint);
typedef GLint(APIENTRY* PFN_WbAttLoc)(GLuint, const char*);
typedef void(APIENTRY* PFN_WbClientAct)(GLenum);
static struct WbGl {
    bool tried = false, ok = false;
    PFN_WbGenBuf gen = nullptr, del = nullptr;
    PFN_WbBindBuf bind = nullptr;
    PFN_WbBufData data = nullptr;
    PFN_WbVAP vap = nullptr;
    PFN_WbVAA enable = nullptr, disable = nullptr;
    PFN_WbAttLoc attLoc = nullptr;
    PFN_WbClientAct clientAct = nullptr;
} gWbGl;
static bool WbGlLoad() {
    if (gWbGl.tried) return gWbGl.ok;
    gWbGl.tried = true;
    if (!oWglGetProcAddress) return false;
    auto get = [](const char* a) { return (void*)oWglGetProcAddress(a); };
    gWbGl.gen = (PFN_WbGenBuf)get("glGenBuffers");
    gWbGl.del = (PFN_WbGenBuf)get("glDeleteBuffers");
    gWbGl.bind = (PFN_WbBindBuf)get("glBindBuffer");
    gWbGl.data = (PFN_WbBufData)get("glBufferData");
    gWbGl.vap = (PFN_WbVAP)get("glVertexAttribPointer");
    gWbGl.enable = (PFN_WbVAA)get("glEnableVertexAttribArray");
    gWbGl.disable = (PFN_WbVAA)get("glDisableVertexAttribArray");
    gWbGl.attLoc = (PFN_WbAttLoc)get("glGetAttribLocation");
    gWbGl.clientAct = (PFN_WbClientAct)get("glClientActiveTexture");
    gWbGl.ok = gWbGl.gen && gWbGl.del && gWbGl.bind && gWbGl.data && gWbGl.vap && gWbGl.enable && gWbGl.disable && gWbGl.attLoc && gWbGl.clientAct &&
               pGetUniformLocation && pUniform1i && pUniform2f && pActiveTexture && oBindTexture && (oUseProgram || oUseProgramObjectARB);
    if (!gWbGl.ok) Log("Wall batch: the driver lacks a function it needs - walls are drawn piece by piece");
    return gWbGl.ok;
}
static void WbFree() {
    for (WbGroup& g : gWb.groups)
        if (g.vbo && gWbGl.del) gWbGl.del(1, &g.vbo);
    gWb.groups.clear();
    gWb.pieces.clear();
    gWb.nPieces = 0;
}
// the arrays of the draw about to be made (glGets: only while recording)
static void WbQuery(WbLay& L, const uint8_t* vp) {
    void* p = nullptr;
    L.nOn = glIsEnabled(GL_NORMAL_ARRAY) != 0;
    L.nOff = 0; L.nSt = 0; L.nTy = GL_FLOAT;
    if (L.nOn) { glGetPointerv(GL_NORMAL_ARRAY_POINTER, &p); glGetIntegerv(0x807E, &L.nTy); glGetIntegerv(0x807F, &L.nSt); L.nOff = (const uint8_t*)p - vp; }
    GLint cat = GL_TEXTURE0;
    glGetIntegerv(0x84E1 /* CLIENT_ACTIVE_TEXTURE */, &cat);
    if (cat != GL_TEXTURE0) gWbGl.clientAct(GL_TEXTURE0);
    L.tOn = glIsEnabled(GL_TEXTURE_COORD_ARRAY) != 0;
    L.tOff = 0; L.tSt = 0; L.tSz = 2; L.tTy = GL_FLOAT;
    if (L.tOn) { glGetPointerv(0x8092, &p); glGetIntegerv(0x8088, &L.tSz); glGetIntegerv(0x8089, &L.tTy); glGetIntegerv(0x808A, &L.tSt); L.tOff = (const uint8_t*)p - vp; }
    if (cat != GL_TEXTURE0) gWbGl.clientAct((GLenum)cat);
    L.cOn = glIsEnabled(GL_COLOR_ARRAY) != 0;
    L.cOff = 0; L.cSt = 0; L.cSz = 4; L.cTy = GL_UNSIGNED_BYTE;
    if (L.cOn) { glGetPointerv(0x8090, &p); glGetIntegerv(0x8081, &L.cSz); glGetIntegerv(0x8082, &L.cTy); glGetIntegerv(0x8083, &L.cSt); L.cOff = (const uint8_t*)p - vp; }
    if (!L.nOn) glGetFloatv(0x0B02 /* CURRENT_NORMAL */, L.curN);
    if (!L.tOn) glGetFloatv(0x0B03 /* CURRENT_TEXTURE_COORDS */, L.curT);
    if (!L.cOn) glGetFloatv(GL_CURRENT_COLOR, L.curC);
}
static bool WbLaySame(const WbLay& a, const WbLay& b) {
    return a.nOn == b.nOn && a.tOn == b.tOn && a.cOn == b.cOn && (!a.nOn || (a.nOff == b.nOff && a.nSt == b.nSt && a.nTy == b.nTy)) &&
           (!a.tOn || (a.tOff == b.tOff && a.tSt == b.tSt && a.tSz == b.tSz && a.tTy == b.tTy)) &&
           (!a.cOn || (a.cOff == b.cOff && a.cSt == b.cSt && a.cSz == b.cSz && a.cTy == b.cTy));
}
static void WbBegin() {
    WbFree();
    gWb.haveState = false; gWb.bad = false; gWb.layOk = false; gWb.perPiece = false; gWb.why = "";
    gWbOther = 0;
    gWb.capT0 = NowSec();
    gWb.fbo = gCurFbo;
}
static void WbFail(const char* why) { if (!gWb.bad) { gWb.bad = true; gWb.why = why; } }
// record the draw about to be made with the current matrix and the last piece's shader values (nocull: drawn with face culling off)
static void WbRecord(GLenum mode, GLint first, GLsizei count, bool nocull) {
    if (!gWbCapture || gWb.bad || count <= 0) return;
    if (mode != GL_TRIANGLES) { WbFail("a wall piece is not drawn as triangles"); return; }
    const ArrState& AS = GetArr();
    if (AS.vbo || AS.ty != GL_FLOAT || AS.sz < 3 || !AS.vp) { WbFail("a wall piece's vertices are not a plain float array"); return; }
    const int vst = AS.st ? AS.st : AS.sz * 4;
    const uint8_t* vp = (const uint8_t*)AS.vp;
    if (!Readable(vp + (size_t)first * vst, (size_t)count * vst)) { WbFail("unreadable vertices"); return; }
    if (!gWb.haveState) {      // the render state of the game's wall pass, put back when the batch is drawn
        gWb.haveState = true;
        gWb.dt = glIsEnabled(GL_DEPTH_TEST); gWb.bl = glIsEnabled(GL_BLEND); gWb.cull = glIsEnabled(GL_CULL_FACE); gWb.at = glIsEnabled(GL_ALPHA_TEST);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &gWb.dm);
        glGetIntegerv(0x80C9, &gWb.bs); glGetIntegerv(0x80C8, &gWb.bd);
        glGetIntegerv(GL_CULL_FACE_MODE, &gWb.cullMode); glGetIntegerv(GL_FRONT_FACE, &gWb.front);
        glGetIntegerv(GL_ALPHA_TEST_FUNC, &gWb.afunc); glGetFloatv(GL_ALPHA_TEST_REF, &gWb.aref);
        for (int u = 0; u < 9; u++) gWb.units[u] = gBound[u];
    }
    WbLay L;
    const int pi = gWb.nPieces;
    if (!gWb.layOk || gWb.perPiece || (pi & 31) == 0) {      // the layout is read once and checked every 32nd piece
        WbQuery(L, vp);
        if (!gWb.layOk) { gWb.lay = L; gWb.layOk = true; }
        else if (!gWb.perPiece && !WbLaySame(L, gWb.lay)) { gWb.perPiece = true; Log("Wall batch: pieces' arrays differ - reading each piece's layout (slower recording)"); }
    } else {
        L = gWb.lay;
        if (!L.cOn) glGetFloatv(GL_CURRENT_COLOR, L.curC);      // a colour set per piece
    }
    if (L.nOn && L.nTy != GL_FLOAT) { WbFail("wall normals are not floats"); return; }
    if (L.tOn && L.tTy != GL_FLOAT) { WbFail("wall texture coordinates are not floats"); return; }
    if (L.cOn && L.cTy != GL_UNSIGNED_BYTE && L.cTy != GL_FLOAT) { WbFail("wall colours are of an unknown type"); return; }
    const int nst = L.nSt ? L.nSt : 12, tst = L.tSt ? L.tSt : L.tSz * 4, cst = L.cSt ? L.cSt : L.cSz * (L.cTy == GL_FLOAT ? 4 : 1);
    if ((L.nOn && !Readable(vp + L.nOff + (ptrdiff_t)first * nst, (size_t)count * nst)) || (L.tOn && !Readable(vp + L.tOff + (ptrdiff_t)first * tst, (size_t)count * tst)) ||
        (L.cOn && !Readable(vp + L.cOff + (ptrdiff_t)first * cst, (size_t)count * cst))) { WbFail("unreadable normals / texture coordinates / colours"); return; }
    // model -> world: the inverse camera times this piece's modelview (rigid camera), so the record does not depend on the camera
    gDrawTick++;
    GLdouble mv[16], inv[16] = {0}, M[16];
    GetMV(mv);
    const GLdouble* C = gCamMV;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) inv[c * 4 + r] = C[r * 4 + c];
    for (int r = 0; r < 3; r++) inv[12 + r] = -(C[r * 4 + 0] * C[12] + C[r * 4 + 1] * C[13] + C[r * 4 + 2] * C[14]);
    inv[15] = 1.0;
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++) M[c * 4 + r] = inv[r] * mv[c * 4] + inv[4 + r] * mv[c * 4 + 1] + inv[8 + r] * mv[c * 4 + 2] + inv[12 + r] * mv[c * 4 + 3];
    float P[32] = {0};
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 4; c++) P[r * 4 + c] = (float)M[c * 4 + r];
    P[12] = gPcBase; P[13] = gPcLift; P[14] = gPcThick[0]; P[15] = gPcThick[1];
    for (int k = 0; k < 4; k++) P[16 + k] = gPcExt[k];
    gWb.pieces.insert(gWb.pieces.end(), P, P + 32);
    gWb.nPieces++;
    WbGroup* G = nullptr;
    const bool nc = nocull && gWb.cull;
    for (WbGroup& g : gWb.groups)
        if (g.prog == gCurProg && g.tex == gBound[0] && g.nocull == nc) { G = &g; break; }
    if (!G) { gWb.groups.push_back(WbGroup{gCurProg, gBound[0], nc, {}, 0, 0}); G = &gWb.groups.back(); }
    const size_t base = G->v.size();
    G->v.resize(base + count);
    for (int i = 0; i < count; i++) {
        WbVert& w = G->v[base + i];
        const float* pp = (const float*)(vp + (size_t)(first + i) * vst);
        w.p[0] = pp[0]; w.p[1] = pp[1]; w.p[2] = pp[2];
        if (L.nOn) { const float* q = (const float*)(vp + L.nOff + (ptrdiff_t)(first + i) * nst); w.n[0] = q[0]; w.n[1] = q[1]; w.n[2] = q[2]; }
        else { w.n[0] = L.curN[0]; w.n[1] = L.curN[1]; w.n[2] = L.curN[2]; }
        if (L.tOn) { const float* q = (const float*)(vp + L.tOff + (ptrdiff_t)(first + i) * tst); w.t[0] = q[0]; w.t[1] = L.tSz > 1 ? q[1] : 0.0f; }
        else { w.t[0] = L.curT[0]; w.t[1] = L.curT[1]; }
        if (L.cOn) {
            const uint8_t* q = vp + L.cOff + (ptrdiff_t)(first + i) * cst;
            if (L.cTy == GL_UNSIGNED_BYTE) { w.c[0] = q[0]; w.c[1] = q[1]; w.c[2] = q[2]; w.c[3] = L.cSz > 3 ? q[3] : 255; }
            else { const float* f = (const float*)q; for (int k = 0; k < 4; k++) w.c[k] = (uint8_t)std::max(0.0f, std::min(255.0f, (k < L.cSz ? f[k] : 1.0f) * 255.0f + 0.5f)); }
        } else
            for (int k = 0; k < 4; k++) w.c[k] = (uint8_t)std::max(0.0f, std::min(255.0f, L.curC[k] * 255.0f + 0.5f));
        w.idx = (float)pi;
    }
}
// end of a recorded pass: upload, or give up for this signature
static bool WbEnd() {
    size_t verts = 0;
    for (WbGroup& g : gWb.groups) verts += g.v.size();
    if (gWbOther) WbFail("the game drew part of the walls another way");
    if (!gWb.bad && gWb.nPieces < 20) WbFail("too few wall pieces");
    if (!gWb.bad && !WbGlLoad()) WbFail("driver functions missing");
    if (gWb.bad) {
        Log("Wall batch not used: %s (%d pieces, %d other draws) - walls drawn piece by piece", gWb.why, gWb.nPieces, gWbOther);
        WbFree();
        return false;
    }
    while (glGetError() != GL_NO_ERROR) {}
    for (WbGroup& g : gWb.groups) {
        gWbGl.gen(1, &g.vbo);
        gWbGl.bind(0x8892, g.vbo);
        gWbGl.data(0x8892, (ptrdiff_t)(g.v.size() * sizeof(WbVert)), g.v.data(), 0x88E4 /* STATIC_DRAW */);
        g.n = (GLsizei)g.v.size();
        std::vector<WbVert>().swap(g.v);
    }
    gWbGl.bind(0x8892, (GLuint)gArrayVbo);
    // the pieces' values: 128 pieces a row of a 1024-texel float texture
    gWb.texH = (gWb.nPieces + 127) / 128;
    gWb.pieces.resize((size_t)gWb.texH * 1024 * 4, 0.0f);
    if (!gWb.pieceTex) glGenTextures(1, &gWb.pieceTex);
    pActiveTexture(GL_TEXTURE0 + 13);
    oBindTexture(GL_TEXTURE_2D, gWb.pieceTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glTexImage2D(GL_TEXTURE_2D, 0, 0x8814 /* RGBA32F */, 1024, gWb.texH, 0, GL_RGBA, GL_FLOAT, gWb.pieces.data());
    oBindTexture(GL_TEXTURE_2D, gBound[13]);
    pActiveTexture(gActiveUnit);
    std::vector<float>().swap(gWb.pieces);
    GLenum er = glGetError();
    if (er != GL_NO_ERROR) {
        Log("Wall batch not used: GL error 0x%x uploading it - walls drawn piece by piece", (unsigned)er);
        WbFree();
        return false;
    }
    Log("Wall batch built: %d pieces, %u vertices, %d draw group(s), %.0f ms%s", gWb.nPieces, (unsigned)verts, (int)gWb.groups.size(),
        (NowSec() - gWb.capT0) * 1000.0, gWb.perPiece ? " (arrays read per piece)" : "");
    return true;
}
// draw the whole record (inside the wall pass: the iso uniforms say "wall"); false = could not (the game's pass must run)
static bool WbDraw() {
    if (gWb.groups.empty() || !gWbGl.ok) return false;
    struct Loc { GLint batch, pieces, size, att; };
    static std::map<GLuint, Loc> locs;
    static int locsReload = -1;
    if (locsReload != gGfxReloads) { locs.clear(); locsReload = gGfxReloads; }      // programs are new after a graphics reload
    for (WbGroup& g : gWb.groups) {
        auto it = locs.find(g.prog);
        if (it == locs.end()) {
            Loc l = {pGetUniformLocation(g.prog, "isoBatch"), pGetUniformLocation(g.prog, "isoPieces"), pGetUniformLocation(g.prog, "isoPiecesSize"),
                     gWbGl.attLoc(g.prog, "isoPiece")};
            it = locs.insert({g.prog, l}).first;
            if (l.batch < 0 || l.pieces < 0 || l.att < 0) Log("Wall batch: program %u has no batch inputs (%d %d %d) - walls drawn piece by piece", g.prog, l.batch, l.pieces, l.att);
        }
        if (it->second.batch < 0 || it->second.pieces < 0 || it->second.att < 0) return false;
    }
    PFN_UseProgram use = oUseProgram ? oUseProgram : oUseProgramObjectARB;
    const GLenum mm = gMatMode;
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixd(gCamMV);
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_POLYGON_BIT);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    if (gWb.dt) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glDepthMask(gWb.dm);
    if (gWb.bl) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBlendFunc((GLenum)gWb.bs, (GLenum)gWb.bd);
    if (gWb.at) glEnable(GL_ALPHA_TEST); else glDisable(GL_ALPHA_TEST);
    glAlphaFunc((GLenum)gWb.afunc, gWb.aref);
    glCullFace((GLenum)gWb.cullMode);
    glFrontFace((GLenum)gWb.front);
    for (int u = 1; u < 9; u++)
        if (gWb.units[u] != gBound[u]) { pActiveTexture(GL_TEXTURE0 + u); oBindTexture(GL_TEXTURE_2D, gWb.units[u]); }
    pActiveTexture(GL_TEXTURE0 + 13);
    oBindTexture(GL_TEXTURE_2D, gWb.pieceTex);
    pActiveTexture(GL_TEXTURE0);
    gWbGl.clientAct(GL_TEXTURE0);
    glDisableClientState(GL_INDEX_ARRAY); glDisableClientState(GL_EDGE_FLAG_ARRAY); glDisableClientState(0x8457); glDisableClientState(0x845E);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_NORMAL_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY); glEnableClientState(GL_COLOR_ARRAY);
    for (WbGroup& g : gWb.groups) {
        const Loc& l = locs[g.prog];
        use(g.prog);
        pUniform1i(l.batch, 1);
        pUniform1i(l.pieces, 13);
        if (l.size >= 0) pUniform2f(l.size, 1024.0f, (float)gWb.texH);
        oBindTexture(GL_TEXTURE_2D, g.tex);
        if (gWb.cull && !g.nocull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        gWbGl.bind(0x8892, g.vbo);
        glVertexPointer(3, GL_FLOAT, sizeof(WbVert), (const void*)0);
        glNormalPointer(GL_FLOAT, sizeof(WbVert), (const void*)12);
        glTexCoordPointer(2, GL_FLOAT, sizeof(WbVert), (const void*)24);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(WbVert), (const void*)32);
        gWbGl.vap((GLuint)l.att, 1, GL_FLOAT, GL_FALSE, sizeof(WbVert), (const void*)36);
        gWbGl.enable((GLuint)l.att);
        oDrawArrays(GL_TRIANGLES, 0, g.n);
        gWbGl.disable((GLuint)l.att);
        pUniform1i(l.batch, 0);
    }
    gWbGl.bind(0x8892, (GLuint)gArrayVbo);
    oBindTexture(GL_TEXTURE_2D, gBound[0]);
    pActiveTexture(GL_TEXTURE0 + 13);
    oBindTexture(GL_TEXTURE_2D, gBound[13]);
    for (int u = 1; u < 9; u++)
        if (gWb.units[u] != gBound[u]) { pActiveTexture(GL_TEXTURE0 + u); oBindTexture(GL_TEXTURE_2D, gBound[u]); }
    pActiveTexture(gActiveUnit);
    glPopClientAttrib();
    glPopAttrib();
    glPopMatrix();
    glMatrixMode(mm);
    use(gCurProg);
    return true;
}
static double gPieceCentre[2] = {0, 0};
static bool gPieceCentreOK = false;
static int gPieceRunAxis = -1;      // 1.9.124: the last wall piece's run axis (0 = model x, 1 = model z, -1 = a junction / post)
static int gCapsFrame = -1;      // 1.9.124: wall frame the fence caps were last drawn in      // world centre of the last wall piece's footprint
static void WallPieceCore(GLenum mode, GLint first, GLsizei count) {
    gPieceCentreOK = false;
    gPieceRunAxis = -1;
    if (count < 3 || !gCurProg) return;
    GLdouble mv[16];
    GetMV(mv);
    double wp[2];      // world position of this piece's origin: ViewInv * (translation column of the modelview)
    for (int r = 0; r < 2; r++) wp[r] = gViewInv[0 * 4 + r] * mv[12] + gViewInv[1 * 4 + r] * mv[13] + gViewInv[2 * 4 + r] * mv[14] + gViewInv[12 + r];
    const int originKey = (int)floor(wp[1]) * 65536 + (int)floor(wp[0]);
    auto it = gLocs.find(gCurProg);
    if (it == gLocs.end() || it->second.base < 0 || !pUniform1f) {
        gDrawnCur.insert(originKey);
        return;
    }
    struct LastU {
        GLuint prog;
        float base, lift, tx, tz;
    };
    static LastU lu = {0, 1e9f, 1e9f, -1.f, -1.f};
    static unsigned luEpoch = 0;      // last uniform values: most pieces repeat them, and every skipped call is a driver round trip saved
    if (lu.prog != gCurProg || luEpoch != gUniEpoch) {
        lu = {gCurProg, 1e9f, 1e9f, -1.f, -1.f};
        luEpoch = gUniEpoch;
    }      // another program, or SetIsoUniforms reset them (it runs on every program bind)
    {
        static int lastWF = -1;
        if (lastWF != gWallFrames) {
            lastWF = gWallFrames;
            lu = {gCurProg, 1e9f, 1e9f, -1.f, -1.f};
        }
    }      // re-send once per frame: other draws change them
    const ArrState& AS = GetArr();
    void* vp = AS.vp;
    GLint sz = AS.sz, ty = AS.ty, st = AS.st, vbo = AS.vbo;
    int stride = st ? st : sz * 4;
    float base = -0.989f;
    // wall pieces come in a handful of shapes: measure each shape once (keyed on its vertex count and a few sampled vertices), not once per piece per frame
    struct WpInfo {
        float mn[3], mx[3];
        float smn[2], smx[2];      // 1.9.107: x / z extent without the full-width base lip (y < -0.97): where the wall body really ends
        bool tx, tz;
    };
    static std::unordered_map<unsigned long long, WpInfo> wpCache;
    WpInfo wi;
    bool haveInfo = false;
    if (sz >= 3 && ty == 0x1406 && !vbo && vp && Readable((uint8_t*)vp + (size_t)first * stride, (size_t)count * stride)) {
        const float* A0 = (const float*)((uint8_t*)vp + (size_t)first * stride);
        const float* A1 = (const float*)((uint8_t*)vp + (size_t)(first + count / 2) * stride);
        const float* A2 = (const float*)((uint8_t*)vp + (size_t)(first + count - 1) * stride);
        unsigned long long key = (unsigned long long)count * 1000003ull ^ (unsigned long long)(long long)lroundf(A0[0] * 1009.0f) * 7919ull ^
                                 (unsigned long long)(long long)lroundf(A0[1] * 997.0f) * 104729ull ^
                                 (unsigned long long)(long long)lroundf(A1[0] * 991.0f) * 15485863ull ^
                                 (unsigned long long)(long long)lroundf(A1[2] * 983.0f) * 32452843ull ^
                                 (unsigned long long)(long long)lroundf(A2[1] * 977.0f) * 49979687ull;
        auto ci = wpCache.find(key);
        if (ci != wpCache.end()) {
            wi = ci->second;
            haveInfo = true;
        } else {
            for (int k = 0; k < 3; k++) {
                wi.mn[k] = 1e9f;
                wi.mx[k] = -1e9f;
            }
            wi.tx = wi.tz = false;
            wi.smn[0] = wi.smn[1] = 1e9f; wi.smx[0] = wi.smx[1] = -1e9f;
            for (int i = 0; i < count; i++) {
                const float* P = (const float*)((uint8_t*)vp + (size_t)(first + i) * stride);
                for (int k = 0; k < 3; k++) {
                    if (P[k] < wi.mn[k]) wi.mn[k] = P[k];
                    if (P[k] > wi.mx[k]) wi.mx[k] = P[k];
                }
                if (P[1] > -0.97f) {
                    wi.smn[0] = std::min(wi.smn[0], P[0]); wi.smx[0] = std::max(wi.smx[0], P[0]);
                    wi.smn[1] = std::min(wi.smn[1], P[2]); wi.smx[1] = std::max(wi.smx[1], P[2]);
                }
                if (fabsf(P[0]) < 0.4f) wi.tx = true;
                if (fabsf(P[2]) < 0.4f) wi.tz = true;
            }
            if (wpCache.size() > 2000) wpCache.clear();
            wpCache[key] = wi;
            haveInfo = true;
            static int lines = 0;
            if (lines < 12) {
                lines++;
                Log("WALLEXT verts=%d x[%.3f..%.3f] y[%.3f..%.3f] z[%.3f..%.3f] thinX %d thinZ %d", count, wi.mn[0], wi.mx[0], wi.mn[1], wi.mx[1],
                    wi.mn[2], wi.mx[2], (int)wi.tx, (int)wi.tz);
            }
        }
        base = wi.mn[1];
        {      // which tiles this piece covers: its footprint in the world (the origin alone sits on a tile edge for some pieces, which misplaced the 1x1 wall filler)
            double mnw[2] = {1e9, 1e9}, mxw[2] = {-1e9, -1e9};
            for (int cc = 0; cc < 8; cc++) {
                double px = (cc & 1) ? wi.mx[0] : wi.mn[0], py = (cc & 2) ? wi.mx[1] : wi.mn[1], pz = (cc & 4) ? wi.mx[2] : wi.mn[2];
                double ex = mv[0] * px + mv[4] * py + mv[8] * pz + mv[12], ey = mv[1] * px + mv[5] * py + mv[9] * pz + mv[13],
                       ez = mv[2] * px + mv[6] * py + mv[10] * pz + mv[14];
                for (int r = 0; r < 2; r++) {
                    double v = gViewInv[0 * 4 + r] * ex + gViewInv[1 * 4 + r] * ey + gViewInv[2 * 4 + r] * ez + gViewInv[12 + r];
                    if (v < mnw[r]) mnw[r] = v;
                    if (v > mxw[r]) mxw[r] = v;
                }
            }
            gPieceCentre[0] = (mnw[0] + mxw[0]) * 0.5;
            gPieceCentre[1] = (mnw[1] + mxw[1]) * 0.5;
            gPieceCentreOK = true;
            {      // 1.9.124: which model axis this piece runs along (body reaches both tile edges on it and stays inside the shaft on the other); -1 = neither
                const bool fx = wi.smx[0] > 0.4f && wi.smn[0] < -0.4f, fz = wi.smx[1] > 0.4f && wi.smn[1] < -0.4f;
                gPieceRunAxis = (fx && !fz) ? 0 : (fz && !fx) ? 1 : -1;
            }
            int x0 = (int)floor(mnw[0] + 0.4), x1 = (int)floor(mxw[0] - 0.4), y0 = (int)floor(mnw[1] + 0.4),
                y1 = (int)floor(
                    mxw[1] -
                    0.4);      // a piece counts for a tile only if it reaches that tile's middle (wall ends poke a little into the next tile)
            if (x1 < x0) x0 = x1 = (int)floor((mnw[0] + mxw[0]) * 0.5);
            if (y1 < y0) y0 = y1 = (int)floor((mnw[1] + mxw[1]) * 0.5);
            {
                static int cl = 0;
                if (cl < 6 && x0 <= 114 && x1 >= 114 && y0 <= 30 && y1 >= 30) {
                    cl++;
                    Log("CELLCLAIM 114,30 claimed by a piece of %d vertices, footprint x %.2f..%.2f y %.2f..%.2f", (int)count, mnw[0], mxw[0], mnw[1],
                        mxw[1]);
                }
            }
            if (x1 - x0 <= 3 && y1 - y0 <= 3) {
                for (int yy = y0; yy <= y1; yy++)
                    for (int xx = x0; xx <= x1; xx++) gDrawnCur.insert(yy * 65536 + xx);
            } else
                gDrawnCur.insert(originKey);
        }
        if (it->second.wext >= 0 && pUniform4f) {      // 1.9.106 (user: walls should run to the tile end - no gap beside doors)
            float ext[4] = {0, 0, 0, 0};
            if (!gDoorTiles.empty()) {
                double ox = MVWorldX(mv),
                       oy = MVWorldY(mv);
                for (int ax = 0; ax < 2; ax++) {      // model x (column 0) and model z (column 2): their world directions
                    const int c0 = ax == 0 ? 0 : 8;
                    double wx = gViewInv[0] * mv[c0] + gViewInv[4] * mv[c0 + 1] + gViewInv[8] * mv[c0 + 2],
                           wy = gViewInv[1] * mv[c0] + gViewInv[5] * mv[c0 + 1] + gViewInv[9] * mv[c0 + 2];
                    double l = sqrt(wx * wx + wy * wy);
                    if (l < 1e-6) continue;
                    wx /= l; wy /= l;
                    const float lo = wi.smn[ax], hi = wi.smx[ax];      // the body, not the full-width lip
                    // 1.9.109 (user: wall and gate widened together, overlapping): only along the wall's own run - a piece that runs the other way
                    // (full length on the other axis) must not have its side pushed out to the tile edge, which made it a full-tile block
                    const int ox2 = 1 - ax;
                    if (wi.smx[ox2] > 0.4f && wi.smn[ox2] < -0.4f) continue;
                    for (int sgn = -1; sgn <= 1; sgn += 2) {
                        float end = sgn < 0 ? lo : hi;
                        if (fabsf(end) > 0.4f) continue;      // this side already reaches the tile edge
                        int tx = (int)floor(ox + sgn * wx * 0.9), ty = (int)floor(oy + sgn * wy * 0.9);
                        auto dt = gDoorTiles.find(ty * 65536 + tx);
                        if (dt != gDoorTiles.end() && gWallFramesSeenExt - dt->second <= (gWbCapture ? 36000 : 3)) ext[ax * 2 + (sgn > 0 ? 1 : 0)] = 1.0f;
                    }
                }
            }
            for (int k = 0; k < 4; k++) gPcExt[k] = ext[k];
            {      // 1.9.121: set only when it changes (it was a uniform call for every wall piece)
                float* le = gWextLast;
                if (gWextFrame != gSwapCount || gWextProg != gCurProg || le[0] != ext[0] || le[1] != ext[1] || le[2] != ext[2] || le[3] != ext[3]) {
                    pUniform4f(it->second.wext, ext[0], ext[1], ext[2], ext[3]);
                    gWextProg = gCurProg; gWextFrame = gSwapCount; for (int k = 0; k < 4; k++) le[k] = ext[k];
                }
            }
            static int el = 0;
            if (el < 6 && ext[0] + ext[1] + ext[2] + ext[3] > 0) { el++; Log("WALLDOOREXT a wall end beside a door runs to the tile edge (%.0f %.0f %.0f %.0f), body x %.2f..%.2f z %.2f..%.2f", ext[0], ext[1], ext[2], ext[3], wi.smn[0], wi.smx[0], wi.smn[1], wi.smx[1]); }
        }
        if (it->second.thick >= 0) {
            float tx = wi.tx ? 1.0f : 0.0f, tz = wi.tz ? 1.0f : 0.0f;
            gPcThick[0] = tx; gPcThick[1] = tz;
            if (lu.tx != tx || lu.tz != tz) {
                pUniform2f(it->second.thick, tx, tz);
                lu.tx = tx;
                lu.tz = tz;
            }
        }
    }
    if (!haveInfo) {
        gDrawnCur.insert(originKey);
        if (it->second.wext >= 0 && pUniform4f) { pUniform4f(it->second.wext, 0.0f, 0.0f, 0.0f, 0.0f); gWextFrame = -1; }
        for (int k = 0; k < 4; k++) gPcExt[k] = 0.0f;
    }
    gPcBase = base;
    if (lu.base != base) {
        pUniform1f(it->second.base, base);
        lu.base = base;
    }
    if (it->second.lift >=
        0) {      // lower the piece so its base meets the floor; the reading comes from the cached bounding box, so it costs 8 corners, not every vertex
        float lift = gCfg.wallLift;
        if (gCfg.wallAutoGround && haveInfo) {
            double minH = 1e9;
            for (int cc = 0; cc < 8; cc++) {
                double px = (cc & 1) ? wi.mx[0] : wi.mn[0], py = (cc & 2) ? wi.mx[1] : wi.mn[1], pz = (cc & 4) ? wi.mx[2] : wi.mn[2];
                double ex = mv[0] * px + mv[4] * py + mv[8] * pz + mv[12], ey = mv[1] * px + mv[5] * py + mv[9] * pz + mv[13],
                       ez = mv[2] * px + mv[6] * py + mv[10] * pz + mv[14];
                double hh = gCfg.upSign * (gViewInv[2] * ex + gViewInv[6] * ey + gViewInv[10] * ez + gViewInv[14]);
                if (hh < minH) minH = hh;
            }
            if (minH > 0.005 && minH < 1.5)
                lift = (float)-minH;
            else
                lift = gCfg.wallLift;
        }
        gPcLift = lift;
        if (fabsf(lu.lift - lift) > 0.0004f) {
            pUniform1f(it->second.lift, lift);
            lu.lift = lift;
        }
    }
}
// A wall cell the game builds no piece for (a one-tile wall squeezed between two doors) is filled by re-drawing a full-cube piece (the template: 36 vertices)
// moved onto the missing cell and stretched along the run of the wall. It is drawn right after the template's own uniforms are set, so it takes the same
// wall colour, cutaway and ground fitting; once per frame.
static void WallPiece(GLenum mode, GLint first, GLsizei count) {
    WallPieceCore(mode, first, count);
    WbRecord(mode, first, count, false);      // 1.9.130: the piece itself (drawn by hkDrawArrays right after this)
    // 1.9.124 (wall end caps): a wall that runs into a fence is open at that edge. The first plain run piece of the frame (36 vertices, one wall
    // thick, run along one model axis) is drawn again squashed to a thin slab on each such edge - its thickness, height, colour and cutaway are
    // the walls' own (same uniforms, and the shader cuts by the slab's own position).
    if (count == 36 && mode == GL_TRIANGLES && !gCaps.empty() && gCapsFrame != gWallFrames && gPieceRunAxis >= 0) {
        gCapsFrame = gWallFrames;
        GLdouble mv[16];
        GetMV(mv);
        // the run axis (model x or z) in the world: which world axis it lies along
        const int c0 = gPieceRunAxis == 0 ? 0 : 8;
        const double rwx = gViewInv[0] * mv[c0] + gViewInv[4] * mv[c0 + 1] + gViewInv[8] * mv[c0 + 2], rwy = gViewInv[1] * mv[c0] + gViewInv[5] * mv[c0 + 1] + gViewInv[9] * mv[c0 + 2];
        const bool runIsWorldX = fabs(rwx) >= fabs(rwy);
        const double ref0 = gPieceCentreOK ? gPieceCentre[0] : MVWorldX(mv), ref1 = gPieceCentreOK ? gPieceCentre[1] : MVWorldY(mv);
        GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
        if (cullWas) glDisable(GL_CULL_FACE);
        for (long long ck : gCaps) {
            const int s = (int)(ck & 3), key = (int)(ck >> 2), cxk = key & 0xFFFF, cyk = key >> 16;
            const double ex = cxk + 0.5 + (s == 0 ? -0.5 : s == 1 ? 0.5 : 0.0), ey = cyk + 0.5 + (s == 2 ? -0.5 : s == 3 ? 0.5 : 0.0);
            const bool normalX = s < 2;      // the edge's normal runs along world x: the slab must be thin in x, its run (= the wall's run) along x
            double d[3] = {ex - ref0, ey - ref1, 0.0}, e[3];
            for (int i = 0; i < 3; i++) e[i] = gViewInv[i * 4 + 0] * d[0] + gViewInv[i * 4 + 1] * d[1] + gViewInv[i * 4 + 2] * d[2];
            glPushMatrix();
            glLoadIdentity();
            glTranslated(e[0], e[1], e[2]);
            glMultMatrixd(mv);
            if (normalX != runIsWorldX) glRotated(90.0, 0.0, 1.0, 0.0);      // turn the run onto the wall's direction
            if (gPieceRunAxis == 0) glScaled(0.06, 1.0, 1.0); else glScaled(1.0, 1.0, 0.06);      // squash along the run: a slab the wall's thickness
            oDrawArrays(mode, first, count);
            WbRecord(mode, first, count, true);
            glPopMatrix();
        }
        if (cullWas) glEnable(GL_CULL_FACE);
        static int cl = 0;
        if (cl < 3) { cl++; Log("WALLCAPS %d wall ends against fences closed", (int)gCaps.size()); }
    }
    // template: a 96-vertex junction piece (it has the top edge trim on every side, like a lone post should); a plain 36-vertex run only if no junction is drawn
    static int seen96Frame = -100;
    if (count == 96) seen96Frame = gWallFrames;
    bool useThis = count == 96 || (count == 36 && gWallFrames - seen96Frame > 2);
    if (!useThis || gExtraDone || gMissing.empty() || !gCfg.redrawMissingWalls || mode != GL_TRIANGLES) return;
    gExtraDone = true;
    GLdouble mv[16];
    GetMV(mv);
    double wp[2];
    for (int r = 0; r < 2; r++) wp[r] = gViewInv[0 * 4 + r] * mv[12] + gViewInv[1 * 4 + r] * mv[13] + gViewInv[2 * 4 + r] * mv[14] + gViewInv[12 + r];
    if (gMissing.empty()) return;
    static double lastLog = 0;
    static size_t lastN = (size_t)-1;
    static int lastFirst = -1;
    double nw = NowSec();
    if (nw - lastLog > 5.0 && (lastN != gMissing.size() || lastFirst != gMissing[0])) {
        lastLog = nw;
        lastN = gMissing.size();
        lastFirst = gMissing[0];
        Log("MISSINGWALL %d wall cells have no piece this frame (first at %d,%d); filling them from a template piece", (int)gMissing.size(),
            gMissing[0] & 0xFFFF, gMissing[0] >> 16);
    }
    double stretch = gCfg.wallThick > 0.2 ? 1.0 / gCfg.wallThick : 1.0;
    for (int key : gMissing) {
        int mx = key & 0xFFFF, my = key >> 16;
        bool wx = gDrawnPrev.count(my * 65536 + mx - 1) || gDrawnPrev.count(my * 65536 + mx + 1);
        bool wy = gDrawnPrev.count((my - 1) * 65536 + mx) || gDrawnPrev.count((my + 1) * 65536 + mx);
        double sx = 1.0, sz = 1.0;
        if (count == 36) {
            if (wx && !wy)
                sx = stretch;
            else if (wy && !wx)
                sz = stretch;
        }      // a run piece is stretched along its run; the junction piece already fills the tile
        double ref0 = gPieceCentreOK ? gPieceCentre[0] : wp[0],
               ref1 = gPieceCentreOK ? gPieceCentre[1] : wp[1];      // move the template's footprint centre (not its origin) onto the empty cell
        double d[3] = {mx + 0.5 - ref0, my + 0.5 - ref1, 0.0}, e[3];
        for (int i = 0; i < 3; i++) e[i] = gViewInv[i * 4 + 0] * d[0] + gViewInv[i * 4 + 1] * d[1] + gViewInv[i * 4 + 2] * d[2];
        GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
        if (cullWas) glDisable(GL_CULL_FACE);      // every face of the filler block, whatever way the template winds
        glPushMatrix();
        glLoadIdentity();
        glTranslated(e[0], e[1], e[2]);
        glMultMatrixd(mv);
        glScaled(sx, 1.0, sz);
        oDrawArrays(mode, first, count);
        WbRecord(mode, first, count, true);
        glPopMatrix();
        if (cullWas) glEnable(GL_CULL_FACE);
    }
}      // Draw survey (diagnostic): logs each distinct (program, texture units 0/1, mode, count) once,
// so the game's 2D wall/floor layers can be told apart. Only with "Verbose log" on.
static void DrawSurvey(const char* kind, GLenum m, GLsizei c) {
    {
        static GLuint lastTex = 0xFFFFFFFF;
        if (gBound[0] != lastTex) {
            lastTex = gBound[0];
            if (gBound[0] < 4096 && gBound[0] && !gInWallPass) gSeenTex.insert((int)gBound[0]);
        }
    }      // always collected, for the layer test
    if (!gCfg.verboseLog) return;
    static std::set<unsigned long long> seen;
    static int lines = 0;
    if (lines >= 600) return;
    unsigned long long key = ((unsigned long long)gCurProg << 48) ^ ((unsigned long long)gBound[0] << 32) ^ ((unsigned long long)gBound[1] << 20) ^
                             ((unsigned long long)m << 8) ^ (unsigned long long)c * 2654435761ull ^ (gInWallPass ? 1ull : 0ull) ^
                             ((unsigned long long)(kind[0]) << 60);
    if (!seen.insert(key).second) return;
    lines++;
    Log("SURVEY %s prog=%u tex0=%u tex1=%u mode=0x%x count=%d wallpass=%d", kind, gCurProg, gBound[0], gBound[1], (unsigned)m, (int)c,
        (int)gInWallPass);
}
// Experiment: hide the game's 2D layers by the texture on unit 0 (menu > Diagnostics).
