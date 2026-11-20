// parts/09_frame.inc - per-frame work in SwapBuffers, the extra-model reload. Part of main.cpp (one translation unit, included in order): it uses everything defined in the parts before it.
// ---------------------------------------------------------------------------
// SwapBuffers: per-frame work + overlay
// ---------------------------------------------------------------------------
// Sprites from the later banks (objects_d11_2: benches, gates, gym gear...) are not registered yet when the game first loads
// its 3D models, so their shape files find no sprite. Once the world is drawing, run the game's own loader again.
static void* gRendererSelf = nullptr;
static int gWallFramesSeen = 0;
static bool gModelsReloaded = false;
static int gRefresh3D = 0;
// the player's world has been loaded for 2 s (its later sprite banks registered): only then is the game's 3D switched on (one model load)
static bool WorldSettled() {
    static double since = 0;
    bool ok = WorldLoaded() && !gMenuWorld;
    if (!ok) { since = 0; return false; }
    double now = NowSec();
    if (since == 0) since = now;
    return now - since >= 2.0;
}
static void MaybeReloadModels() {
    if (gModelsReloaded || !gRendererSelf || gWallFramesSeen < 150 || !gCfg.extraModels || !gBuildOK) return;
    gModelsReloaded = true;
    {      // 1.9.92 (user: three long pauses loading in): 3D now goes on only once the world has loaded, so the game's own first load may
           // already have the later banks' models - then the second full load (3+ s) is not needed
        int have = 0;
        for (int i = 871; i < 16384; i++) if (paiso_tbl[i]) have++;
        if (have >= 50) {
            gDlcLoaded = have;
            Log("Model reload skipped: the first load already holds %d extra models", have);
            return;
        }
    }
    void** tbl = (void**)((uint8_t*)gRendererSelf + 0x50);
    int base = 0;
    if (Readable(tbl, 871 * 8))
        for (int i = 0; i < 871; i++)
            if (tbl[i]) base++;
    Log("Model reload: renderer %p holds %d base-game models", gRendererSelf, base);
    {      // diagnostic: how does the game's sprite-name table answer?
        typedef int (*PFN_Find)(void*, const char*);
        uint8_t* app = App();
        PFN_Find find = (PFN_Find)(gExe + 0x116fa0);
        if (app && Readable(app + 0x770, 0x20)) {
            void* map = app + 0x770;
            int cap = *(int*)((uint8_t*)map + 0x14);
            int32_t* ids = *(int32_t**)(app + 0x778);
            Log("SPRITEMAP capacity=%d ids=%p", cap, (void*)ids);
            const char* names[] = {"tree",
                                   "d_baretree",
                                   "baretree",
                                   "d_treesnowyconifer",
                                   "treesnowyconifer",
                                   "d_treeyoungsnowyconifer",
                                   "d_treespooky",
                                   "d_treesakura",
                                   "sink",
                                   "kitchensink",
                                   "kitchensink2",
                                   "d_kitchensink",
                                   "doctor",
                                   "prisonerbus",
                                   "d_prisonerletter",
                                   "bench",
                                   "Bench",
                                   "d_oakbench",
                                   "d_OakBench",
                                   "d_battery",
                                   "d_Battery",
                                   "d_benchfullrotate",
                                   "d_BenchFullRotate",
                                   "d_gate",
                                   "d_Gate",
                                   "sheetmetal",
                                   "light",
                                   "d_floodlight",
                                   "d_smalltable"};
            for (const char* nm : names) {
                int ix = find(map, nm);
                int id = (ix >= 0 && ids && Readable(ids + ix, 4)) ? ids[ix] : -999;
                Log("SPRITEMAP  %-22s slot=%d spriteId=%d", nm, ix, id);
            }
        }
    }
    if (base < 50) {
        Log("Model reload skipped: that does not look like the model owner");
        return;
    }
    typedef void (*PFN_Load3D)(void*);
    ((PFN_Load3D)(gExe + 0x180560))(gRendererSelf);
    int n = 0;
    for (int i = 871; i < 16384; i++)
        if (paiso_tbl[i]) n++;
    gDlcLoaded = n;
    Log("Model reload done: %d extra models loaded", n);
    gRefresh3D =
        90;      // people are only drawn after the game re-reads its 3D state (what F9 off/on did by hand): flip the flag once the reload is done
}
static BOOL WINAPI hkSwapBuffers(HDC dc) {
    gProjGen++;      // a new frame (and the menu's own drawing): read the projection again
    gMvGen++;        // and the modelview (the menu's ImGui backend sets its own)
    if (gMvMiss == 1) {
        gMvMiss = 2;
        Log("WARN modelview cache: a matrix change was missed; reading it on every draw from now on");
    }
    gRdB = gRdE = 0;
    gRdCacheN = 0;
    gRdCacheNext = 0;
    gSwapCount++;
    {
        static double nextCap = 0;
        double nowc = NowSec();
        if (nowc >= nextCap) {
            nextCap = nowc + 0.5;
            gCaptureArmed = true;
        }
    }
    static int nSwap = 0;
    {      // frame-time watch: a hitch or a style drop shows up here with a timestamp that can be lined up with the flicker
        static double last = 0;
        static int lines = 0;
        double now = NowSec();
        if (last > 0 && now - last > 0.12 && lines < 60 && WorldLoaded() && gWallFramesSeen > 100) {
            lines++;
            Log("FRAMEHITCH %.0f ms (camera active %d, world loaded %d)", (now - last) * 1000.0, (int)gCam.active, (int)WorldLoaded());
        }
        static bool wasActive = true;
        if (wasActive != gCam.active && lines < 40) {
            lines++;
            Log("CAMSTATE active %d", (int)gCam.active);
        }
        wasActive = gCam.active;
        last = now;
        static int missLines = 0;
        if (gCam.active && gWallFramesSeen > 200 && missLines < 40 && (gWallPassesFrame == 0 || (gCfg.ownFloor && gFloorDrawsFrame == 0))) {
            missLines++;
            Log("FRAMEMISS wallPasses %d floorDraws %d (this frame)", gWallPassesFrame, gFloorDrawsFrame);
        }
        gWallPassesFrame = 0;
        gFloorDrawsFrame = 0;
        static double perfT0 = 0;
        if (perfT0 == 0) perfT0 = NowSec();
        gPerf.frames++;
        static int sampled = 0;
        if (gPerfOn) sampled++;
        gPerfOn = (gPerf.frames % 8) == 0;
        if (NowSec() - perfT0 >= 5.0 && gPerf.frames > 0) {
            int nf = gPerf.frames;
            int ns = sampled > 0 ? sampled : 1;
            sampled = 0;
            perfT0 = NowSec();
            Log("PERF %d frames in 5 s (%.1f fps); per frame ms: hideCut %.3f bubble %.3f wallSnap %.3f lying %.3f wallPiece %.3f items %.3f cull %.3f notes %.3f total %.3f | hook %.2f (%u draws) gpuWait %.2f outline %.3f frame %.1f", nf,
                nf / 5.0, gPerf.t[0] / ns, gPerf.t[1] / ns, gPerf.t[2] / ns, gPerf.t[3] / ns, gPerf.t[4] / ns, gPerf.t[5] / ns, gPerf.t[6] / ns, gPerf.t[7] / ns,
                (gPerf.t[0] + gPerf.t[1] + gPerf.t[2] + gPerf.t[3] + gPerf.t[4] + gPerf.t[5] + gPerf.t[6] + gPerf.t[7]) / ns, gPerf.t[8] / ns, gPerf.draws / (unsigned)ns, gPerf.t[9] / ns, gPerf.t[11] / ns, 5000.0 / (nf > 0 ? nf : 1));
            gPerf.draws = 0;
            for (double& v : gPerf.t) v = 0;
            gPerf.frames = 0;
            if (gPrisStat[0])
                Log("PRISSTAT last 5 s: prisoner notes %u, coloured %u, object not found %u, no colour %u, tinted draws %u, no tint uniform %u",
                    gPrisStat[0], gPrisStat[1], gPrisStat[2], gPrisStat[3], gPrisStat[4], gPrisStat[5]);
            for (unsigned& v : gPrisStat) v = 0;
            if (gEquipSkip[0] + gEquipSkip[1] + gEquipSkip[2] + gEquipSkip[3] + gEquipSkip[4]) Log("EQUIPSTAT last 5 s: drawn %u, skipped: wall pass / no camera %u, not a body draw %u, note too old %u, vertex buffer %u", gEquipSkip[0], gEquipSkip[1], gEquipSkip[2], gEquipSkip[3], gEquipSkip[4]); for (unsigned& v : gEquipSkip) v = 0;
            Log("CULL %u of %u camera draws skipped as off screen (last 5 s)", gCulled, gCullSeen);
            gCulled = gCullSeen = 0;
            Log("SNAPSTAT calls %u typeOK %u arrOK %u highBaseCheck %u decided %u shifted %u notSnapType %u", gSnapStat[0], gSnapStat[1],
                gSnapStat[2], gSnapStat[3], gSnapStat[4], gSnapStat[5], gSnapStat[7]);
            for (unsigned& v : gSnapStat) v = 0;
        }
    }
    {      // diagnostic for the rapid UI flicker: what does each presented frame look like? (framebuffer, masks, scissor, draw count since the last swap)
        static unsigned lastSerial = 0;
        static std::vector<std::string> sigs;
        static std::vector<int> hist;
        static double nextDump = 0;
        GLint fbo = 0, vp[4] = {0, 0, 0, 0};
        GLboolean cm[4] = {1, 1, 1, 1};
        glGetIntegerv(0x8CA6, &fbo);
        glGetIntegerv(GL_VIEWPORT, vp);
        glGetBooleanv(GL_COLOR_WRITEMASK, cm);
        char sg[200];
        snprintf(sg, sizeof sg, "fbo %d vp %d,%d,%d,%d cmask %d%d%d%d scissor %d depth %d blend %d draws %s wallpasses %d", (int)fbo, vp[0], vp[1],
                 vp[2], vp[3], cm[0], cm[1], cm[2], cm[3], (int)glIsEnabled(GL_SCISSOR_TEST), (int)glIsEnabled(GL_DEPTH_TEST),
                 (int)glIsEnabled(GL_BLEND), (gDrawSerial - lastSerial) > 400 ? "many" : ((gDrawSerial - lastSerial) > 40 ? "some" : "few"),
                 gWallPassesFrame);
        lastSerial = gDrawSerial;
        int ix = -1;
        for (size_t i = 0; i < sigs.size(); i++)
            if (sigs[i] == sg) {
                ix = (int)i;
                break;
            }
        if (ix < 0 && sigs.size() < 24) {
            sigs.push_back(sg);
            ix = (int)sigs.size() - 1;
            Log("SWAPSIG #%d new: %s", ix, sg);
        }
        hist.push_back(ix);
        if (hist.size() > 24) hist.erase(hist.begin());
        double nw = NowSec();
        if (nw > nextDump && WorldLoaded()) {
            nextDump = nw + 10.0;
            std::string h;
            for (int v : hist) {
                h += std::to_string(v);
                h += ' ';
            }
            Log("SWAPHIST last %d frames: %s", (int)hist.size(), h.c_str());
        }
    }
    {      // diagnostic for the periodic redraw / flicker: a frame with far fewer draws or wall pieces than the recent average
        static double emaD = 0, emaW = 0;
        static unsigned lastS = 0;
        static int lines = 0;
        static double t0 = 0;
        double nowD = NowSec();
        double d = (double)(gDrawSerial - lastS);
        lastS = gDrawSerial;
        double w = (double)gDrawnPrev.size();
        if (WorldLoaded() && gCam.active) {
            if (t0 == 0) t0 = nowD;
        } else
            t0 = 0;      // the average restarts whenever the world (re)loads
        bool settled = t0 > 0 && nowD - t0 > 25.0;
        if (settled && emaD > 200 && lines < 120 && (d < emaD * 0.8 || d > emaD * 1.25 || (emaW > 20 && w < emaW * 0.85))) {
            lines++;
            Log("FRAMEDIP draws %.0f (avg %.0f) wallcells %.0f (avg %.0f)", d, emaD, w, emaW);
        }
        {
            static int skipRun = 0;
            static int skipLines = 0;
            gSkipSwap = false;
            if (gCfg.dropBadFrames && settled && emaD > 1000 && d < emaD * 0.3 && skipRun < 3) {
                gSkipSwap = true;
                skipRun++;
                if (skipLines < 40) {
                    skipLines++;
                    Log("FRAMESKIP draws %.0f (avg %.0f) - previous frame kept on screen", d, emaD);
                }
            } else
                skipRun = 0;
        }
        if (!gSkipSwap) { emaD = (emaD == 0 || !settled) ? d : emaD * 0.98 + d * 0.02; }
        emaW = (emaW == 0 || !settled) ? w : emaW * 0.98 + w * 0.02;
    }
    {      // flicker diagnostic 2: per-frame counters; a frame that differs from the running average is logged with its time
        static double ema[6] = {0, 0, 0, 0, 0, 0};
        static unsigned lastS = 0, lastE = 0, lastSnap = 0;
        static int lines = 0, frames = 0;
        static unsigned lastMask = 0;
        double v[6] = {(double)(gDrawSerial - lastS), (double)gFrFbo,    (double)gFrProgCalls, (double)(gUniEpoch - lastE),
                       (double)gDrawnPrev.size(),     (double)gFrFboMask};
        lastS = gDrawSerial;
        lastE = gUniEpoch;
        if (WorldLoaded() && gCam.active)
            frames++;
        else
            frames = 0;
        if (frames > 300 && lines < 400) {
            bool odd = false;
            for (int i = 0; i < 5; i++) {
                double d = fabs(v[i] - ema[i]);
                if (d > 3.0 && d > ema[i] * 0.3) odd = true;
            }
            if (gFrFboMask != lastMask) odd = true;
            if (odd) {
                lines++;
                Log("FRAMEODD draws %.0f (avg %.0f) fboBinds %.0f (avg %.1f) progBinds %.0f (avg %.1f) uniformResets %.0f (avg %.1f) wallcells %.0f "
                    "(avg %.0f) fboMask %08x",
                    v[0], ema[0], v[1], ema[1], v[2], ema[2], v[3], ema[3], v[4], ema[4], gFrFboMask);
            }
        }
        for (int i = 0; i < 5; i++) ema[i] = (ema[i] == 0 || frames < 300) ? v[i] : ema[i] * 0.97 + v[i] * 0.03;
        {
            static bool heldLast = false;
            bool extra =
                (gFrFboMask & (1u << 4)) !=
                0;      // the frame in which the game makes its time-lapse picture (frame buffer 4) comes out wrong: show the previous frame instead
            if (gCfg.holdExtraPassFrame && extra && !heldLast && WorldLoaded() && gCam.active) {
                gSkipSwap = true;
                heldLast = true;
                static int hl = 0;
                if (hl < 5) {
                    hl++;
                    Log("HOLDFRAME frame with the extra frame-buffer pass not shown");
                }
            } else
                heldLast = false;
        }
        lastMask = gFrFboMask;
        gFrFbo = 0;
        gFrProgCalls = 0;
        gFrFboMask = 0;
    }
    {      // flicker diagnostic 3: what reaches the screen. Two rows of the finished frame are read back; a frame much darker or brighter than the average is logged
        static std::vector<unsigned char> row;
        static double ema = 0;
        static int lines = 0, frames = 0;
        RECT rc = {0, 0, 0, 0};
        GLint fbo = 0;
        glGetIntegerv(0x8CA6, &fbo);
        if (gCfg.verboseLog && gWnd && GetClientRect(gWnd, &rc) && fbo == 0 && WorldLoaded() && gCam.active && rc.right > 64 && rc.bottom > 64) {
            int W = rc.right - rc.left, H = rc.bottom - rc.top;
            row.resize((size_t)W * 4);
            double sum = 0;
            int n = 0;
            for (int k = 1; k <= 3; k++) {
                glReadPixels(0, H * k / 4, W, 1, GL_RGBA, GL_UNSIGNED_BYTE, row.data());
                for (int x = 0; x < W; x += 4) {
                    sum += row[x * 4] * 0.3 + row[x * 4 + 1] * 0.59 + row[x * 4 + 2] * 0.11;
                    n++;
                }
            }
            double lum = n ? sum / n : 0;
            frames++;
            if (frames > 120 && lines < 300 && fabs(lum - ema) > 6.0) {
                lines++;
                Log("SCREENDIP brightness %.1f (avg %.1f)", lum, ema);
            }
            ema = (frames < 120 || ema == 0) ? lum : ema * 0.9 + lum * 0.1;
        } else
            frames = 0;
    }
    if (gTransFramesLeft >= 0) {
        static int lastLeft = -1;
        if (gTransFramesLeft != lastLeft && gCam.active) {
            lastLeft = gTransFramesLeft;
            Log("TRANSDIAG ==== swap (frames left %d) ====", gTransFramesLeft);
        }
    }
    FlushOverlays();
    MaybeReloadModels();
    {      // cell lights off during the Sleep regime: ease over ~3 s so it never snaps
        static double last = NowSec(); double now = NowSec(), dt = std::min(0.5, now - last); last = now;
        float target = (gCfg.cellLightsOff && SleepNow()) ? 1.0f : 0.0f;
        gCellsDark += (float)std::max(-dt / 3.0, std::min(dt / 3.0, (double)(target - gCellsDark)));
    }
    if (nSwap++ == 0) Log("First SwapBuffers (dc %p)", dc);
    if (!gWnd) {
        HWND w = WindowFromDC(dc);
        if (w) {
            gWnd = w;
            gOrigWndProc = (WNDPROC)SetWindowLongPtrW(w, GWLP_WNDPROC, (LONG_PTR)HookWndProc);
            Log("Subclassed game window %p", w);
            LoadGlFuncs();
            InitImGui(w);
        }
    }

    // Keep the game's hidden 3D renderer in sync with our toggle.
    if (uint8_t* app = App()) {
        uint8_t& flag = app[pa::kApp_Pseudo3dFlag];
        {
            static bool wasLoaded = false;
            static int liveFrames = 0;
            bool ld = WorldLoaded() && gCam.active;
            liveFrames = ld ? liveFrames + 1 : 0;
            if (!ld)
                wasLoaded = false;
            else if (!wasLoaded && liveFrames == 60 && gRefresh3D == 0) {
                wasLoaded = true;
                gRefresh3D = 90;
                gPrisLog = 0;
                gPrisBad = 0;
                gPrisVar = 0;
                Log("World loaded: refreshing 3D mode so people are drawn");
            } else if (liveFrames > 60)
                wasLoaded = true;
        }
        if (gGfxReload && WorldLoaded()) { gGfxReload = false; gRefresh3D = 90; }      // 1.9.128: after a resolution change
        if (gRefresh3D > 0) {
            gRefresh3D--;
            if (gRefresh3D == 85) {
                flag = 0;
                Log("Refreshing 3D mode (off)");
            } else if (gRefresh3D == 80) {
                flag = 1;
                gWeForced3D = true;      // 1.9.85: ours, so F9 (mod off) puts the game back in 2D (it stayed in the game's own 3D mode)
                Log("Refreshing 3D mode (on)");
            }
        }
        bool menu2D = gMenuWorld && !gCfg.isoOnMenu;      // 1.9.79 (user): the main menu's fly-over stays in the game's normal 2D view
        if (gRefresh3D > 80 && gRefresh3D < 86) { /* flag stays off for a few frames */
        } else if (menu2D) {
            if (flag && gWeForced3D) {
                flag = 0;
                gWeForced3D = false;
                Log("Main menu: game 2D mode");
            }
        } else if (gCfg.isoEnabled && gCfg.force3D && !flag && WorldSettled()) {
            flag = 1;
            gWeForced3D = true;
            Log("Enabled game 3D mode");
        } else if (!gCfg.isoEnabled && (gWeForced3D || gCfg.force3D) && flag) {
            flag = 0;
            gWeForced3D = false;
            Log("Restored game 2D mode");
        }
    }
    {
        int lim = WorldLoaded()
                      ? 30
                      : 2;      // tolerate frames in which the game does not re-apply the camera (a one-frame style drop looks like a flicker)
        if (++gCam.framesSinceApply > lim) {
            if (gCam.active) {
                static int dl = 0;
                if (dl < 20) {
                    dl++;
                    Log("Camera went inactive after %d frames without an update", (int)gCam.framesSinceApply);
                }
            }
            gCam.active = false;
        }
    }

    if (gImguiReady) {
        GLint prog = 0, activeTex = GL_TEXTURE0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTex);
        PFN_UseProgram use = oUseProgram ? oUseProgram : oUseProgramObjectARB;
        if (prog && use) use(0);
        if (pActiveTexture) pActiveTexture(GL_TEXTURE0);

        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::GetIO().MouseDrawCursor = gMenuOpen;
        ImGui::NewFrame();
        if (gMenuOpen) DrawMenu();
        DrawWallButton();
        DrawToast();
        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        gProjGen++;
        gMvGen++;

        if (pActiveTexture) pActiveTexture((GLenum)activeTex);
        if (prog && use) use((GLuint)prog);
    }
    if (gSkipSwap) {
        gSkipSwap = false;
        return TRUE;
    }      // a half-drawn frame (floor only): leave the last complete frame on screen
    const double sw0 = gPerfOn ? PerfNow() : 0.0;      // 1.9.123: time the swap takes = the wait for the graphics card (PERF 'gpuWait')
    BOOL swr = oSwapBuffers(dc);
    if (sw0 > 0) gPerf.t[9] += PerfNow() - sw0;
    return swr;
}
