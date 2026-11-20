// parts/10_crash_diag.inc - diagnostics: exceptions, message boxes, process exit, debug output. Part of main.cpp (one translation unit, included in order): it uses everything defined in the parts before it.
// ---------------------------------------------------------------------------
// Diagnostics: exceptions, message boxes, process exit
// ---------------------------------------------------------------------------
typedef void (*PFN_DebugOut)(const char*, ...);
static PFN_DebugOut oDebugOut = nullptr;
static void DescribeAddr(void* a, char* out, size_t n) {
    HMODULE m = nullptr;
    char name[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)a, &m) && m) {
        GetModuleFileNameA(m, name, MAX_PATH);
        const char* b = strrchr(name, '\\');
        snprintf(out, n, "%s+0x%llx", b ? b + 1 : name, (unsigned long long)((uintptr_t)a - (uintptr_t)m));
    } else
        snprintf(out, n, "%p", a);
}
static LONG CALLBACK DiagVEH(EXCEPTION_POINTERS* ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code == 0xE06D7363 || code == 0x406D1388 || code == DBG_PRINTEXCEPTION_C || code == 0x4001000A || code == 0x80000003 || code < 0x80000000)
        return EXCEPTION_CONTINUE_SEARCH;
    static volatile LONG n = 0;
    if (InterlockedIncrement(&n) > 25) return EXCEPTION_CONTINUE_SEARCH;
    char where[300];
    DescribeAddr(ep->ExceptionRecord->ExceptionAddress, where, sizeof where);
    char ret[300] = "";
    if (ep->ContextRecord && Readable((void*)ep->ContextRecord->Rsp, 8)) DescribeAddr(*(void**)ep->ContextRecord->Rsp, ret, sizeof ret);
    ULONG_PTR i0 = ep->ExceptionRecord->NumberParameters > 0 ? ep->ExceptionRecord->ExceptionInformation[0] : 0;
    ULONG_PTR i1 = ep->ExceptionRecord->NumberParameters > 1 ? ep->ExceptionRecord->ExceptionInformation[1] : 0;
    Log("EXCEPTION 0x%08lx at %s (info %llx %llx) [rsp] %s", code, where, (unsigned long long)i0, (unsigned long long)i1, ret);
    return EXCEPTION_CONTINUE_SEARCH;
}
typedef int(WINAPI* PFN_MessageBoxA)(HWND, LPCSTR, LPCSTR, UINT);
typedef void(WINAPI* PFN_ExitProcess)(UINT);
static PFN_MessageBoxA oMessageBoxA = nullptr;
static PFN_ExitProcess oExitProcess = nullptr;
static int WINAPI hkMessageBoxA(HWND h, LPCSTR text, LPCSTR cap, UINT t) {
    Log("Game MessageBox [%s]: %s", cap ? cap : "", text ? text : "");
    return oMessageBoxA(h, text, cap, t);
}
static void WINAPI hkExitProcess(UINT code) {
    char where[300];
    DescribeAddr(__builtin_return_address(0), where, sizeof where);
    Log("ExitProcess(%u) called from %s. Stack:", code, where);
    void* frames[62];
    USHORT n = RtlCaptureStackBackTrace(0, 62, frames, nullptr);
    for (USHORT i = 0; i < n; i++) {
        char w[300];
        DescribeAddr(frames[i], w, sizeof w);
        Log("   #%02u %s", i, w);
    }
    oExitProcess(code);
}
typedef void(WINAPI* PFN_ODSA)(LPCSTR);
typedef void(WINAPI* PFN_ODSW)(LPCWSTR);
static PFN_ODSA oODSA = nullptr;
static PFN_ODSW oODSW = nullptr;
static void WINAPI hkODSA(LPCSTR s) {
    if (s && !oDebugOut) {
        std::string t(s);
        while (!t.empty() && (t.back() == '\n' || t.back() == '\r')) t.pop_back();
        Log("ODS: %s", t.c_str());
    }
    oODSA(s);
}
static void WINAPI hkODSW(LPCWSTR s) {
    if (s) {
        char b[1024];
        WideCharToMultiByte(CP_UTF8, 0, s, -1, b, sizeof b, nullptr, nullptr);
        Log("GAME(W): %s", b);
    }
    oODSW(s);
}

// Inline hook on the game's printf-style debug logger (RVA 0xf26b0).
static void hkDebugOut(const char* fmt, ...) {
    char buf[4096];
    va_list a;
    va_start(a, fmt);
    vsnprintf(buf, sizeof buf, fmt ? fmt : "", a);
    va_end(a);
    size_t n = strlen(buf);
    while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
    Log("GAME: %s", buf);
    if (strstr(buf, "Loading map from '")) {
        gMenuWorld = strstr(buf, "data/premadeprisons/") != nullptr;
        Log("World is the %s", gMenuWorld ? "main-menu background" : "player's prison");
    }
    if (oDebugOut) oDebugOut("%s\n", buf);
}
static bool InstallDebugOutHook() {
    static const unsigned char sig[20] = {0x48, 0x89, 0x4C, 0x24, 0x08, 0x48, 0x89, 0x54, 0x24, 0x10,
                                          0x4C, 0x89, 0x44, 0x24, 0x18, 0x4C, 0x89, 0x4C, 0x24, 0x20};
    uint8_t* target = (uint8_t*)(gExe + 0xf26b0);
    if (!CheckSig(0xf26b0, sig, sizeof sig)) return false;
    // trampoline: original 20 bytes + jmp back
    uint8_t* tr = (uint8_t*)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tr) return false;
    memcpy(tr, sig, 20);
    tr[20] = 0xFF;
    tr[21] = 0x25;
    *(uint32_t*)(tr + 22) = 0;
    *(uint64_t*)(tr + 26) = (uint64_t)(target + 20);
    oDebugOut = (PFN_DebugOut)tr;
    DWORD old;
    VirtualProtect(target, 20, PAGE_EXECUTE_READWRITE, &old);
    target[0] = 0xFF;
    target[1] = 0x25;
    *(uint32_t*)(target + 2) = 0;
    *(uint64_t*)(target + 6) = (uint64_t)&hkDebugOut;
    for (int i = 14; i < 20; i++) target[i] = 0x90;
    VirtualProtect(target, 20, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, 20);
    return true;
}
