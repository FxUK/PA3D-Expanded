// parts/08_input_menu.inc - hotkeys, window procedure, the ImGui mod menu. Part of main.cpp (one translation unit, included in order): it uses everything defined in the parts before it.
// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
static bool IsoLive() { return gCfg.isoEnabled && Is3DActive(); }

static void RotateBy(double deg) {
    gCam.yawTarget += deg;
    double a = fmod(gCam.yawTarget, 360.0);
    if (a < 0) a += 360.0;
    Toast("View %.0f\xC2\xB0", a);
}
static void CycleWalls() {
    int v = WallViewIndex();
    v = (v >= kWallViews - 1) ? 0 : v + 1;
    SetWallView(v);      // Walls Up -> Cutaway Far -> Cutaway Close -> Cutaway All -> Walls Down -> Walls Up
    Toast("%s", WallViewName(v));
}
static void ToggleIso() {
    gCfg.isoEnabled = !gCfg.isoEnabled;
    Toast(gCfg.isoEnabled ? "Isometric view ON" : "Isometric view OFF");
}

static RECT gWallBtn = {0, 0, 0, 0};      // the wall-view button's screen rectangle (set while it is drawn)
static LRESULT CALLBACK HookWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    {      // the wall-view button: a click on it is ours, the game never sees it
        static bool downOnBtn = false;
        if ((msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) && gWallBtn.right > gWallBtn.left) {
            POINT pt = {(short)LOWORD(lp), (short)HIWORD(lp)};
            if (PtInRect(&gWallBtn, pt)) {
                downOnBtn = true;
                CycleWalls();
                return 0;
            }
        }
        if (msg == WM_LBUTTONUP && downOnBtn) {
            downOnBtn = false;
            return 0;
        }
    }
    bool keyMsg = msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || msg == WM_CHAR;
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        bool repeat = (lp & (1 << 30)) != 0;
        int vk = (int)wp;
        if (!repeat) {
            if (vk == VK_F11 && !repeat) {
                gTransFramesLeft = 3;
                Toast("Capturing draws for the next 3 frames");
                return 0;
            }
            if (vk == gCfg.keyMenu) {
                gMenuOpen = !gMenuOpen;
                return 0;
            }
            if (vk == gCfg.keyToggle) {
                ToggleIso();
                return 0;
            }
            if (vk == gCfg.keyWalls && IsoLive()) {
                CycleWalls();
                return 0;
            }
        }
        bool menuTyping = gMenuOpen && gImguiReady && ImGui::GetIO().WantCaptureKeyboard;
        if (!menuTyping && IsoLive()) {
            if (vk == gCfg.keyRotL) {
                if (!repeat) RotateBy(-gCfg.rotStepDeg);
                if (gCfg.consumeKeys) return 0;
            }
            if (vk == gCfg.keyRotR) {
                if (!repeat) RotateBy(+gCfg.rotStepDeg);
                if (gCfg.consumeKeys) return 0;
            }
        }
    }
    if (gCfg.consumeKeys && IsoLive() && (msg == WM_KEYUP || msg == WM_CHAR)) {
        int vk = (int)wp;
        if (msg == WM_CHAR) vk = toupper((int)wp);
        if (vk == gCfg.keyRotL || vk == gCfg.keyRotR) return 0;
    }
    if (gMenuOpen && gImguiReady) {
        ImGui_ImplWin32_WndProcHandler(h, msg, wp, lp);
        ImGuiIO& io = ImGui::GetIO();
        bool mouseMsg = msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST;
        if (mouseMsg && io.WantCaptureMouse) return 0;
        if (keyMsg && io.WantCaptureKeyboard) return 0;
    }
    return CallWindowProcW(gOrigWndProc, h, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Menu
// ---------------------------------------------------------------------------
static const char* KeyName(int vk) {
    static char buf[8][32];
    static int i = 0;
    i = (i + 1) & 7;
    UINT sc = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    if (!GetKeyNameTextA((LONG)(sc << 16), buf[i], 32)) snprintf(buf[i], 32, "VK %d", vk);
    return buf[i];
}

static void DrawMenu() {
    ImGui::SetNextWindowSize(ImVec2(430, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(40, 80), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("PA3D Expanded", &gMenuOpen)) {
        ImGui::End();
        return;
    }

    if (!gBuildOK) {
        ImGui::TextColored(ImVec4(1, .35f, .3f, 1), "Unsupported game build - camera hooks disabled.");
        ImGui::TextWrapped("This mod targets v1.02 r11056 (Steam build 1838). Shader styling still works.");
        ImGui::Separator();
    }
    bool in3d = Is3DActive();
    ImGui::Text("Game 3D renderer: %s   Shaders patched: %d", in3d ? "ON" : "off", gShadersPatched);

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Isometric camera", &gCfg.isoEnabled);
        ImGui::SameLine();
        ImGui::Checkbox("Auto-enable 3D mode", &gCfg.force3D);
        ImGui::SliderFloat("Viewing angle", &gCfg.pitchDeg, 15.0f, 89.0f, "%.1f\xC2\xB0");
        if (ImGui::SmallButton("True iso 35.3\xC2\xB0")) gCfg.pitchDeg = 35.264f;
        ImGui::SameLine();
        if (ImGui::SmallButton("2:1 pixel 30\xC2\xB0")) gCfg.pitchDeg = 30.0f;
        ImGui::SameLine();
        if (ImGui::SmallButton("Steep 55\xC2\xB0")) gCfg.pitchDeg = 55.0f;
        ImGui::SameLine();
        if (ImGui::SmallButton("Top-down")) gCfg.pitchDeg = 89.0f;
        float yaw = (float)fmod(fmod(gCam.yawTarget, 360.0) + 360.0, 360.0);
        if (ImGui::SliderFloat("Rotation", &yaw, 0.0f, 359.0f, "%.0f\xC2\xB0")) {
            gCam.yawTarget = yaw;
            gCam.yaw = yaw;
        }
        ImGui::SliderFloat("WASD pan speed", &gCfg.panSpeed, 0.5f, 4.0f, "%.2fx");
        ImGui::Checkbox("Lower people lying down to the floor", &gCfg.snapLying);
        ImGui::Checkbox("Upright speech bubbles", &gCfg.billboardBubbles);
        ImGui::Checkbox("Hide flat sun shadows", &gCfg.hideSunShadows);
        ImGui::Checkbox("Hide windows / lights / signs when their wall is cut", &gCfg.hideCutFittings);

        ImGui::Checkbox("Own procedural floor", &gCfg.ownFloor);
        ImGui::TextDisabled("Bubbles find their speaker by themselves (1.9.119)");
        ImGui::SliderFloat("Bubble height", &gCfg.bubbleLift, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Bubble size", &gCfg.bubbleScale, 0.3f, 3.0f, "%.2f");
        ImGui::SliderFloat("Lying snap limit (tiles)", &gCfg.snapMax, 0.05f, 1.0f, "%.2f");
        ImGui::Checkbox("Perspective camera (off = isometric)", &gCfg.perspective);
        ImGui::SliderFloat("Field of view", &gCfg.fovDeg, 15.0f, 90.0f, "%.0f\xC2\xB0");
        ImGui::SliderFloat("Lowest tilt (keeps sky out)", &gCfg.minPitchDeg, 10.0f, 60.0f, "%.0f\xC2\xB0");
        ImGui::TextDisabled("Middle mouse drag = free rotate / tilt");
        ImGui::SliderFloat("Q/E step", &gCfg.rotStepDeg, 15.0f, 90.0f, "%.0f\xC2\xB0");
        ImGui::SliderFloat("Rotate speed", &gCfg.rotSpeed, 2.0f, 40.0f, "%.0f");
        ImGui::SliderFloat("Zoom scale", &gCfg.zoom, 0.3f, 2.5f, "%.2f");
        ImGui::Checkbox("Pan relative to view", &gCfg.rotateScroll);
        ImGui::SameLine();
        ImGui::Checkbox("Mirror Y", &gCfg.flipY);
    }
    if (ImGui::CollapsingHeader("Walls", ImGuiTreeNodeFlags_DefaultOpen)) {
        {
            int wv = WallViewIndex(), wv0 = wv;
            ImGui::RadioButton("Up", &wv, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Cutaway far (near 3/4 of screen)", &wv, 1);
            ImGui::SameLine();
            ImGui::RadioButton("Cutaway close (near half)", &wv, 2);
            ImGui::RadioButton("Cutaway all", &wv, 3);
            ImGui::SameLine();
            ImGui::RadioButton("Down", &wv, 4);
            if (wv != wv0) SetWallView(wv);
        }
        ImGui::Checkbox("Sims-style (rooms behind)", &gCfg.roomCut);
        ImGui::SameLine();
        ImGui::TextDisabled(gOutdoorsTex ? "(room map OK)" : "(room map not found: cursor mode)");
        ImGui::Text("Cutaway follows:");
        ImGui::SameLine();
        ImGui::RadioButton("Mouse", &gCfg.focusMode, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Screen centre", &gCfg.focusMode, 1);
        ImGui::SliderFloat("Cutaway radius", &gCfg.cutRadius, 0.0f, 40.0f, gCfg.cutRadius <= 0 ? "unlimited" : "%.0f tiles");
        ImGui::SliderFloat("Cutaway wall height", &gCfg.stub, 0.05f, 0.9f, "%.2f");
        ImGui::SliderFloat("Walls down height", &gCfg.downStub, 0.02f, 0.6f, "%.2f");
        ImGui::SliderFloat("Wall base lift (fix floating walls)", &gCfg.wallLift, -2.0f, 2.0f, "%.3f");
        ImGui::SliderFloat("Wall thickness (tiles)", &gCfg.wallThick, 0.3f, 1.0f, "%.2f");
        ImGui::Checkbox("Auto-ground walls and doors (ignores the manual lift)", &gCfg.wallAutoGround);
        ImGui::SliderFloat("Grime", &gCfg.grit, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("White floor brightness", &gCfg.floorBright, 0.5f, 1.0f, "%.2f");
        ImGui::SliderFloat("Warm lighting", &gCfg.warm, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Night darkness", &gCfg.nightDark, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Object edge lines", &gCfg.outline, 0.0f, 1.0f, "%.2f");
        ImGui::SameLine();
        ImGui::TextDisabled("(now %.0f%% night)", gNight * 100.0f);
        ImGui::SliderFloat("Outline round objects", &gCfg.silhouette, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Sitting: lower people on seats", &gCfg.seatDrop, 0.0f, 0.4f, "%.2f");
        ImGui::Checkbox("Lamp light pools", &gCfg.bulbPools);
        ImGui::SameLine();
        ImGui::Checkbox("Show every small sprite (extruded)", &gCfg.spriteFallback);
        ImGui::Checkbox("Furniture shadows", &gCfg.objectShadows);
        ImGui::SameLine();
        ImGui::Checkbox("People shadows", &gCfg.personShadows);
        ImGui::ColorEdit3("Wall tint", gCfg.wallTint, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
        if (ImGui::TreeNode("Calibration")) {
            ImGui::SliderFloat("Wall height (world)", &gCfg.wallHeight, 0.1f, 300.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
            bool inv = gCfg.upSign > 0;
            if (ImGui::Checkbox("Camera: world up is +Z", &inv)) gCfg.upSign = inv ? 1.0f : -1.0f;
            ImGui::TreePop();
        }
    }
    if (ImGui::CollapsingHeader("Look", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Cartoon shading", &gCfg.cartoon);
        ImGui::SliderFloat("Cel bands", &gCfg.toon, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Painted textures", &gCfg.style, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Colour boost", &gCfg.sat, 0.5f, 2.0f, "%.2f");
    }
    if (ImGui::CollapsingHeader("Keys")) {
        ImGui::Text("Rotate: %s / %s   Menu: %s", KeyName(gCfg.keyRotL), KeyName(gCfg.keyRotR), KeyName(gCfg.keyMenu));
        ImGui::Text("Toggle iso: %s   Cycle walls: %s", KeyName(gCfg.keyToggle), KeyName(gCfg.keyWalls));
        ImGui::Checkbox("Hide Q/E from the game", &gCfg.consumeKeys);
        ImGui::TextDisabled("Rebind in PA3D Expanded\\PA3D Expanded.ini (virtual-key codes).");
    }
    if (ImGui::CollapsingHeader("Diagnostics")) {
        ImGui::Text("Game eye    %.2f %.2f %.2f", gCam.gameEye[0], gCam.gameEye[1], gCam.gameEye[2]);
        ImGui::Text("Game center %.2f %.2f %.2f", gCam.gameCenter[0], gCam.gameCenter[1], gCam.gameCenter[2]);
        ImGui::Text("Game up     %.2f %.2f %.2f", gCam.gameUp[0], gCam.gameUp[1], gCam.gameUp[2]);
        ImGui::Text("Iso target  %.2f %.2f   focus %.2f %.2f", gCam.T[0], gCam.T[1], gCam.focus[0], gCam.focus[1]);
        {
            int n = 0;
            for (int i = 871; i < 16384; i++)
                if (paiso_tbl[i]) n++;
            gDlcLoaded = n;
        }
        ImGui::Text("Programs patched: %d   Extra models loaded: %d", (int)gPatchedPrograms.size(), gDlcLoaded);
        ImGui::Checkbox("Verbose log", &gCfg.verboseLog);
        ImGui::Checkbox("Diagnostic log lines", &gCfg.diagLines);
        if (ImGui::Button("Log camera samples")) gCam.samplesLogged = 0;
        ImGui::TextWrapped("Hide 2D layers: type a texture id (3, 8, 14, 28 were the busy ones) and watch the floor strips under the walls.");
        ImGui::InputInt("Hide texture A", &gCfg.hideTexA);
        ImGui::InputInt("Hide texture B", &gCfg.hideTexB);
        if (ImGui::Button(gLayerTest ? "Stop layer test" : "Layer test (watch what disappears)")) {
            gLayerTest = !gLayerTest;
            gLayerList.clear();
            gLayerIdx = -1;
            gLayerNext = 0;
            if (gLayerTest)
                gHideBeforeTest = gCfg.hideTexA;
            else
                gCfg.hideTexA = gHideBeforeTest;
        }
        ImGui::TextDisabled(
            "Hides each 2D texture in turn for 4 s: note the number when the strips / steam vanish, then type it above and Save settings.");
    }
    ImGui::Separator();
    if (ImGui::Button("Save settings")) {
        gCfg.Save(gIni);
        Toast("Settings saved");
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        gCfg.Load(gIni);
        Toast("Settings reloaded");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("PA3D Expanded %s", kVersion);
    ImGui::End();
}

// On-screen wall-view button, left of the game's clock (top right). Drawn with ImGui; clicks are caught in the window procedure.
static void DrawWallButton() {
    gWallBtn = RECT{0, 0, 0, 0};
    if (!gCfg.wallButton || !IsoLive() || gMenuWorld) return;
    ImGuiIO& io = ImGui::GetIO();
    float w = 150.0f, h = 30.0f, x = io.DisplaySize.x - gCfg.wallBtnRight - w, y = gCfg.wallBtnTop;
    gWallBtn = RECT{(LONG)x, (LONG)y, (LONG)(x + w), (LONG)(y + h)};
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    POINT pt;
    bool hover = false;
    if (gWnd && GetCursorPos(&pt) && ScreenToClient(gWnd, &pt)) hover = PtInRect(&gWallBtn, pt) != 0;
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), hover ? IM_COL32(70, 70, 70, 235) : IM_COL32(30, 30, 30, 215), 4.0f);
    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(200, 200, 200, 160), 4.0f);
    char lab[48];
    snprintf(lab, sizeof lab, "Walls: %s", WallViewName(WallViewIndex()) + (WallViewIndex() == 0 || WallViewIndex() == 4 ? 6 : 0));
    ImVec2 ts = ImGui::CalcTextSize(lab);
    dl->AddText(ImVec2(x + (w - ts.x) * 0.5f, y + (h - ts.y) * 0.5f), IM_COL32(240, 240, 240, 255), lab);
}
static void DrawToast() {
    if (NowSec() > gToastUntil || !gToast[0]) return;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, 60), ImGuiCond_Always, ImVec2(0.5f, 0));
    ImGui::SetNextWindowBgAlpha(0.7f);
    ImGui::Begin("##paiso_toast", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::TextUnformatted(gToast);
    ImGui::End();
}

static void InitImGui(HWND hwnd) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    static std::string imguiIni = gDir + "PA3D Expanded\\imgui.ini";
    io.IniFilename = imguiIni.c_str();
    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 2;
    st.FrameRounding = 2;
    ImVec4* c = st.Colors;
    c[ImGuiCol_TitleBgActive] = ImVec4(0.30f, 0.27f, 0.18f, 1);
    c[ImGuiCol_Header] = ImVec4(0.35f, 0.31f, 0.20f, 0.8f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.45f, 0.40f, 0.25f, 0.9f);
    c[ImGuiCol_CheckMark] = ImVec4(0.95f, 0.80f, 0.25f, 1);
    c[ImGuiCol_SliderGrab] = ImVec4(0.85f, 0.70f, 0.25f, 1);
    c[ImGuiCol_Button] = ImVec4(0.35f, 0.31f, 0.20f, 0.9f);
    c[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.19f, 0.16f, 0.9f);
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL2_Init();
    gImguiReady = true;
    Log("ImGui initialised on HWND %p", hwnd);
}
