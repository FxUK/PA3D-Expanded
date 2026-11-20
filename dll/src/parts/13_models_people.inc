// parts/13_models_people.inc - extra model slots, sprite ids, prisoner colours / Bio, DLC model install. Part of main.cpp (one translation unit, included in order): it uses everything defined in the parts before it.
// ---------------------------------------------------------------------------
// Models for sprites beyond the base game's 871 slots (see dlc_models.S)
// ---------------------------------------------------------------------------
extern "C" {
void* paiso_tbl[16384];      // our model table for sprite ids 871..16383
extern uint64_t paiso_K_cont, paiso_K_skip, paiso_J_ok, paiso_J_have, paiso_J_fail, paiso_L_cont, paiso_B1_cont, paiso_B2_cont, paiso_C1_cont,
    paiso_C2_cont, paiso_C3_cont;
void paiso_miss(void*, long long);
void paiso_note(void*, long long);
int paiso_is_accountant(void*);
void paiso_stubK();
void paiso_stubJ();
void paiso_stubL();
void paiso_stubB1();
void paiso_stubB2();
void paiso_stubC1();
void paiso_stubC2();
void paiso_stubC3();
}
#include "d11_2_ids.h"
#include "type_bank2.h"
#include "entity_names.h"
bool gObjPersonExt = false;
static bool& gObjPerson = gObjPersonExt;      // the object being drawn is a person or animal
static long long gStaffModelId = 0;           // sprite id of a staff model the game does have (lawyer), borrowed for the accountant which has none
static bool gIngOnTop = false;      // 1.9.126: the "ingredients" object just noted lies on furniture (a pan on a cooker: pot), not on the floor (a crate)
extern "C" uint64_t paiso_regs[16];
extern "C" int paiso_tree_note;
extern "C" int paiso_bare_id;
// Diagnostic: for tree and sink draws (one type, several looks), dump the registers and the memory they point at, so the per-object look can be found.
static void VariantDiag(const char* low) {
    static int shown = 0;
    static std::set<uint64_t> seenObj;
    static int shownP = 0;
    bool pris = strcmp(low, "prisoner") == 0;
    if (pris || !gCfg.variantDiag)
        return;      // prisoner layout found (Category at +0xA34); tree / sink variants solved: dumps only on request else if (shown >= 12 || (strcmp(low, "tree") != 0 && strcmp(low, "sink") != 0)) return;
    static const char* rn[16] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rsp"};
    uint64_t key = paiso_regs[1] ^ (paiso_regs[4] << 1) ^ (paiso_regs[5] << 2) ^ (paiso_regs[13] << 3) ^ (paiso_regs[14] << 4);
    if (!seenObj.insert(key).second) return;
    if (pris)
        shownP++;
    else
        shown++;
    std::string r;
    char b[64];
    if (pris) {      // prisoners: only the object (rsi) and r14, longer dump, to find the security level
        for (int i : {4, 13}) {
            uint8_t* q = (uint8_t*)paiso_regs[i];
            if (!Readable(q, 0x600)) continue;
            std::string d;
            for (int k = 0; k < 0x600; k += 4) {
                snprintf(b, sizeof b, "%x ", *(uint32_t*)(q + k));
                d += b;
            }
            Log("PRISONER #%d %s=%llx: %s", shownP, i == 4 ? "rsi" : "r14", (unsigned long long)paiso_regs[i], d.c_str());
        }
        return;
    }
    for (int i = 0; i < 16; i++) {
        snprintf(b, sizeof b, " %s=%llx", rn[i], (unsigned long long)paiso_regs[i]);
        r += b;
    }
    Log("VARIANT %s #%d%s", low, shown, r.c_str());
    for (int i = 0; i < 16; i++) {
        uint8_t* q = (uint8_t*)paiso_regs[i];
        if (i == 2 || !Readable(q, 0x180)) continue;      // rcx is the type definition
        std::string d;
        for (int k = 0; k < 0x180; k += 4) {
            snprintf(b, sizeof b, "%x ", *(uint32_t*)(q + k));
            d += b;
        }
        Log("VARIANT %s #%d %s+0: %s", low, shown, rn[i], d.c_str());
    }
}
// The prisoner's Bio (Forname +0x18, Surname +0x38, HeadType +0x68, BodyType +0x6C, BodyScale +0x70, SkinColour +0x74) is built INLINE in the
// prisoner at +0xCB0: the prisoner constructor does lea rcx,[rsi+0xCB0]; call BioCtor (exe+0x48E4A0). (Its first qword is a shared base-class
// vtable, also found at prisoner+0x8 - not usable to find it.) Checked on use: the first name must be a short readable string.
static std::string BioName(const uint8_t* s) {      // an MSVC std::string: 16-byte buffer, size at +0x10, capacity at +0x18 (>= 16: heap pointer)
    if (!Readable(s, 0x20)) return "?";
    uint64_t n = *(const uint64_t*)(s + 0x10), cap = *(const uint64_t*)(s + 0x18);
    if (n > 40) return "?";
    const char* p = cap >= 16 ? *(const char* const*)s : (const char*)s;
    if (!Readable(p, (size_t)n + 1)) return "?";
    return std::string(p, (size_t)n);
}
static const int kPrisonerBio = 0xcb0;
static uint8_t* FindBio(uint8_t* obj) {
    uint8_t* b = obj + kPrisonerBio;
    if (!Readable(b, 0x80)) return nullptr;
    uint64_t n = *(const uint64_t*)(b + 0x18 + 0x10);      // first name length: a real Bio has a short name
    if (n == 0 || n > 40) {
        static int bad = 0;
        if (bad < 5) { bad++; Log("PRISVAR prisoner %p: no Bio at +0x%x (name length %llu)", obj, kPrisonerBio, (unsigned long long)n); }
        return nullptr;
    }
    return b;
}
static void BioDump(const uint8_t* b) {      // diagnostic: the first prisoners' names and raw fields, to check the offsets against the save
    if (!gCfg.diagLog) return;      // one-off diagnostic (Settings: diagLog)
    static int n = 0;
    if (n >= 8 || !Readable(b, 0x100)) return;
    n++;
    std::string d;
    char t[12];
    for (int k = 0x58; k < 0x100; k += 4) {
        snprintf(t, sizeof t, "%08x ", *(const uint32_t*)(b + k));
        d += t;
    }
    Log("PRISVAR %s %s: +0x58.. %s", BioName(b + 0x18).c_str(), BioName(b + 0x38).c_str(), d.c_str());
}
extern "C" void paiso_note(void* def, long long id) {
    struct NoteTimer { double t0; ~NoteTimer() { if (t0 > 0) gPerf.t[7] += PerfNow() - t0; } } noteTimer{gPerfOn ? PerfNow() : 0.0};
    gNoteSpriteId = id;
    paiso_tree_note = 0;      // 1.9.111 (user: a flood light drawn as a bare tree): a note that returns early kept the last tree's flag
    gObjDoor = false;
    gObjPerson = false;
    gObjSnapType = false;
    gObjBench = false;
    gObjSurface = 0;
    gObjLift = false;
    gObjRot90 = false;
    gObjLight = 0;
    gObjUtil = false;
    gObjDetector = false;
    gObjStack = false;
    gObjStructural = false;
    gObjNoteTiny = false;
    gNoteSerial = (unsigned)gDrawSerial;
    gNoteSkip = false;
    gNotePosOk = false;
    gNotePath = (paiso_path == 1 || paiso_path == 2) ? paiso_path : 0;
    {      // the object's position: the register + offset found for this draw path (EarlyCullCheck) - trusted only once checked
        if (gNotePath && gPosSamples[gNotePath] < 3000)      // still searching (element by element: no aligned block copy of the asm data)
            for (int r = 0; r < 16; r++) gNoteRegs[r] = ((volatile uint64_t*)paiso_regs)[r];
        int pr = gNotePath ? gPosReg[gNotePath] : -1;
        uint8_t* o = pr >= 0 ? (uint8_t*)paiso_regs[pr] + gPosOff[gNotePath] : nullptr;
        if (o && (uintptr_t)o >= 0x10000 && (uintptr_t)o < 0x7FFFFFFF0000ull && !((uintptr_t)o & 3) && Readable(o, 8)) {
            float x = *(float*)o, y = *(float*)(o + 4);
            if (x == x && y == y && x > 0.0f && y > 0.0f && x < 4096.0f && y < 4096.0f) { gNotePos[0] = x; gNotePos[1] = y; gNotePosOk = true; }
        }
        // 1.9.107 (user: swivel chair missing): the position offset is right for 99 % of objects, but a type whose position sits elsewhere was
        // read wrongly and culled every time. Each object type must first be seen drawn where its note said, 4 times with no misses
        {
            static const void* lastDef = nullptr; static EarlyStat* lastES = nullptr;
            if (def != lastDef || !lastES) { if (gEarlyStat.size() > 8192) gEarlyStat.clear(); lastES = &gEarlyStat[def]; lastDef = def; }
            gNoteES = lastES;
        }
        if (gNotePosOk && gNotePath && gEarlyOn[gNotePath] && gCfg.earlyCull && gCam.active && !gInWallPass && !gMenuWorld && gNoteES->ok >= 4 && gNoteES->bad * 20 <= gNoteES->ok) {      // 1.9.121: a rare miss is tolerated (1 in 20)
            double p[4] = {gNotePos[0], gNotePos[1], 0.0, 1.0}, e[4], c[4];
            for (int r = 0; r < 4; r++) e[r] = gCamMV[r] * p[0] + gCamMV[4 + r] * p[1] + gCamMV[8 + r] * p[2] + gCamMV[12 + r];
            for (int r = 0; r < 4; r++) c[r] = gCamP[r] * e[0] + gCamP[4 + r] * e[1] + gCamP[8 + r] * e[2] + gCamP[12 + r] * e[3];
            if (c[3] > 1e-6) {
                double rx = 3.5 * fabs(gCamP[0]) / c[3], ry = 3.5 * (fabs(gCamP[5]) + fabs(gCamP[4])) / c[3];      // 3.5 tiles of margin: big and tall things
                if (fabs(c[0] / c[3]) > 1.0 + rx || fabs(c[1] / c[3]) > 1.0 + ry) { gNoteSkip = true; gEarlyCulled++; }
            }
        }
    }
    if (gNoteSkip) return;      // 1.9.87: off screen - nothing below matters for an object that will not be drawn (notes cost ~4 ms a frame on a big prison)
    if (!def || !Readable(def, 0x20)) return;
    uint8_t* p = (uint8_t*)def;
    const char* nm = (*(uint64_t*)(p + 0x18) >= 0x10) ? *(const char**)p : (const char*)p;
    if (!Readable(nm, 4)) return;
    // What kind of object this is depends only on its type's name: work it out once per type definition (this runs for every object drawn,
    // well over 100,000 times a second) and keep it. The cached name's first bytes are compared each time in case a definition is reused.
    struct NoteClass { char raw[16]; char low[64]; bool person, snap, tv, shop, serving, prisoner, tree, bench, door, lift, rot90, util, animal; int surf, light; unsigned cnt; };
    static std::unordered_map<const void*, NoteClass> nclass;
    NoteClass* C = nullptr;
    static const void* lastDef = nullptr; static NoteClass* lastC = nullptr;
    {      // 1.9.123: a one-entry cache in front of the map (runs of the same type are common: lights, bushes, prisoners)
        if (def == lastDef && lastC && Readable(nm, 16) && memcmp(lastC->raw, nm, 16) == 0) C = lastC;
        else {
            auto ci = nclass.find(def);
            if (ci != nclass.end() && Readable(nm, 16) && memcmp(ci->second.raw, nm, 16) == 0) { C = &ci->second; lastDef = def; lastC = C; }
        }
    }
    if (!C) {
        NoteClass k;
        memset(&k, 0, sizeof k);
        int i = 0;
        for (; i < 63 && nm[i] && Readable(nm + i, 1); i++) k.low[i] = (char)tolower((unsigned char)nm[i]);
        k.low[i] = 0;
        const char* low = k.low;
        for (int j = 0; j < 16 && j <= i; j++) k.raw[j] = nm[j];
        for (int j = i + 1; j < 16; j++) k.raw[j] = Readable(nm + j, 1) ? nm[j] : 0;
        k.tree = strcmp(low, "tree") == 0;
        for (const char* en : kEntityNames)
            if (strcmp(en, low) == 0) { k.person = true; Log("PERSONSEEN %s id %lld", low, id); break; }
        static const char* snapWords[] = {"toilet", "sink",   "shower",  "bookcase", "shelf",   "shelves", "tv",     "television", "cabinet",
                                          "locker", "fridge", "freezer", "cooker",   "oven",    "washing", "dryer",  "radiator",   "vending",
                                          "phone",  "urinal", "bath",    "wardrobe", "drawers", "counter", "boiler", "cupboard",   "bin"};
        for (const char* w : snapWords)
            if (strstr(low, w)) { k.snap = true; Log("SNAPTYPE %s", low); break; }
        k.tv = (strcmp(low, "tv") == 0 || strstr(low, "television") != nullptr || strstr(low, "toilet") != nullptr || strstr(low, "radiator") != nullptr ||
                strcmp(low, "bin") == 0);
        k.shop = strcmp(low, "shopfront") == 0;
        k.serving = strcmp(low, "servingtable") == 0;
        k.prisoner = k.person && strncmp(low, "prisoner", 8) == 0;      // 1.9.89: not prisoneruniform / prisonerbus / prisonerletter (were 40 % of "prisoner" notes, each a failed object search)
        k.bench = strstr(low, "bench") != nullptr;
        if (k.bench) Log("BENCHNOTE %s sprite id %lld", low, id);
        k.door = strstr(low, "door") && !strstr(low, "indoor") && !strstr(low, "outdoor") && !strstr(low, "mat") && !strstr(low, "control") &&
                 !strstr(low, "timer") && !strstr(low, "window") && !strstr(low, "servo");
        if (k.door) Log("DOORTYPE %s", low);
        if ((strstr(low, "table") && !strstr(low, "pool") && !strstr(low, "operating")) || strstr(low, "desk")) k.surf = 1;
        else if (strstr(low, "sofa") || strstr(low, "bench") || strstr(low, "couch")) k.surf = 2;
        else if ((strstr(low, "fridge") || strstr(low, "freezer")) && !strstr(low, "lid")) k.surf = 1;      // 1.9.88: the game's own freezer is back (glass top): food on top
        else if (strcmp(low, "cooker") == 0) k.surf = 1;      // 1.9.107: pans of food stand on its top (were drawn inside it)
        else if (strstr(low, "shelf") || strstr(low, "shelves") || strstr(low, "workshopsaw") ||
                 strstr(low, "workshoppress")) k.surf = 1;      // 1.9.77: workshop machines and shelves hold their stacks too      // 1.9.76: the food stored in it shows on its top (user: missing food, clear-top freezer)
        k.lift = !k.person && (strstr(low, "tray") || strstr(low, "meal") || strstr(low, "bowl") || strstr(low, "cup") || strcmp(low, "stack") == 0 ||
                               strstr(low, "food") || strstr(low, "plate")) && !strstr(low, "table");
        k.rot90 = strstr(low, "waterboiler") != nullptr;      // the stock tank lies with its length out from the wall (user 8 Nov: needs rotating)
        // lamps (1.9.72): each one's position feeds the light-pool map
        if (strcmp(low, "light") == 0) k.light = 1;
        else if (strcmp(low, "lightoutdoor") == 0 || strcmp(low, "d_solarlight") == 0) k.light = 2;
        else if (strcmp(low, "d_floodlight") == 0) k.light = 3;
        else if (strcmp(low, "d_walllight") == 0 || strcmp(low, "d_solarwalllight") == 0) k.light = 4;
        if (k.light) Log("LAMPTYPE %s kind %d", low, k.light);
        // 1.9.82: pipes and cables lie under the floor (the own 3D floor covers them): drawing them only costs time
        k.util = (strstr(low, "pipe") || strstr(low, "cable") || strstr(low, "valve")) && !strstr(low, "drain") && !k.person;
        if (k.util) Log("UTILITYTYPE %s (not drawn while hideUtilities)", low);
        if (k.surf || k.lift) Log("SURFACE type %s: %s", low, k.lift ? "small thing, lifted onto furniture" : (k.surf == 1 ? "table top" : "seat"));
        k.animal = k.person && strcmp(low, "dog") == 0;
        if (nclass.size() > 4096) { nclass.clear(); lastDef = nullptr; lastC = nullptr; }
        C = &(nclass[def] = k);
    }
    char low[64];
    memcpy(low, C->low, sizeof low);
    paiso_tree_note = C->tree ? 1 : 0;
    if (id == 0) {
        static std::set<std::string> z;
        if (z.size() < 16 && z.insert(low).second) Log("SPRITE0 %s drawn with sprite 0 (the accountant model)", low);
    }
    if (C->person) {
        gObjPerson = true;
        if (id > 0 && id <= 0x366 && (strcmp(low, "lawyer") == 0 || (!gStaffModelId && strcmp(low, "foreman") == 0))) gStaffModelId = id;
    }
    gObjSnapType = C->snap;
    gObjTV = C->tv;
    gObjShopFront = C->shop;
    gObjServing = C->serving;
    gServObj = C->serving ? (uint8_t*)paiso_regs[4] : nullptr;
    gObjSurface = C->surf;
    gObjLift = C->lift;
    gObjRot90 = C->rot90;
    gObjLight = C->light;
    gObjUtil = C->util;
    gObjStack = strcmp(C->low, "stack") == 0;
    if (gNotePosOk && !gInWallPass && strcmp(C->low, "visitortablesecure") == 0) {      // 1.9.108: a booth stands in the wall line: walls beside it run to it, gaps between booths are filled
        MarkDoorTiles(gNotePos[0], gNotePos[1]);

    }
    gIngOnTop = false;
    if (gNotePosOk && !gInWallPass && strcmp(C->low, "ingredients") == 0) gIngOnTop = SurfHeightAt(gNotePos[0], gNotePos[1]) > 0.05f;      // 1.9.126: on a cooker / table: a pan
    if (gNotePosOk && !gInWallPass && ((strcmp(C->low, "ingredients") == 0 && (gIngOnTop || !gCfg.ingredientCrates)) || (strstr(C->low, "ingredient") && strstr(C->low, "cook")))) {      // 1.9.106: a pot here
        if (gPotFrame != gSwapCount) {      /* 1.9.115: per real frame (buffer swap), not per wall pass */ gPotPrev.swap(gPotCur); gPotCur.clear(); gPotFrame = gSwapCount; }
        float px = gNotePos[0], py = gNotePos[1];
        float ph = SurfHeightAt(px, py);
        // carried by a cook: the person was noted just before, standing on the same spot - held at waist height
        if (ph < 0.05f && gPersonPosOk && (unsigned)(gNoteSerial - gPersonNote) <= 24 && fabsf(gPersonPos[0] - px) < 0.4f && fabsf(gPersonPos[1] - py) < 0.4f) ph = 0.42f;
        if (gPotCur.size() < 2000) gPotCur.push_back({px, py, ph});
        int hx = (int)floor(px * 2.0f), hy = (int)floor(py * 2.0f);      // its flat sprite is not extruded as well (DrawSmallSprites skips model spots)
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) gThingTilesCur.insert((hy + dy) * 65536 + (hx + dx));
    }
    gObjDetector = strstr(C->low, "metaldetector") != nullptr || strstr(C->low, "doorframe") != nullptr;      // 1.9.93: thickness follows the walls, like doors
    gObjStructural = !C->person && (strstr(C->low, "gate") || strstr(C->low, "fence") || strstr(C->low, "bars") || strstr(C->low, "barrier") || strstr(C->low, "window"));
    gObjNoteTiny = !C->person && !C->door && !C->light && !C->serving && !C->tree;      // may be skipped when tiny on screen (CullOffscreen)
    {      // 1.9.82 diagnostic: which kinds of object fill the frame (25,000 draws a frame on a 500-prisoner save): every 20 s, per frame
        C->cnt++;
        static double nextT = 0; static int lastSwap = 0; static int lines = 0; static int checkedSwap = -1;
        if (!gCfg.diagLines || checkedSwap == gSwapCount) goto objtypesDone;      // 1.9.123: the clock is read once a frame, and only for the log
        checkedSwap = gSwapCount;
        double now = NowSec();
        if (nextT == 0) { nextT = now + 20; lastSwap = gSwapCount; }
        else if (now >= nextT && lines < 12) {
            int frames = std::max(1, gSwapCount - lastSwap);
            std::vector<std::pair<unsigned, const char*>> v;
            unsigned tot = 0;
            for (auto& kv : nclass) { if (kv.second.cnt) v.push_back({kv.second.cnt, kv.second.low}); tot += kv.second.cnt; }
            std::sort(v.begin(), v.end(), [](const std::pair<unsigned, const char*>& a, const std::pair<unsigned, const char*>& b) { return a.first > b.first; });
            std::string s;
            char b[96];
            for (size_t i = 0; i < v.size() && i < 18; i++) { snprintf(b, sizeof b, " %s %u", v[i].second, v[i].first / frames); s += b; }
            Log("OBJTYPES per frame (%u objects noted):%s", tot / frames, s.c_str());
            for (auto& kv : nclass) kv.second.cnt = 0;
            nextT = now + 20; lastSwap = gSwapCount; lines++;
        }
    }
objtypesDone:
    gObjAnimal = C->animal;
    gObjPrisoner = C->prisoner;
    gPrisTint = 0;
    gPrisSkin = 0;
    gPrisHair = 0;
    gPrisHead = -1; gPrisScale = 1.0f; gPrisBio = nullptr; gPrisObj = nullptr;
    if (gObjPrisoner &&
        gCfg.uniformTints) {      // security level: Prisoner+0xA34 (1 MinSec .. 7 Insane), +0xA4C naked; colours: world+0x4928 table, RGBA bytes
        uint8_t* obj = (uint8_t*)(gNotePath == 2 ? paiso_regs[1] : paiso_regs[4]);      // 1.9.86: draw path 2 holds the object in rbx (the position search found rbx+0x48)
        uint8_t* app = App();
        const uint64_t pv = (uint64_t)(gExe + 0xb09478);
        if (!(Readable(obj, 0xa50) &&
              *(uint64_t*)obj == pv))      // rsi holds the prisoner on the usual path; other draw paths keep it in another register
            for (int r = 0; r < 15; r++) {
                uint8_t* q = (uint8_t*)paiso_regs[r];
                if (r != 4 && (uintptr_t)q >= 0x10000 && (uintptr_t)q < 0x7FFFFFFF0000ull && !((uintptr_t)q & 7) && Readable(q, 0xa50) && *(uint64_t*)q == pv) {
                    obj = q;
                    static int rl = 0;
                    if (rl < 10) {
                        rl++;
                        Log("PRISTINT prisoner found in register %d", r);
                    }
                    break;
                }
            }
        gPrisStat[0]++;
        if (app && Readable(obj, 0xa50) && *(uint64_t*)obj == pv) {
            gPrisObj = obj;
            int cat = *(int*)(obj + 0xa34);
            bool naked = obj[0xa4c] != 0;
            uint8_t* world = *(uint8_t**)(app + 0x198);
            if (cat >= 0 && cat < 16 &&
                Readable(world, 0x4930)) {      // naked prisoners too: the 2D view shows them in uniform, and so do their models now
                uint32_t* tbl = *(uint32_t**)(world + 0x4928);
                if (Readable(tbl, (size_t)(cat + 1) * 4)) gPrisTint = tbl[cat] | 0x01000000u;      // never 0, so "found" is clear even for black
            }
            if (gPrisTint)
                gPrisStat[1]++;
            else
                gPrisStat[3]++;
            // variety: the prisoner's Bio (HeadType +0x68, BodyType +0x6C, BodyScale +0x70, SkinColour +0x74); +0xA20 was wrong, so FindBio looks for it
            uint8_t* bio = gCfg.prisonerVariety ? FindBio(obj) : nullptr;
            if (bio) {
                gPrisBio = bio;
                BioDump(bio);
                int head = *(int*)(bio + 0x68);
                { float bs = *(float*)(bio + 0x70); if (bs > 0.5f && bs < 1.6f) gPrisScale = bs; }
                const uint8_t* sc = bio + 0x74;
                // the colour's byte order is not known for sure: skin always has R >= G >= B, so take the reading that does
                uint8_t a0 = sc[0], a1 = sc[1], a2 = sc[2], b0 = sc[3], b1 = sc[2], b2 = sc[1];
                bool first = (a0 >= a1 && a1 >= a2) || !(b0 >= b1 && b1 >= b2);
                uint8_t r = first ? a0 : b0, g = first ? a1 : b1, b = first ? a2 : b2;
                if (r || g || b) gPrisSkin = 0x01000000u | (r << 16) | (g << 8) | b;
                if (head >= 0 && head < (int)(sizeof kHeadCell / sizeof kHeadCell[0]) && kHeadCell[head] >= 0) gPrisHead = head;      // HeadType = head sprite index (392 = Head7)
                if (gPrisHead >= 0 && kHeadHair[kHeadCell[gPrisHead]]) gPrisHair = 0x01000000u | kHeadHair[kHeadCell[gPrisHead]];
                if (gPrisVar < 24) {
                    gPrisVar++;
                    Log("PRISVAR bio %p head %d bodytype %d scale %.2f skin bytes %02x %02x %02x %02x -> %06x hair %06x", bio, head,
                        *(int*)(bio + 0x6c), *(float*)(bio + 0x70), sc[0], sc[1], sc[2], sc[3], gPrisSkin & 0xffffff, gPrisHair & 0xffffff);
                }
            }
            if (naked && gPrisSkin && gCfg.nakedSkin) {      // the 2D view draws a naked prisoner as bare skin (user 7 Nov): the "uniform" takes the skin colour
                uint32_t s = gPrisSkin;      // 0x01RRGGBB -> the colour table's layout (R in the low byte)
                gPrisTint = 0x01000000u | ((s & 255) << 16) | (((s >> 8) & 255) << 8) | ((s >> 16) & 255);
            }
            if (gPrisLog < 30) {
                gPrisLog++;
                Log("PRISTINT cat %d naked %d colour %08x", cat, (int)naked, gPrisTint);
            }
        } else {
            gPrisStat[2]++;
            if (gPrisBad < 8) {
                gPrisBad++;
                Log("PRISTINT object at %p is not a prisoner (vtable %llx)", obj,
                    obj && Readable(obj, 8) ? (unsigned long long)*(uint64_t*)obj : 0ull);
            }
        }
    }
    VariantDiag(low);
    if (C->bench) {
        gObjBench = true;
        strncpy(gBenchName, low, 63);
    }
    gObjDoor = C->door;
    // equipment shown in 2D as small overlay sprites (cuffs, baton, taser, gun), hidden in 3D: note it for DrawEquipment.
    // Person +0x2F8 Equipment, +0x30A Shackled; Guard +0xBC8 SecondaryEquipment; the other armed class +0xB48 (field registration)
    // (a person can be noted twice before its body is drawn, the second time with another object in the register: keep what the good read found)
    // (not cleared by other objects' notes: a guard's baton / taser is noted as its own object between the guard and its body draw)
    if (gObjPerson && gCfg.showEquipment) {
        uint8_t* o = (uint8_t*)(gNotePath == 2 ? paiso_regs[1] : paiso_regs[4]);      // 1.9.87: draw path 2 (every object on the big save) holds the object in rbx, not rsi
        if (Readable(o, 0xc10)) {
            int prim = *(int*)(o + 0x2f8);
            bool sane = prim >= 0 && prim < 4096;
            EquipNote keep = gEquip; gEquip = {0, false, false, false, false, def};
            bool readOk = false;      // 1.9.103: a prisoner read from its own object stands even when not shackled (it took the last prisoner's cuffs)
            // prisoners: the object is often in another register (PRISSTAT: over half not in rsi); the tint code found it
            if (gObjPrisoner) {
                // 1.9.103 (user: cuffs on the wrong people): the register's own object first; the tint code's object only if it stands here
                // (1.9.111: no position check - +0x48 is not a prisoner's position, every record was thrown away and no cuffs showed; the tint
                // code's object is this prisoner's - the uniform colours are right - and the wrong-people cuffs came from the carry-over, removed)
                uint8_t* po = (*(uint64_t*)o == (uint64_t)(gExe + 0xb09478)) ? o : gPrisObj;
                if (po) { gEquip.cuffs = po[0x30a] == 1; o = po; readOk = true; }
            }
            else if (sane && strcmp(low, "guard") == 0) {
                int sec = *(int*)(o + 0xbc8);
                gEquip.baton = prim != 0; gEquip.tazer = sec > 0 && sec < 4096;
            } else if (sane && (strstr(low, "armed") || strstr(low, "soldier") || strstr(low, "sniper") || strstr(low, "elite") || strstr(low, "riot"))) {
                gEquip.gun = prim != 0 && !strstr(low, "riot"); gEquip.baton = prim != 0 && strstr(low, "riot") != nullptr;
                gEquip.tazer = *(int*)(o + 0xbc8) == 37 || *(int*)(o + 0xb48) == 37;      // 1.9.104 (user: armed guard's taser not shown): 37 = the guards' taser
            }
            if (gEquip.cuffs || gEquip.baton || gEquip.tazer || gEquip.gun) gEquip.serial = gNoteSerial;
            else if (!readOk && !gObjPrisoner && keep.serial && keep.def == def && (unsigned)(gNoteSerial - keep.serial) <= 16) gEquip = keep;      // this read failed: the earlier one of the SAME person type stands (a cook after a guard got the guard's baton)
            static int el = 0, cl = 0;
            if (gEquip.cuffs && cl < 10) { cl++; Log("EQUIP prisoner shackled -> cuffs (object %p)", o); }
            if (el < 20 && (gEquip.serial || strcmp(low, "guard") == 0)) { el++; Log("EQUIP %s: equipment %d, secondary %d / %d, shackled %d -> cuffs %d baton %d taser %d gun %d", low, prim, *(int*)(o + 0xbc8), *(int*)(o + 0xb48), (int)o[0x30a], gEquip.cuffs, gEquip.baton, gEquip.tazer, gEquip.gun); }
        }
    }
    // the last person noted: the body is often drawn after other notes (baton, taser, cuffs are their own objects), which clear gObjPerson
    if (C->person && !gObjAnimal) {
        bool again = (unsigned)(gNoteSerial - gPersonNote) <= 16;      // noted twice before its body: a failed second read keeps the first's skin
        gPersonNote = gNoteSerial;
        gPersonPosOk = gNotePosOk && gNotePath && gEarlyOn[gNotePath];      // where the person stands (checked position source only)
        gPersonPos[0] = gNotePos[0]; gPersonPos[1] = gNotePos[1];
        gPersonPrisoner = gObjPrisoner;
        {      // 1.9.111 (user: only prisoners, staff, guards and workers have hands - not visitors, lawyers, civilians...)
            static const char* kHandsWho[] = {"prisoner", "guard", "doghandler", "riot", "soldier", "sniper", "elite", "cook", "doctor", "medic",
                                              "workman", "janitor", "gardener", "foreman", "psychologist", "accountant", "chief", "warden",
                                              "teacher", "spiritual", "librarian", "staff", "worker", "fire", "trooper"};
            gPersonHands = false;
            for (const char* w : kHandsWho) if (strstr(low, w)) { gPersonHands = true; break; }
        }
        if (gPrisSkin || !again) gPersonSkin = gPrisSkin;
    }
    // double doors: the game slides the whole model sideways like a single door. Read the door object (Pos +0x48/+0x4C, OpenDir +0x260/+0x264,
    // Open +0x274, from its field registration) so ShiftDoor can put it back in its frame and part the two leaves instead.
    gDoorSplitOK = false;
    // 1.9.127 (user: saloon door slides open as one): the DLC SaloonDoor is a double door (sprite d_SaloonDoubleDoor, two swing leaves) without "double" in its name
    if (gObjDoor && gCfg.splitDoubleDoors && (strstr(C->low, "double") || strstr(C->low, "saloon"))) {      // 1.9.98: only real double doors; a large jail door is one 2-tile leaf that slides (it was being split down the middle)
        uint8_t* d = (uint8_t*)(gNotePath == 2 ? paiso_regs[1] : paiso_regs[4]);      // 1.9.92: path 2 holds the object in rbx (double doors slid as one slab)
        if (Readable(d, 0x280)) {
            float px = *(float*)(d + 0x48), py = *(float*)(d + 0x4c), op = *(float*)(d + 0x274);
            int dx = *(int*)(d + 0x260), dy = *(int*)(d + 0x264);
            if (px > 0 && px < 2000 && py > 0 && py < 2000 && op >= 0.0f && op <= 1.0f && (abs(dx) == 2 || abs(dy) == 2) && abs(dx) <= 2 && abs(dy) <= 2) {
                gDoorSplitOK = true; gDoorPos[0] = px; gDoorPos[1] = py; gDoorOpen = op;
            }
            static int dl = 0;
            if (dl < 12) { dl++; Log("Double door %s: pos %.2f,%.2f open %.2f dir %d,%d -> %s", low, px, py, op, dx, dy, gDoorSplitOK ? "split" : "left as the game draws it"); }
        }
    }
}
extern "C" void paiso_miss(void* def, long long id) {
    static int shown = 0;
    if (shown >= 40 || !def || !Readable(def, 0x20)) return;
    uint8_t* p = (uint8_t*)def;
    const char* nm = (*(uint64_t*)(p + 0x18) >= 0x10) ? *(const char**)p : (const char*)p;
    if (!Readable(nm, 4)) return;
    char low[64];
    int i = 0;
    for (; i < 63 && nm[i] && Readable(nm + i, 1); i++) low[i] = (char)tolower((unsigned char)nm[i]);
    low[i] = 0;
    static std::set<std::string> seen;
    if (!seen.insert(low).second) return;
    shown++;
    Log("NOMODEL type %s has sprite id %lld and no model in the extra table", low, id);
}
extern "C" int paiso_is_accountant(void* def) {
    if (!def || !Readable(def, 0x20)) return 0;
    uint8_t* p = (uint8_t*)def;
    const char* nm = (*(uint64_t*)(p + 0x18) >= 0x10) ? *(const char**)p : (const char*)p;
    if (!Readable(nm, 11)) return 0;
    return _strnicmp(nm, "accountant", 11) == 0;
}
// 1.9.126: a sprite's id by its name in the game's own sprite-name map (the spellings tried in turn); 0 = not found
static int SpriteIdByName(const char* const* names, int n) {
    typedef int (*PFN_Find)(void*, const char*);
    PFN_Find find = (PFN_Find)(gExe + 0x116fa0);
    uint8_t* app = App();
    if (!app || !Readable(app + 0x770, 0x20)) return 0;
    int32_t* ids = *(int32_t**)(app + 0x778);
    for (int t = 0; t < n; t++) {
        int ix = find(app + 0x770, names[t]);
        if (ix >= 0 && ids && Readable(ids + ix, 4) && ids[ix] > 0) return ids[ix];
    }
    return 0;
}
extern "C" int paiso_def_to_id(void* def) {
    if (gNoteSkip) return -3;      // off screen (early cull): the stub draws nothing for it
    if (!def || !Readable(def, 0x20)) return 0;
    uint8_t* p = (uint8_t*)def;
    const char* nm = (*(uint64_t*)(p + 0x18) >= 0x10) ? *(const char**)p : (const char*)p;
    if (!Readable(nm, 4)) return 0;
    char low[64];
    int i = 0;
    for (; i < 63 && nm[i] && Readable(nm + i, 1); i++) low[i] = (char)tolower((unsigned char)nm[i]);
    low[i] = 0;
    static int calls = 0, shown = 0, benchLogs = 0;
    calls++;
    unsigned long long nh = 1469598103934665603ull;
    for (const char* q = low; *q; q++) nh = (nh ^ (unsigned char)*q) * 1099511628211ull;
    // 1.9.87 (user: "man as ingredients"): ingredients being cooked on a cooker came out as the accountant model; it has no model of its
    // own: draw no model (its 2D sprite then shows as the extruded cut-out, like other loose items)
    if (strcmp(low, "ingredients") == 0 && gCfg.ingredientCrates && !gIngOnTop) {
        // 1.9.126 (user: "food / ingredients still an issue 2d boxes"): a box of raw ingredients takes the game's own stock crate model
        // (ingredient1.obj, which loads as sprite Ingredient1) instead of its flat sprite
        static int crate = -1;
        if (crate < 0) {
            static const char* names[3] = {"Ingredient1", "ingredient1", "Ingredients1"};
            crate = SpriteIdByName(names, 3);
            Log("Ingredient boxes: drawn with the stock crate model (sprite %d)%s", crate, crate > 0 ? "" : " - not found, flat sprite kept");
        }
        if (crate > 0) return crate;
    }
    if ((strstr(low, "ingredient") && strstr(low, "cook")) || strcmp(low, "ingredients") == 0) {      // 1.9.106: "ingredients" (a pan of food) too: drawn as a pot (DrawQueuedEquip)
        static int il = 0;
        if (il < 3) { il++; Log("DEF2ID %s: no model (was drawn as a person) - its sprite shows instead", low); }
        return -3;
    }
    // 1.9.126 (user: "some dogs no 3d model, so use original dog model instead"): only the guard-dog sprites have models (the game cannot match
    // dog.obj to its plain Dog sprite). A dog drawn with any other sprite gets the guard dog; the guard dog's own poses (sitting, sleeping,
    // digging) keep theirs.
    if (strcmp(low, "dog") == 0) {
        static int dogIds[4] = {-1, -1, -1, -1};
        if (dogIds[0] < 0) {
            static const char* names[4][3] = {{"GuardDog", "guardDog", "guarddog"}, {"GuardDogSitting", "guardDogSitting", "guarddogsitting"},
                                              {"GuardDogSleeping", "guardDogSleeping", "guarddogsleeping"}, {"GuardDogDigging", "guardDogDigging", "guarddogdigging"}};
            for (int k = 0; k < 4; k++) dogIds[k] = SpriteIdByName(names[k], 3);
            Log("Dogs: guard dog sprite %d (sitting %d, sleeping %d, digging %d); a dog drawn with any other sprite gets the guard dog model", dogIds[0],
                dogIds[1], dogIds[2], dogIds[3]);
        }
        if (dogIds[0] <= 0) return 0;      // not found: the game's own choice
        for (int k = 0; k < 4; k++)
            if (dogIds[k] > 0 && gNoteSpriteId == dogIds[k]) return 0;      // a guard-dog sprite: it has its model
        static int dl = 0;
        if (dl < 4) { dl++; Log("Dog drawn with sprite %lld (no model): guard dog model used", gNoteSpriteId); }
        return dogIds[0];
    }
    // A bare tree carries no base-game sprite (index 0 = the accountant, which drew it as a person). Its art lives in a later bank the game has no
    // 3D slot for, so draw it with the spooky-tree slot, whose model file is my bare tree. Other trees keep their own sprite.
    if (strcmp(low, "tree") == 0) {
        {
            static int tl = 0, tl2 = 0;
            if (gNotePath == 2 && tl2 < 24) {      // 1.9.103: path 2 still drew bare trees as men - what its registers hold for trees
                tl2++;
                Log("BARETREE2 rax %llx rbx %llx rcx %llx rdx %llx rsi %llx rdi %llx r12 %llx r13 %llx r14 %llx r15 %llx", (unsigned long long)paiso_regs[0],
                    (unsigned long long)paiso_regs[1], (unsigned long long)paiso_regs[2], (unsigned long long)paiso_regs[3], (unsigned long long)paiso_regs[4],
                    (unsigned long long)paiso_regs[5], (unsigned long long)paiso_regs[11], (unsigned long long)paiso_regs[12], (unsigned long long)paiso_regs[13],
                    (unsigned long long)paiso_regs[14]);
            }
            if (tl < 16) {
                tl++;
                Log("BARETREE def tree: path %d rax %llx r11 %llx r15 %llx rsi %llx -> %s", gNotePath, (unsigned long long)paiso_regs[0], (unsigned long long)paiso_regs[10],
                    (unsigned long long)paiso_regs[14], (unsigned long long)paiso_regs[4],
                    (gNotePath == 2 ? paiso_regs[14] : paiso_regs[0]) != 0 ? "own sprite" : "bare (spooky slot)");
            }
        }
        if ((gNotePath == 2 ? paiso_regs[14] : paiso_regs[0]) != 0) return 0;      // 1.9.87: path 2 carries the sprite index in r15 (bare trees showed as men)
        static int bareId = -1;
        if (bareId < 0) {
            typedef int (*PFN_Find)(void*, const char*);
            PFN_Find find = (PFN_Find)(gExe + 0x116fa0);
            uint8_t* app = App();
            bareId = 0;
            if (app && Readable(app + 0x770, 0x20)) {
                int ix = find(app + 0x770, "d_TreeSpooky");
                int32_t* ids = *(int32_t**)(app + 0x778);
                if (ix >= 0 && ids && Readable(ids + ix, 4)) bareId = ids[ix];
            }
            Log("Bare trees drawn with sprite %d (the spooky-tree slot)", bareId);
            paiso_bare_id = bareId;      // the model-load stubs use it too (they still see index 0 for bare trees)
        }
        return bareId;
    }
    static std::unordered_map<unsigned long long, int> idCache;
    {
        auto ci = idCache.find(nh);
        if (ci != idCache.end()) return ci->second;
    }
    if (strstr(low, "bench") && benchLogs < 6) {
        benchLogs++;
        int r = 0;
        for (const auto& e : kTypeBank2)
            if (strcmp(e.name, low) == 0) r = e.id;
        Log("BENCHDEF type %s -> id %d, model in table: %s", low, r, (r > 870 && r < 16384 && paiso_tbl[r]) ? "yes" : "NO");
    }
    {      // ask the game's own sprite-name map first: my built-in table came from another game build and some ids point at the wrong sprite
        typedef int (*PFN_Find)(void*, const char*);
        PFN_Find find = (PFN_Find)(gExe + 0x116fa0);
        uint8_t* app = App();
        bool inTable = false;
        for (const auto& e : kTypeBank2)
            if (strcmp(e.name, low) == 0) {
                inTable = true;
                break;
            }      // only types my table would have re-mapped: the rest keep the game's own per-object sprite (tree and sink variants)
        // 1.9.111 (user: a flood light drawn as a bare tree, then nothing): a type the game gives sprite 0 (the accountant) is looked up by its
        // DLC sprite name too (FloodLight -> d_FloodLight, whose model is installed)
        // 1.9.116 (user's kitchen: crates standing up as triangles): only the DLC name ("d_" + type), never the plain name, and never stacks /
        // ingredients (their look comes from their contents: one cached id for the type drew every crate with the same wrong model)
        bool dlcOnly = false;
        // (the "sprite 0" id is 0 for nearly every type - SPRITE0 log - so this is an explicit list, not a rule)
        if (!inTable && !gObjPerson && strcmp(low, "floodlight") == 0) {
            inTable = true;
            dlcOnly = true;
        }
        if (inTable && gCfg.liveSpriteIds && app && Readable(app + 0x770, 0x20)) {
            void* map = app + 0x770;
            int32_t* ids = *(int32_t**)(app + 0x778);
            char dn[72];
            snprintf(dn, sizeof dn, "d_%s", low);
            const char* tries[2] = {dn, low};
            for (int ti = 0; ti < (dlcOnly ? 1 : 2); ti++) {
                const char* tn = tries[ti];
                int ix = find(map, tn);
                if (ix >= 0 && ids && Readable(ids + ix, 4)) {
                    int id = ids[ix];
                    if (id > 0) {
                        static int ll = 0;
                        if (ll < 40) {
                            ll++;
                            Log("DEF2ID live %s -> %s sprite %d", low, tn, id);
                        }
                        idCache[nh] = id;
                        return id;
                    }
                }
            }
        }
    }
    for (const auto& e : kTypeBank2)
        if (strcmp(e.name, low) == 0) {
            if (shown < 12) {
                shown++;
                Log("DEF2ID %s -> sprite id %d (call %d)", low, e.id, calls);
            }
            idCache[nh] = e.id;
            return e.id;
        }
    if (strcmp(low, "accountant") == 0) {
        idCache[nh] = -2;
        return -2;
    }      // sprite index 0 is the accountant: its model lives in slot 0
    if (shown < 12 && calls < 40) {
        shown++;
        Log("DEF2ID %s -> no bank-2 id (call %d)", low, calls);
    }
    idCache[nh] = 0;
    return 0;
}
extern "C" int paiso_resolve_missing(const char* name) {
    if (!name) return -1;
    for (const auto& e : kD11_2)
        if (_stricmp(e.name, name) == 0) return e.id;
    return -1;
}
static uint8_t* AllocNear(uintptr_t addr, size_t size) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    uintptr_t gran = si.dwAllocationGranularity;
    for (uintptr_t a = (addr & ~(gran - 1)) - gran; a > addr - 0x7FFF0000ull && a > gran; a -= gran) {
        void* p = VirtualAlloc((void*)a, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (p) return (uint8_t*)p;
    }
    return nullptr;
}
static bool PatchJmp(uint8_t* tramp, uintptr_t siteRva, const unsigned char* sig, int len, void* stub) {
    if (!CheckSig(siteRva, sig, len)) return false;
    uint8_t* site = (uint8_t*)(gExe + siteRva);
    tramp[0] = 0xFF;
    tramp[1] = 0x25;
    *(uint32_t*)(tramp + 2) = 0;
    *(uint64_t*)(tramp + 6) = (uint64_t)stub;
    int64_t rel = (int64_t)tramp - (int64_t)(site + 5);
    if (rel > INT32_MAX || rel < INT32_MIN) return false;
    DWORD old;
    VirtualProtect(site, len, PAGE_EXECUTE_READWRITE, &old);
    site[0] = 0xE9;
    *(int32_t*)(site + 1) = (int32_t)rel;
    for (int i = 5; i < len; i++) site[i] = 0x90;
    VirtualProtect(site, len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, len);
    return true;
}
// raise the "sprite id <= 0x366" limit to 0x3fff (the 4 bytes of the cmp immediate at siteRva + immOff)
static bool PatchLimit(uintptr_t siteRva, const unsigned char* sig, int len, int immOff) {
    if (!CheckSig(siteRva, sig, len)) return false;
    uint8_t* p = (uint8_t*)(gExe + siteRva + immOff);
    DWORD old;
    VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &old);
    *(uint32_t*)p = 0x3fff;
    VirtualProtect(p, 4, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 4);
    return true;
}
static void InstallDlcModels() {
    static const unsigned char sLcmp[6] = {0x81, 0xff, 0x66, 0x03, 0x00, 0x00};            // 1806c2 cmp edi,0x366
    static const unsigned char sLst[5] = {0x49, 0x89, 0x44, 0xff, 0x50};                   // 1806fe mov [r15+rdi*8+0x50],rax
    static const unsigned char sBcmp[5] = {0x3d, 0x66, 0x03, 0x00, 0x00};                  // 1821cc cmp eax,0x366
    static const unsigned char sB1[6] = {0x49, 0x83, 0x7c, 0xc4, 0x50, 0x00};              // 1821da cmp [r12+rax*8+0x50],0
    static const unsigned char sB2[5] = {0x4d, 0x8b, 0x64, 0xdc, 0x50};                    // 1821fd mov r12,[r12+rbx*8+0x50]
    static const unsigned char sCcmp[7] = {0x41, 0x81, 0xfb, 0x66, 0x03, 0x00, 0x00};      // 182920 cmp r11d,0x366
    static const unsigned char sC1[6] = {0x4b, 0x83, 0x7c, 0xdf, 0x50, 0x00};              // 182930 cmp [r15+r11*8+0x50],0
    static const unsigned char sC2[6] = {0x4a, 0x83, 0x7c, 0xf8, 0x50, 0x00};              // 182950 cmp [rax+r15*8+0x50],0
    static const unsigned char sC3[5] = {0x4e, 0x8b, 0x7c, 0xf8, 0x50};                    // 182978 mov r15,[rax+r15*8+0x50]
    static const unsigned char sJ[6] = {0x0f, 0x88, 0x95, 0x00, 0x00, 0x00};               // 1806ae js 0x180749 (name not in the sprite table)
    if (!CheckSig(0x1806c2, sLcmp, 6) || !CheckSig(0x1806fe, sLst, 5) || !CheckSig(0x1821cc, sBcmp, 5) || !CheckSig(0x1821da, sB1, 6) ||
        !CheckSig(0x1821fd, sB2, 5) || !CheckSig(0x182920, sCcmp, 7) || !CheckSig(0x182930, sC1, 6) || !CheckSig(0x182950, sC2, 6) ||
        !CheckSig(0x182978, sC3, 5) || !CheckSig(0x1806ae, sJ, 6)) {
        Log("Extra models: signature mismatch, not installed");
        return;
    }
    uint8_t* t = AllocNear(gExe, 4096);
    if (!t) {
        Log("Extra models: no memory near the game, not installed");
        return;
    }
    paiso_K_cont = gExe + 0x429e3c;
    paiso_K_skip = gExe + 0x429e54;
    paiso_J_ok = gExe + 0x1806b4;
    paiso_J_have = gExe + 0x1806c2;
    paiso_J_fail = gExe + 0x180749;
    paiso_L_cont = gExe + 0x180703;
    paiso_B1_cont = gExe + 0x1821e0;
    paiso_B2_cont = gExe + 0x182202;
    paiso_C1_cont = gExe + 0x182936;
    paiso_C2_cont = gExe + 0x182956;
    paiso_C3_cont = gExe + 0x18297d;
    bool ok = true;
    ok &= PatchJmp(t + 0, 0x1806fe, sLst, 5, (void*)&paiso_stubL);
    ok &= PatchJmp(t + 16, 0x1821da, sB1, 6, (void*)&paiso_stubB1);
    ok &= PatchJmp(t + 32, 0x1821fd, sB2, 5, (void*)&paiso_stubB2);
    ok &= PatchJmp(t + 48, 0x182930, sC1, 6, (void*)&paiso_stubC1);
    ok &= PatchJmp(t + 64, 0x182950, sC2, 6, (void*)&paiso_stubC2);
    ok &= PatchJmp(t + 80, 0x182978, sC3, 5, (void*)&paiso_stubC3);
    {
        static const unsigned char sK[5] = {0x77, 0x1b, 0x48, 0x8b, 0xc8};
        ok &= PatchJmp(t + 112, 0x429e37, sK, 5, (void*)&paiso_stubK);
    }
    if (gCfg.resolveBank2)
        ok &= PatchJmp(t + 96, 0x1806ae, sJ, 6, (void*)&paiso_stubJ);      // off by default: some bank-2 objects drew the wrong model
    // raise the limits last, so no id above 870 can reach an unpatched access
    ok &= PatchLimit(0x1806c2, sLcmp, 6, 2);
    ok &= PatchLimit(0x1821cc, sBcmp, 5, 1);
    ok &= PatchLimit(0x182920, sCcmp, 7, 3);
    Log("Extra models (sprites 871..16383): %s", ok ? "installed" : "PARTIAL - some patches failed");
}
