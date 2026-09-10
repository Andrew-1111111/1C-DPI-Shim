#include "dpi_shim.h"
#include "dpi_hooks.h"
#include "dpi_math.h"
#include "io.h"
#include "logging.h"
#include "process_utils.h"

#include <ShellScalingApi.h>
#include <stdio.h>
#include <string.h>

namespace {

ShimConfig g_config;
HMODULE g_module = nullptr;
using GetDpiForSystemFn = UINT(WINAPI*)();
using GetThreadDpiAwarenessContextFn = HANDLE(WINAPI*)();
using GetWindowDpiAwarenessContextFn = HANDLE(WINAPI*)(HWND);
using GetAwarenessFromDpiAwarenessContextFn = DPI_AWARENESS(WINAPI*)(HANDLE);
using GetProcessDpiAwarenessFn = HRESULT(WINAPI*)(HANDLE, PROCESS_DPI_AWARENESS*);
using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);

GetDpiForSystemFn g_GetDpiForSystem = nullptr;
GetThreadDpiAwarenessContextFn g_GetThreadDpiAwarenessContext = nullptr;
GetWindowDpiAwarenessContextFn g_GetWindowDpiAwarenessContext = nullptr;
GetAwarenessFromDpiAwarenessContextFn g_GetAwarenessFromDpiAwarenessContext = nullptr;
GetProcessDpiAwarenessFn g_GetProcessDpiAwareness = nullptr;
GetDpiForMonitorFn g_GetDpiForMonitor = nullptr;
using SetThreadDpiAwarenessContextFn = DPI_AWARENESS_CONTEXT(WINAPI*)(DPI_AWARENESS_CONTEXT);
SetThreadDpiAwarenessContextFn g_SetThreadDpiAwarenessContext = nullptr;

void BindDpiApis() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        g_GetDpiForSystem = reinterpret_cast<GetDpiForSystemFn>(GetProcAddress(user32, "GetDpiForSystem"));
        g_GetThreadDpiAwarenessContext = reinterpret_cast<GetThreadDpiAwarenessContextFn>(
            GetProcAddress(user32, "GetThreadDpiAwarenessContext"));
        g_GetWindowDpiAwarenessContext = reinterpret_cast<GetWindowDpiAwarenessContextFn>(
            GetProcAddress(user32, "GetWindowDpiAwarenessContext"));
        g_GetAwarenessFromDpiAwarenessContext = reinterpret_cast<GetAwarenessFromDpiAwarenessContextFn>(
            GetProcAddress(user32, "GetAwarenessFromDpiAwarenessContext"));
        g_SetThreadDpiAwarenessContext = reinterpret_cast<SetThreadDpiAwarenessContextFn>(
            GetProcAddress(user32, "SetThreadDpiAwarenessContext"));
    }
    HMODULE shcore = GetModuleHandleW(L"SHCore.dll");
    if (shcore) {
        g_GetProcessDpiAwareness = reinterpret_cast<GetProcessDpiAwarenessFn>(
            GetProcAddress(shcore, "GetProcessDpiAwareness"));
        g_GetDpiForMonitor = reinterpret_cast<GetDpiForMonitorFn>(
            GetProcAddress(shcore, "GetDpiForMonitor"));
    }
}

UINT QueryRealSystemDpi() {
    DPI_AWARENESS_CONTEXT oldCtx = nullptr;
    if (g_SetThreadDpiAwarenessContext) {
        oldCtx = g_SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }

    UINT dpi = 0;
    if (g_GetDpiForMonitor) {
        POINT pt = {0, 0};
        HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
        UINT x = 0;
        UINT y = 0;
        if (SUCCEEDED(g_GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &x, &y)) && x != 0) {
            dpi = x;
        }
    }
    if (dpi == 0 && g_GetDpiForSystem) {
        dpi = g_GetDpiForSystem();
    }
    if (dpi == 0) {
        HDC hdc = GetDC(nullptr);
        if (hdc) {
            const int x = GetDeviceCaps(hdc, LOGPIXELSX);
            if (x > 0) {
                dpi = static_cast<UINT>(x);
            }
            ReleaseDC(nullptr, hdc);
        }
    }

    if (g_SetThreadDpiAwarenessContext && oldCtx) {
        g_SetThreadDpiAwarenessContext(oldCtx);
    }
    return dpi != 0 ? dpi : 96;
}

void JoinPath(wchar_t* out, size_t outCch, const wchar_t* dir, const wchar_t* file) {
    wcsncpy_s(out, outCch, dir, _TRUNCATE);
    const size_t n = wcslen(out);
    if (n > 0 && out[n - 1] != L'\\' && out[n - 1] != L'/') {
        wcsncat_s(out, outCch, L"\\", _TRUNCATE);
    }
    wcsncat_s(out, outCch, file, _TRUNCATE);
}

void DirName(wchar_t* path) {
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) {
        slash = wcsrchr(path, L'/');
    }
    if (slash) {
        *slash = 0;
    }
}

bool IsAbsolutePath(const wchar_t* path) {
    if (!path || path[0] == 0) {
        return false;
    }
    if (path[0] == L'\\' || path[0] == L'/') {
        return true;
    }
    if (((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) && path[1] == L':') {
        return true;
    }
    return false;
}

void MakeInstallPath(HMODULE module, const wchar_t* fileName, wchar_t* out, size_t outCch) {
    wchar_t dir[MAX_PATH] = {};
    GetModuleFileNameW(module, dir, MAX_PATH);
    DirName(dir);
    JoinPath(out, outCch, dir, fileName);
}

void ResolveLogPath(HMODULE module, const wchar_t* requested, wchar_t* out, size_t outCch) {
    if (!requested || requested[0] == 0) {
        MakeInstallPath(module, L"1c-dpi-shim.log", out, outCch);
        return;
    }
    if (IsAbsolutePath(requested)) {
        wcsncpy_s(out, outCch, requested, _TRUNCATE);
        return;
    }
    wchar_t dir[MAX_PATH] = {};
    GetModuleFileNameW(module, dir, MAX_PATH);
    DirName(dir);
    JoinPath(out, outCch, dir, requested);
}

void ResolveIniPath(HMODULE module, wchar_t* out, size_t outCch) {
    wchar_t envIni[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"ONEC_DPI_INI", envIni, MAX_PATH) > 0 && IO::FileExists(envIni)) {
        wcsncpy_s(out, outCch, envIni, _TRUNCATE);
        return;
    }

    wchar_t dllDir[MAX_PATH] = {};
    GetModuleFileNameW(module, dllDir, MAX_PATH);
    DirName(dllDir);
    wchar_t candidate[MAX_PATH] = {};
    JoinPath(candidate, MAX_PATH, dllDir, L"1c-dpi.ini");
    if (IO::FileExists(candidate)) {
        wcsncpy_s(out, outCch, candidate, _TRUNCATE);
        return;
    }

    wchar_t appdata[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH) > 0) {
        JoinPath(candidate, MAX_PATH, appdata, L"1C-DPI-Shim\\1c-dpi.ini");
        if (IO::FileExists(candidate)) {
            wcsncpy_s(out, outCch, candidate, _TRUNCATE);
            return;
        }
    }

    wcsncpy_s(out, outCch, dllDir, _TRUNCATE);
    wcsncat_s(out, outCch, L"\\1c-dpi.ini", _TRUNCATE);
}

int ReadInt(const wchar_t* ini, const wchar_t* key, int defaultValue) {
    return GetPrivateProfileIntW(L"shim", key, defaultValue, ini);
}

void ReadString(const wchar_t* ini, const wchar_t* key, wchar_t* out, size_t outCch, const wchar_t* defaultValue) {
    GetPrivateProfileStringW(L"shim", key, defaultValue, out, static_cast<DWORD>(outCch), ini);
}

void LoadConfig(HMODULE module) {
    g_config = ShimConfig{};
    GetModuleFileNameW(module, g_config.dllPath, MAX_PATH);
    ResolveIniPath(module, g_config.iniPath, MAX_PATH);

    const bool iniExists = IO::FileExists(g_config.iniPath);
    g_config.enabled = ReadInt(g_config.iniPath, L"enabled", 1) != 0;
    g_config.logEnabled = ReadInt(g_config.iniPath, L"log", 1) != 0;
    g_config.verboseLog = ReadInt(g_config.iniPath, L"log_verbose", 1) != 0;
    g_config.scaleSystemMetrics = ReadInt(g_config.iniPath, L"scale_system_metrics", 0) != 0;
    g_config.scaleNonClientMetrics = ReadInt(g_config.iniPath, L"scale_nonclient_metrics", 0) != 0;
    g_config.scaleStockFonts = ReadInt(g_config.iniPath, L"scale_stock_fonts", 1) != 0;
    g_config.blockPerMonitor = ReadInt(g_config.iniPath, L"block_per_monitor", 1) != 0;
    g_config.dpiPercent = ReadInt(g_config.iniPath, L"dpi", 225);

    wchar_t envDpi[32] = {};
    if (GetEnvironmentVariableW(L"ONEC_DPI", envDpi, 32) > 0) {
        const int v = _wtoi(envDpi);
        if (v >= 50 && v <= 400) {
            g_config.dpiPercent = v;
        }
    }
    wchar_t envEnabled[16] = {};
    if (GetEnvironmentVariableW(L"ONEC_DPI_ENABLED", envEnabled, 16) > 0) {
        g_config.enabled = _wtoi(envEnabled) != 0;
    }
    wchar_t envLogEnabled[16] = {};
    if (GetEnvironmentVariableW(L"ONEC_DPI_LOG_ENABLED", envLogEnabled, 16) > 0) {
        g_config.logEnabled = _wtoi(envLogEnabled) != 0;
    }

    if (g_config.dpiPercent < 50 || g_config.dpiPercent > 400) {
        g_config.dpiPercent = 225;
    }
    g_config.virtualDpi = Shim_PercentToDpi(g_config.dpiPercent);

    wchar_t requestedLog[MAX_PATH] = {};
    ReadString(g_config.iniPath, L"log_path", requestedLog, MAX_PATH, L"1c-dpi-shim.log");
    wchar_t envLog[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"ONEC_DPI_LOG", envLog, MAX_PATH) > 0) {
        wcsncpy_s(requestedLog, envLog, _TRUNCATE);
    }
    ResolveLogPath(module, requestedLog, g_config.logPath, MAX_PATH);

    (void)iniExists;
}

void LogStartupBanner() {
    wchar_t exe[MAX_PATH] = {};
    Proc_GetExePath(exe, MAX_PATH);
    wchar_t name[MAX_PATH] = {};
    Proc_GetExeName(name, MAX_PATH);

    char awareness[128] = {};
    Shim_DescribeAwareness(awareness, sizeof(awareness));

    LOG_INFO("========== 1C DPI Shim loaded ==========");
    LOG_INFO("pid=%lu architecture=%s", Proc_CurrentPid(), Proc_ArchitectureName());
    Log_WriteW(L"process=%s", name);
    Log_WriteW(L"exe=%s", exe);
    Log_WriteW(L"dll=%s", g_config.dllPath);
    Log_WriteW(L"ini=%s", g_config.iniPath);
    LOG_INFO("allowed_process=%s starter=%s platform=%s",
             Proc_IsAllowedTarget() ? "yes" : "no",
             Proc_IsStarterProcess() ? "yes" : "no",
             Proc_IsPlatformProcess() ? "yes" : "no");
    LOG_INFO("enabled=%s dpi_percent=%d system_dpi=%u (%d%%) virtual_dpi=%u (%d%%)",
             g_config.enabled ? "yes" : "no",
             g_config.dpiPercent,
             g_config.systemDpi, Shim_DpiToPercent(g_config.systemDpi),
             g_config.virtualDpi, Shim_DpiToPercent(g_config.virtualDpi));
    LOG_INFO("awareness=%s", awareness);
    LOG_INFO("scale_system_metrics=%s scale_nonclient_metrics=%s scale_stock_fonts=%s block_per_monitor=%s",
             g_config.scaleSystemMetrics ? "yes" : "no",
             g_config.scaleNonClientMetrics ? "yes" : "no",
             g_config.scaleStockFonts ? "yes" : "no",
             g_config.blockPerMonitor ? "yes" : "no");
    LOG_INFO("spoof_active=%s", Shim_ShouldSpoof() ? "yes" : "no");
    if (Proc_IsStarterProcess()) {
        LOG_INFO("1cestart.exe is a trampoline. DPI spoofing is NOT applied here.");
        LOG_INFO("CreateProcess will inject this DLL into child 1cv8*.exe processes.");
    }
}

} // namespace

ShimConfig& Shim_Config() { return g_config; }
HMODULE Shim_Module() { return g_module; }

UINT Shim_PercentToDpi(int percent) {
    return DpiMath_PercentToDpi(percent);
}

int Shim_DpiToPercent(UINT dpi) {
    return DpiMath_DpiToPercent(dpi);
}

UINT Shim_VirtualDpi() { return g_config.virtualDpi; }
UINT Shim_SystemDpi() { return g_config.systemDpi; }

bool Shim_ShouldSpoof() {
    if (!g_config.enabled) {
        return false;
    }
    if (!Proc_IsPlatformProcess()) {
        return false;
    }
    if (g_GetThreadDpiAwarenessContext && g_GetAwarenessFromDpiAwarenessContext) {
        HANDLE ctx = g_GetThreadDpiAwarenessContext();
        if (g_GetAwarenessFromDpiAwarenessContext(ctx) == DPI_AWARENESS_UNAWARE) {
            return false;
        }
    }
    return true;
}

UINT Shim_SpoofDpi(UINT originalDpi) {
    if (!Shim_ShouldSpoof()) {
        return originalDpi;
    }
    return g_config.virtualDpi;
}

int Shim_ScaleFromSystem(int value) {
    if (!Shim_ShouldSpoof() || g_config.systemDpi == 0 || g_config.virtualDpi == 0) {
        return value;
    }
    return DpiMath_Scale(value, g_config.systemDpi, g_config.virtualDpi);
}

const char* Shim_AwarenessName(HANDLE context) {
    if (!g_GetAwarenessFromDpiAwarenessContext) {
        return "unknown";
    }
    switch (g_GetAwarenessFromDpiAwarenessContext(context)) {
    case DPI_AWARENESS_UNAWARE: return "unaware";
    case DPI_AWARENESS_SYSTEM_AWARE: return "system";
    case DPI_AWARENESS_PER_MONITOR_AWARE: return "per-monitor";
    default: return "invalid";
    }
}

void Shim_DescribeAwareness(char* out, size_t outBytes) {
    if (!out || outBytes == 0) {
        return;
    }
    out[0] = 0;
    PROCESS_DPI_AWARENESS pda = PROCESS_DPI_UNAWARE;
    if (g_GetProcessDpiAwareness && SUCCEEDED(g_GetProcessDpiAwareness(nullptr, &pda))) {
        const char* n = "unaware";
        if (pda == PROCESS_SYSTEM_DPI_AWARE) n = "system";
        else if (pda == PROCESS_PER_MONITOR_DPI_AWARE) n = "per-monitor";
        sprintf_s(out, outBytes, "process=%s", n);
    } else {
        sprintf_s(out, outBytes, "process=unknown");
    }
    if (g_GetThreadDpiAwarenessContext) {
        char extra[64] = {};
        sprintf_s(extra, " thread=%s", Shim_AwarenessName(g_GetThreadDpiAwarenessContext()));
        strcat_s(out, outBytes, extra);
    }
}

BOOL DpiShim_OnAttach(HMODULE module) {
    g_module = module;
    if (!Proc_IsAllowedTarget()) {
        return TRUE;
    }

    BindDpiApis();
    LoadConfig(module);
    g_config.systemDpi = QueryRealSystemDpi();
    if (g_config.systemDpi == 0) {
        g_config.systemDpi = 96;
    }

    Log_Initialize(g_config.logPath, g_config.logEnabled, g_config.verboseLog);
    LogStartupBanner();

    if (!Hooks_Install()) {
        LOG_INFO("ERROR: failed to install hooks");
        return TRUE;
    }
    return TRUE;
}

void DpiShim_OnDetach(bool processExit) {
    if (processExit) {
        Log_Shutdown();
        return;
    }
    Hooks_Uninstall();
    LOG_INFO("1C DPI Shim unloaded");
    Log_Shutdown();
}
