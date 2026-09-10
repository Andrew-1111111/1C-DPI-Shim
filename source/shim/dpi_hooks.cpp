#include "dpi_hooks.h"
#include "dpi_shim.h"
#include "io.h"
#include "inject.h"
#include "logging.h"
#include "process_utils.h"

#include "MinHook.h"
#include <ShellScalingApi.h>
#include <shellapi.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

#include <stdio.h>
#include <string.h>

namespace {

using GetDeviceCapsFn = int (WINAPI*)(HDC, int);
using GetDpiForWindowFn = UINT (WINAPI*)(HWND);
using GetDpiForSystemFn = UINT (WINAPI*)();
using GetDpiForMonitorFn = HRESULT (WINAPI*)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);
using GetSystemMetricsFn = int (WINAPI*)(int);
using GetSystemMetricsForDpiFn = int (WINAPI*)(int, UINT);
using SystemParametersInfoWFn = BOOL (WINAPI*)(UINT, UINT, PVOID, UINT);
using SystemParametersInfoForDpiFn = BOOL (WINAPI*)(UINT, UINT, PVOID, UINT, UINT);
using AdjustWindowRectExForDpiFn = BOOL (WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
using SetProcessDpiAwarenessContextFn = BOOL (WINAPI*)(DPI_AWARENESS_CONTEXT);
using SetProcessDpiAwarenessFn = HRESULT (WINAPI*)(PROCESS_DPI_AWARENESS);
using SetThreadDpiAwarenessContextFn = DPI_AWARENESS_CONTEXT (WINAPI*)(DPI_AWARENESS_CONTEXT);
using GetProcessDpiAwarenessFn = HRESULT (WINAPI*)(HANDLE, PROCESS_DPI_AWARENESS*);
using CreateWindowExWFn = HWND (WINAPI*)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
using CreateProcessWFn = BOOL (WINAPI*)(LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION);
using CreateProcessAFn = BOOL (WINAPI*)(LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION);
using CreateProcessInternalWFn = BOOL (WINAPI*)(HANDLE, LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION, PHANDLE);
using LoadLibraryExWFn = HMODULE (WINAPI*)(LPCWSTR, HANDLE, DWORD);
using ShellExecuteExWFn = BOOL (WINAPI*)(SHELLEXECUTEINFOW*);
using ShellExecuteWFn = HINSTANCE (WINAPI*)(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);
using GetStockObjectFn = HGDIOBJ (WINAPI*)(int);
using CreateFontIndirectWFn = HFONT (WINAPI*)(const LOGFONTW*);
using LogicalToPhysicalPointForPerMonitorDPIFn = BOOL (WINAPI*)(HWND, LPPOINT);
using PhysicalToLogicalPointForPerMonitorDPIFn = BOOL (WINAPI*)(HWND, LPPOINT);
using GetContextDpiFn = int (__cdecl*)();

GetDeviceCapsFn Real_GetDeviceCaps = nullptr;
GetDpiForWindowFn Real_GetDpiForWindow = nullptr;
GetDpiForSystemFn Real_GetDpiForSystem = nullptr;
GetDpiForMonitorFn Real_GetDpiForMonitor = nullptr;
GetSystemMetricsFn Real_GetSystemMetrics = nullptr;
GetSystemMetricsForDpiFn Real_GetSystemMetricsForDpi = nullptr;
SystemParametersInfoWFn Real_SystemParametersInfoW = nullptr;
SystemParametersInfoForDpiFn Real_SystemParametersInfoForDpi = nullptr;
AdjustWindowRectExForDpiFn Real_AdjustWindowRectExForDpi = nullptr;
SetProcessDpiAwarenessContextFn Real_SetProcessDpiAwarenessContext = nullptr;
SetProcessDpiAwarenessFn Real_SetProcessDpiAwareness = nullptr;
SetThreadDpiAwarenessContextFn Real_SetThreadDpiAwarenessContext = nullptr;
GetProcessDpiAwarenessFn Real_GetProcessDpiAwareness = nullptr;
CreateWindowExWFn Real_CreateWindowExW = nullptr;
CreateProcessWFn Real_CreateProcessW = nullptr;
CreateProcessAFn Real_CreateProcessA = nullptr;
CreateProcessInternalWFn Real_CreateProcessInternalW = nullptr;
LoadLibraryExWFn Real_LoadLibraryExW = nullptr;
ShellExecuteExWFn Real_ShellExecuteExW = nullptr;
ShellExecuteWFn Real_ShellExecuteW = nullptr;
GetStockObjectFn Real_GetStockObject = nullptr;
CreateFontIndirectWFn Real_CreateFontIndirectW = nullptr;
LogicalToPhysicalPointForPerMonitorDPIFn Real_LogicalToPhysicalPointForPerMonitorDPI = nullptr;
PhysicalToLogicalPointForPerMonitorDPIFn Real_PhysicalToLogicalPointForPerMonitorDPI = nullptr;
GetContextDpiFn Real_getContextDPI = nullptr;

bool g_installed = false;
bool g_internalHooked = false;
HFONT g_scaledStockFonts[32] = {};
volatile LONG g_nGetDeviceCaps = 0;
volatile LONG g_nGetDpiForWindow = 0;
volatile LONG g_nGetDpiForSystem = 0;
volatile LONG g_nGetDpiForMonitor = 0;
volatile LONG g_nGetSystemMetrics = 0;
volatile LONG g_nGetSystemMetricsForDpi = 0;
volatile LONG g_nSystemParametersInfo = 0;
volatile LONG g_nCreateWindow = 0;
volatile LONG g_nGetStockObject = 0;
volatile LONG g_nCreateFont = 0;
volatile LONG g_nPointConv = 0;
volatile LONG g_nGetContextDpi = 0;
volatile LONG g_nPmaBlock = 0;

bool IsDisplayDc(HDC hdc) {
    if (!hdc || !Real_GetDeviceCaps) {
        return true;
    }
    const int tech = Real_GetDeviceCaps(hdc, TECHNOLOGY);
    return tech != DT_RASPRINTER && tech != DT_PLOTTER;
}

const char* MetricName(int index) {
    switch (index) {
    case SM_CXSCREEN: return "SM_CXSCREEN";
    case SM_CYSCREEN: return "SM_CYSCREEN";
    case SM_CXVSCROLL: return "SM_CXVSCROLL";
    case SM_CYHSCROLL: return "SM_CYHSCROLL";
    case SM_CYCAPTION: return "SM_CYCAPTION";
    case SM_CXBORDER: return "SM_CXBORDER";
    case SM_CYBORDER: return "SM_CYBORDER";
    case SM_CXICON: return "SM_CXICON";
    case SM_CYICON: return "SM_CYICON";
    case SM_CXCURSOR: return "SM_CXCURSOR";
    case SM_CYCURSOR: return "SM_CYCURSOR";
    case SM_CYMENU: return "SM_CYMENU";
    case SM_CXFULLSCREEN: return "SM_CXFULLSCREEN";
    case SM_CYFULLSCREEN: return "SM_CYFULLSCREEN";
    case SM_CXMINTRACK: return "SM_CXMINTRACK";
    case SM_CYMINTRACK: return "SM_CYMINTRACK";
    case SM_CXSMICON: return "SM_CXSMICON";
    case SM_CYSMICON: return "SM_CYSMICON";
    case SM_CYSMCAPTION: return "SM_CYSMCAPTION";
    case SM_CXICONSPACING: return "SM_CXICONSPACING";
    case SM_CYICONSPACING: return "SM_CYICONSPACING";
    case SM_CXEDGE: return "SM_CXEDGE";
    case SM_CYEDGE: return "SM_CYEDGE";
    case SM_CXPADDEDBORDER: return "SM_CXPADDEDBORDER";
    case SM_CXVIRTUALSCREEN: return "SM_CXVIRTUALSCREEN";
    case SM_CYVIRTUALSCREEN: return "SM_CYVIRTUALSCREEN";
    default: return nullptr;
    }
}

bool IsScreenSizeMetric(int index) {
    switch (index) {
    case SM_CXSCREEN:
    case SM_CYSCREEN:
    case SM_CXFULLSCREEN:
    case SM_CYFULLSCREEN:
    case SM_CXVIRTUALSCREEN:
    case SM_CYVIRTUALSCREEN:
    case SM_XVIRTUALSCREEN:
    case SM_YVIRTUALSCREEN:
    case SM_CXMAXIMIZED:
    case SM_CYMAXIMIZED:
    case SM_CMONITORS:
        return true;
    default:
        return false;
    }
}

bool IsScalableMetric(int index) {
    if (IsScreenSizeMetric(index)) {
        return false;
    }
    switch (index) {
    case SM_CXVSCROLL: case SM_CYHSCROLL: case SM_CYCAPTION:
    case SM_CXBORDER: case SM_CYBORDER: case SM_CXDLGFRAME: case SM_CYDLGFRAME:
    case SM_CYVTHUMB: case SM_CXHTHUMB: case SM_CXICON: case SM_CYICON:
    case SM_CXCURSOR: case SM_CYCURSOR: case SM_CYMENU: case SM_CYVSCROLL:
    case SM_CXHSCROLL: case SM_CXMIN: case SM_CYMIN: case SM_CXSIZE: case SM_CYSIZE:
    case SM_CXFRAME: case SM_CYFRAME: case SM_CXMINTRACK: case SM_CYMINTRACK:
    case SM_CXDOUBLECLK: case SM_CYDOUBLECLK: case SM_CXICONSPACING: case SM_CYICONSPACING:
    case SM_CXEDGE: case SM_CYEDGE: case SM_CXMINSPACING: case SM_CYMINSPACING:
    case SM_CXSMICON: case SM_CYSMICON: case SM_CYSMCAPTION: case SM_CXSMSIZE:
    case SM_CYSMSIZE: case SM_CXMENUSIZE: case SM_CYMENUSIZE:
    case SM_CXMENUCHECK: case SM_CYMENUCHECK: case SM_CXPADDEDBORDER:
        return true;
    default:
        return false;
    }
}

void DescribeHwnd(HWND hwnd, char* out, size_t outBytes) {
    if (!out || outBytes == 0) {
        return;
    }
    if (!hwnd) {
        sprintf_s(out, outBytes, "hwnd=NULL");
        return;
    }
    wchar_t cls[128] = {};
    wchar_t title[160] = {};
    GetClassNameW(hwnd, cls, 128);
    GetWindowTextW(hwnd, title, 160);
    char clsA[128] = {};
    char titleA[160] = {};
    WideCharToMultiByte(CP_UTF8, 0, cls, -1, clsA, sizeof(clsA), nullptr, nullptr);
    WideCharToMultiByte(CP_UTF8, 0, title, -1, titleA, sizeof(titleA), nullptr, nullptr);
    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);
    sprintf_s(out, outBytes, "hwnd=0x%p class=\"%s\" title=\"%s\" monitor=(%ld,%ld)-(%ld,%ld)%s",
              hwnd, clsA, titleA,
              mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right, mi.rcMonitor.bottom,
              (mi.dwFlags & MONITORINFOF_PRIMARY) ? " primary" : "");
}

bool ParseCommandImage(LPCWSTR appName, LPCWSTR cmdLine, wchar_t* out, size_t outCch) {
    if (appName && appName[0]) {
        wcsncpy_s(out, outCch, appName, _TRUNCATE);
        return true;
    }
    if (!cmdLine || !cmdLine[0] || !out || outCch == 0) {
        return false;
    }
    const wchar_t* p = cmdLine;
    while (*p == L' ' || *p == L'\t') {
        ++p;
    }
    if (*p == L'"') {
        ++p;
        size_t i = 0;
        while (*p && *p != L'"' && i + 1 < outCch) {
            out[i++] = *p++;
        }
        out[i] = 0;
        return i > 0;
    }

    // Unquoted path may contain spaces (Program Files (x86)\...\1cv8.exe).
    const wchar_t* exeExt = nullptr;
    for (const wchar_t* s = p; *s; ++s) {
        if (_wcsnicmp(s, L".exe", 4) == 0) {
            const wchar_t next = s[4];
            if (next == 0 || next == L' ' || next == L'\t' || next == L'"') {
                exeExt = s;
                break;
            }
        }
    }
    if (exeExt) {
        size_t len = static_cast<size_t>(exeExt - p) + 4;
        if (len >= outCch) {
            len = outCch - 1;
        }
        wcsncpy_s(out, outCch, p, len);
        out[len] = 0;
        return true;
    }

    size_t i = 0;
    while (*p && *p != L' ' && *p != L'\t' && i + 1 < outCch) {
        out[i++] = *p++;
    }
    out[i] = 0;
    return i > 0;
}

bool CommandMentionsOneC(LPCWSTR text) {
    if (!text || !text[0]) {
        return false;
    }
    const wchar_t* names[] = {
        L"1cestart.exe", L"1cv8.exe", L"1cv8c.exe", L"1cv8s.exe", L"1cv8a.exe"
    };
    for (const wchar_t* name : names) {
        const size_t n = wcslen(name);
        for (const wchar_t* p = text; *p; ++p) {
            if (_wcsnicmp(p, name, n) == 0) {
                return true;
            }
        }
    }
    return false;
}

bool ShouldInjectImage(LPCWSTR appName, LPCWSTR cmdLine, wchar_t* image, size_t imageCch) {
    image[0] = 0;
    ParseCommandImage(appName, cmdLine, image, imageCch);
    return Proc_IsOneCImagePath(image) || CommandMentionsOneC(appName) || CommandMentionsOneC(cmdLine);
}

bool IsAbsolutePath(LPCWSTR path) {
    if (!path || !path[0]) {
        return false;
    }
    if (path[0] == L'\\' || path[0] == L'/') {
        return true;
    }
    return ((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) && path[1] == L':';
}

void JoinDirFile(LPCWSTR dir, LPCWSTR file, wchar_t* out, size_t outCch) {
    wcsncpy_s(out, outCch, dir, _TRUNCATE);
    const size_t n = wcslen(out);
    if (n > 0 && out[n - 1] != L'\\' && out[n - 1] != L'/') {
        wcsncat_s(out, outCch, L"\\", _TRUNCATE);
    }
    wcsncat_s(out, outCch, file, _TRUNCATE);
}

bool ResolveOneCImage(LPCWSTR file, LPCWSTR dir, wchar_t* out, size_t outCch) {
    if (!file || !file[0]) {
        return false;
    }
    if (IsAbsolutePath(file)) {
        wcsncpy_s(out, outCch, file, _TRUNCATE);
        return Proc_IsOneCImagePath(out);
    }
    if (dir && dir[0]) {
        JoinDirFile(dir, file, out, outCch);
        if (IO::FileExists(out) && Proc_IsOneCImagePath(out)) {
            return true;
        }
    }
    wchar_t self[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    wchar_t* slash = wcsrchr(self, L'\\');
    if (slash) {
        *slash = 0;
        JoinDirFile(self, file, out, outCch);
        if (IO::FileExists(out) && Proc_IsOneCImagePath(out)) {
            return true;
        }
    }
    if (SearchPathW(nullptr, file, L".exe", static_cast<DWORD>(outCch), out, nullptr) > 0) {
        return Proc_IsOneCImagePath(out);
    }
    wcsncpy_s(out, outCch, file, _TRUNCATE);
    return Proc_IsOneCImagePath(out);
}

bool HasOneCFileExt(LPCWSTR path) {
    if (!path) {
        return false;
    }
    const wchar_t* ext = PathFindExtensionW(path);
    return ext && (
        _wcsicmp(ext, L".v8i") == 0 ||
        _wcsicmp(ext, L".v8cs") == 0 ||
        _wcsicmp(ext, L".1cd") == 0 ||
        _wcsicmp(ext, L".efd") == 0);
}

bool AssociatedExecutableIsOneC(LPCWSTR file, wchar_t* out, size_t outCch) {
    if (!file || !file[0] || !out) {
        return false;
    }
    DWORD len = static_cast<DWORD>(outCch);
    const HRESULT hr = AssocQueryStringW(0, ASSOCSTR_EXECUTABLE, file, nullptr, out, &len);
    return SUCCEEDED(hr) && out[0] != 0 && Proc_IsOneCImagePath(out);
}

bool PrepareShellOneCLaunch(LPCWSTR file, LPCWSTR params, LPCWSTR dir,
                            wchar_t* exe, size_t exeCch, wchar_t* outParams, size_t paramsCch) {
    exe[0] = 0;
    outParams[0] = 0;
    if (ResolveOneCImage(file, dir, exe, exeCch)) {
        if (params) {
            wcsncpy_s(outParams, paramsCch, params, _TRUNCATE);
        }
        return true;
    }
    wchar_t associated[MAX_PATH] = {};
    if (AssociatedExecutableIsOneC(file, associated, MAX_PATH) || HasOneCFileExt(file)) {
        if (!associated[0]) {
            AssociatedExecutableIsOneC(file, associated, MAX_PATH);
        }
        if (!associated[0]) {
            return false;
        }
        wcsncpy_s(exe, exeCch, associated, _TRUNCATE);
        wchar_t quoted[MAX_PATH + 4] = {};
        if (file && wcschr(file, L' ')) {
            swprintf_s(quoted, L"\"%s\"", file);
        } else {
            wcsncpy_s(quoted, file ? file : L"", _TRUNCATE);
        }
        wcsncpy_s(outParams, paramsCch, quoted, _TRUNCATE);
        if (params && params[0]) {
            wcsncat_s(outParams, paramsCch, L" ", _TRUNCATE);
            wcsncat_s(outParams, paramsCch, params, _TRUNCATE);
        }
        return true;
    }
    if (CommandMentionsOneC(file) || CommandMentionsOneC(params)) {
        if (file) {
            wcsncpy_s(exe, exeCch, file, _TRUNCATE);
        }
        if (params) {
            wcsncpy_s(outParams, paramsCch, params, _TRUNCATE);
        }
        return exe[0] != 0;
    }
    return false;
}

bool InjectChild(HANDLE process, HANDLE thread, bool resumeAfter) {
    ShimConfig& cfg = Shim_Config();
    wchar_t err[512] = {};
    const bool ok = Inject_LoadLibraryW(process, cfg.dllPath, 15000, err, 512);
    if (!ok) {
        Log_WriteW(L"child inject FAILED: %s", err);
        if (resumeAfter && thread) {
            ResumeThread(thread);
        }
        return false;
    }
    LOG_INFO("child inject OK pid=%lu", GetProcessId(process));
    if (resumeAfter && thread) {
        ResumeThread(thread);
    }
    return true;
}

int WINAPI Hook_GetDeviceCaps(HDC hdc, int index) {
    if (!Real_getContextDPI) {
        Hooks_TryLateApis();
    }
    const int original = Real_GetDeviceCaps(hdc, index);
    if (index != LOGPIXELSX && index != LOGPIXELSY) {
        return original;
    }
    if (!IsDisplayDc(hdc)) {
        return original;
    }
    const int spoofed = static_cast<int>(Shim_SpoofDpi(static_cast<UINT>(original)));
    if (Log_ShouldSample(&g_nGetDeviceCaps, 40, 500)) {
        LOG_INFO("GetDeviceCaps(%s) hdc=%p original=%d spoofed=%d spoof=%s",
                 index == LOGPIXELSX ? "LOGPIXELSX" : "LOGPIXELSY",
                 hdc, original, spoofed, Shim_ShouldSpoof() ? "yes" : "no");
    }
    return spoofed;
}

UINT WINAPI Hook_GetDpiForWindow(HWND hwnd) {
    const UINT original = Real_GetDpiForWindow ? Real_GetDpiForWindow(hwnd) : Shim_SystemDpi();
    const UINT spoofed = Shim_SpoofDpi(original);
    if (Log_ShouldSample(&g_nGetDpiForWindow, 40, 200)) {
        char desc[512] = {};
        DescribeHwnd(hwnd, desc, sizeof(desc));
        LOG_INFO("GetDpiForWindow original=%u spoofed=%u %s", original, spoofed, desc);
    }
    return spoofed;
}

UINT WINAPI Hook_GetDpiForSystem() {
    const UINT original = Real_GetDpiForSystem ? Real_GetDpiForSystem() : Shim_SystemDpi();
    const UINT spoofed = Shim_SpoofDpi(original);
    if (Log_ShouldSample(&g_nGetDpiForSystem, 20, 200)) {
        LOG_INFO("GetDpiForSystem original=%u spoofed=%u", original, spoofed);
    }
    return spoofed;
}

HRESULT WINAPI Hook_GetDpiForMonitor(HMONITOR monitor, MONITOR_DPI_TYPE type, UINT* dpiX, UINT* dpiY) {
    HRESULT hr = Real_GetDpiForMonitor ? Real_GetDpiForMonitor(monitor, type, dpiX, dpiY) : E_FAIL;
    UINT ox = dpiX ? *dpiX : 0;
    UINT oy = dpiY ? *dpiY : 0;
    if (SUCCEEDED(hr) && Shim_ShouldSpoof()) {
        const UINT v = Shim_VirtualDpi();
        if (dpiX) *dpiX = v;
        if (dpiY) *dpiY = v;
    }
    if (Log_ShouldSample(&g_nGetDpiForMonitor, 20, 200)) {
        LOG_INFO("GetDpiForMonitor type=%d original=%u,%u spoofed=%u,%u hr=0x%08lx",
                 static_cast<int>(type), ox, oy, dpiX ? *dpiX : 0, dpiY ? *dpiY : 0, hr);
    }
    return hr;
}

int WINAPI Hook_GetSystemMetrics(int index) {
    int original = Real_GetSystemMetrics(index);
    int value = original;
    if (Shim_Config().scaleSystemMetrics && IsScalableMetric(index)) {
        value = Shim_ScaleFromSystem(original);
    }
    if (Log_ShouldSample(&g_nGetSystemMetrics, 30, 400)) {
        const char* name = MetricName(index);
        if (name) {
            LOG_INFO("GetSystemMetrics(%s/%d) original=%d returned=%d", name, index, original, value);
        } else {
            LOG_INFO("GetSystemMetrics(%d) original=%d returned=%d", index, original, value);
        }
    }
    return value;
}

int WINAPI Hook_GetSystemMetricsForDpi(int index, UINT dpi) {
    UINT useDpi = dpi;
    if (Shim_ShouldSpoof() && dpi == Shim_SystemDpi()) {
        useDpi = Shim_VirtualDpi();
    }
    const int value = Real_GetSystemMetricsForDpi(index, useDpi);
    if (Log_ShouldSample(&g_nGetSystemMetricsForDpi, 20, 200)) {
        LOG_INFO("GetSystemMetricsForDpi index=%d dpi_in=%u dpi_used=%u result=%d", index, dpi, useDpi, value);
    }
    return value;
}

void ScaleLogFont(LOGFONTW* lf) {
    if (!lf) {
        return;
    }
    lf->lfHeight = Shim_ScaleFromSystem(lf->lfHeight);
    lf->lfWidth = Shim_ScaleFromSystem(lf->lfWidth);
}

BOOL WINAPI Hook_SystemParametersInfoW(UINT action, UINT uiParam, PVOID pvParam, UINT fWinIni) {
    const BOOL ok = Real_SystemParametersInfoW(action, uiParam, pvParam, fWinIni);
    if (!ok || !Shim_Config().scaleNonClientMetrics || !Shim_ShouldSpoof()) {
        if (Log_ShouldSample(&g_nSystemParametersInfo, 15, 300) &&
            (action == SPI_GETNONCLIENTMETRICS || action == SPI_GETICONTITLELOGFONT)) {
            LOG_INFO("SystemParametersInfoW action=0x%04x ok=%d scaled=no", action, ok);
        }
        return ok;
    }
    if (action == SPI_GETICONTITLELOGFONT && pvParam) {
        ScaleLogFont(static_cast<LOGFONTW*>(pvParam));
    } else if (action == SPI_GETNONCLIENTMETRICS && pvParam) {
        auto* ncm = static_cast<NONCLIENTMETRICSW*>(pvParam);
        ncm->iBorderWidth = Shim_ScaleFromSystem(ncm->iBorderWidth);
        ncm->iScrollWidth = Shim_ScaleFromSystem(ncm->iScrollWidth);
        ncm->iScrollHeight = Shim_ScaleFromSystem(ncm->iScrollHeight);
        ncm->iCaptionWidth = Shim_ScaleFromSystem(ncm->iCaptionWidth);
        ncm->iCaptionHeight = Shim_ScaleFromSystem(ncm->iCaptionHeight);
        ncm->iSmCaptionWidth = Shim_ScaleFromSystem(ncm->iSmCaptionWidth);
        ncm->iSmCaptionHeight = Shim_ScaleFromSystem(ncm->iSmCaptionHeight);
        ncm->iMenuWidth = Shim_ScaleFromSystem(ncm->iMenuWidth);
        ncm->iMenuHeight = Shim_ScaleFromSystem(ncm->iMenuHeight);
        ScaleLogFont(&ncm->lfCaptionFont);
        ScaleLogFont(&ncm->lfSmCaptionFont);
        ScaleLogFont(&ncm->lfMenuFont);
        ScaleLogFont(&ncm->lfStatusFont);
        ScaleLogFont(&ncm->lfMessageFont);
    }
    if (Log_ShouldSample(&g_nSystemParametersInfo, 15, 300)) {
        LOG_INFO("SystemParametersInfoW action=0x%04x ok=%d scaled=yes", action, ok);
    }
    return ok;
}

BOOL WINAPI Hook_SystemParametersInfoForDpi(UINT action, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi) {
    UINT useDpi = dpi;
    if (Shim_ShouldSpoof() && dpi == Shim_SystemDpi()) {
        useDpi = Shim_VirtualDpi();
    }
    return Real_SystemParametersInfoForDpi(action, uiParam, pvParam, fWinIni, useDpi);
}

BOOL WINAPI Hook_AdjustWindowRectExForDpi(LPRECT rect, DWORD style, BOOL menu, DWORD exStyle, UINT dpi) {
    UINT useDpi = dpi;
    if (Shim_ShouldSpoof() && dpi == Shim_SystemDpi()) {
        useDpi = Shim_VirtualDpi();
    }
    return Real_AdjustWindowRectExForDpi(rect, style, menu, exStyle, useDpi);
}

BOOL WINAPI Hook_SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT context) {
    LOG_INFO("SetProcessDpiAwarenessContext(%s) requested", Shim_AwarenessName(context));
    if (Proc_IsPlatformProcess() &&
        Shim_AwarenessName(context) != nullptr &&
        strcmp(Shim_AwarenessName(context), "unaware") == 0) {
        LOG_INFO("blocked process-wide UNAWARE (would enable bitmap virtualization and double-scale)");
        return TRUE;
    }
    const BOOL ok = Real_SetProcessDpiAwarenessContext(context);
    LOG_INFO("SetProcessDpiAwarenessContext result=%d", ok);
    return ok;
}

HRESULT WINAPI Hook_SetProcessDpiAwareness(PROCESS_DPI_AWARENESS value) {
    LOG_INFO("SetProcessDpiAwareness(%d)", static_cast<int>(value));
    if (Proc_IsPlatformProcess() && value == PROCESS_DPI_UNAWARE) {
        LOG_INFO("blocked process-wide PROCESS_DPI_UNAWARE");
        return S_OK;
    }
    return Real_SetProcessDpiAwareness(value);
}

DPI_AWARENESS_CONTEXT WINAPI Hook_SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT context) {
    const char* name = Shim_AwarenessName(context);
    if (Shim_ShouldSpoof() && Shim_Config().blockPerMonitor &&
        name && strcmp(name, "per-monitor") == 0) {
        if (Log_ShouldSample(&g_nPmaBlock, 8, 80)) {
            LOG_INFO("blocked SetThreadDpiAwarenessContext(per-monitor) -> system");
        }
        if (!Real_SetThreadDpiAwarenessContext) {
            return context;
        }
        return Real_SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
    }
    LOG_VERBOSE("SetThreadDpiAwarenessContext(%s)", name);
    return Real_SetThreadDpiAwarenessContext(context);
}

HRESULT WINAPI Hook_GetProcessDpiAwareness(HANDLE process, PROCESS_DPI_AWARENESS* value) {
    return Real_GetProcessDpiAwareness(process, value);
}

HWND WINAPI Hook_CreateWindowExW(DWORD exStyle, LPCWSTR className, LPCWSTR windowName, DWORD style,
                                 int x, int y, int w, int h, HWND parent, HMENU menu,
                                 HINSTANCE instance, LPVOID param) {
    HWND hwnd = Real_CreateWindowExW(exStyle, className, windowName, style, x, y, w, h, parent, menu, instance, param);
    if (hwnd && Log_ShouldSample(&g_nCreateWindow, 60, 200)) {
        char desc[512] = {};
        DescribeHwnd(hwnd, desc, sizeof(desc));
        UINT winDpi = 0;
        if (Real_GetDpiForWindow) {
            winDpi = Real_GetDpiForWindow(hwnd);
        }
        LOG_INFO("CreateWindowExW size=%dx%d pos=%d,%d realWindowDpi=%u spoofDpi=%u %s",
                 w, h, x, y, winDpi, Shim_ShouldSpoof() ? Shim_VirtualDpi() : winDpi, desc);
    }
    return hwnd;
}

HGDIOBJ WINAPI Hook_GetStockObject(int index) {
    HGDIOBJ original = Real_GetStockObject ? Real_GetStockObject(index) : nullptr;
    if (!original || !Shim_ShouldSpoof() || !Shim_Config().scaleStockFonts) {
        return original;
    }
    switch (index) {
    case OEM_FIXED_FONT:
    case ANSI_FIXED_FONT:
    case ANSI_VAR_FONT:
    case SYSTEM_FONT:
    case DEVICE_DEFAULT_FONT:
    case SYSTEM_FIXED_FONT:
    case DEFAULT_GUI_FONT:
        break;
    default:
        return original;
    }
    if (index < 0 || index >= 32) {
        return original;
    }
    if (g_scaledStockFonts[index]) {
        return g_scaledStockFonts[index];
    }
    LOGFONTW lf = {};
    if (GetObjectW(original, sizeof(lf), &lf) != sizeof(lf)) {
        return original;
    }
    const LONG oldHeight = lf.lfHeight;
    lf.lfHeight = Shim_ScaleFromSystem(lf.lfHeight);
    if (lf.lfWidth) {
        lf.lfWidth = Shim_ScaleFromSystem(lf.lfWidth);
    }
    HFONT scaled = Real_CreateFontIndirectW ? Real_CreateFontIndirectW(&lf) : CreateFontIndirectW(&lf);
    if (!scaled) {
        return original;
    }
    g_scaledStockFonts[index] = scaled;
    if (Log_ShouldSample(&g_nGetStockObject, 12, 200)) {
        LOG_INFO("GetStockObject(%d) lfHeight %ld -> %ld hwndFont=%p",
                 index, oldHeight, lf.lfHeight, scaled);
    }
    return scaled;
}

HFONT WINAPI Hook_CreateFontIndirectW(const LOGFONTW* src) {
    if (!src) {
        return Real_CreateFontIndirectW ? Real_CreateFontIndirectW(src) : nullptr;
    }
    if (Log_ShouldSample(&g_nCreateFont, 25, 400)) {
        char face[64] = {};
        WideCharToMultiByte(CP_UTF8, 0, src->lfFaceName, -1, face, sizeof(face), nullptr, nullptr);
        LOG_INFO("CreateFontIndirectW h=%ld w=%ld weight=%ld face=\"%s\"",
                 src->lfHeight, src->lfWidth, src->lfWeight, face);
    }
    return Real_CreateFontIndirectW(src);
}

void RescalePointFromWindow(HWND hwnd, LPPOINT out, UINT fromDpi, UINT toDpi) {
    if (!out || fromDpi == 0 || fromDpi == toDpi) {
        return;
    }
    RECT wr = {};
    if (hwnd && GetWindowRect(hwnd, &wr)) {
        out->x = wr.left + MulDiv(out->x - wr.left, static_cast<int>(toDpi), static_cast<int>(fromDpi));
        out->y = wr.top + MulDiv(out->y - wr.top, static_cast<int>(toDpi), static_cast<int>(fromDpi));
        return;
    }
    out->x = MulDiv(out->x, static_cast<int>(toDpi), static_cast<int>(fromDpi));
    out->y = MulDiv(out->y, static_cast<int>(toDpi), static_cast<int>(fromDpi));
}

BOOL WINAPI Hook_LogicalToPhysicalPointForPerMonitorDPI(HWND hwnd, LPPOINT pt) {
    if (!pt || !Real_LogicalToPhysicalPointForPerMonitorDPI) {
        return FALSE;
    }
    const POINT in = *pt;
    const BOOL ok = Real_LogicalToPhysicalPointForPerMonitorDPI(hwnd, pt);
    if (!ok || !Shim_ShouldSpoof()) {
        return ok;
    }
    const UINT realDpi = Real_GetDpiForWindow ? Real_GetDpiForWindow(hwnd) : Shim_SystemDpi();
    const UINT virt = Shim_VirtualDpi();
    if (realDpi != 0 && realDpi != virt && (in.x != pt->x || in.y != pt->y)) {
        RescalePointFromWindow(hwnd, pt, realDpi, virt);
    }
    if (Log_ShouldSample(&g_nPointConv, 20, 200)) {
        LOG_INFO("LogicalToPhysical in=%ld,%ld out=%ld,%ld realDpi=%u virt=%u hwnd=0x%p",
                 in.x, in.y, pt->x, pt->y, realDpi, virt, hwnd);
    }
    return TRUE;
}

BOOL WINAPI Hook_PhysicalToLogicalPointForPerMonitorDPI(HWND hwnd, LPPOINT pt) {
    if (!pt || !Real_PhysicalToLogicalPointForPerMonitorDPI) {
        return FALSE;
    }
    const POINT in = *pt;
    const BOOL ok = Real_PhysicalToLogicalPointForPerMonitorDPI(hwnd, pt);
    if (!ok || !Shim_ShouldSpoof()) {
        return ok;
    }
    const UINT realDpi = Real_GetDpiForWindow ? Real_GetDpiForWindow(hwnd) : Shim_SystemDpi();
    const UINT virt = Shim_VirtualDpi();
    if (realDpi != 0 && realDpi != virt && (in.x != pt->x || in.y != pt->y)) {
        RescalePointFromWindow(hwnd, pt, virt, realDpi);
    }
    if (Log_ShouldSample(&g_nPointConv, 20, 200)) {
        LOG_INFO("PhysicalToLogical in=%ld,%ld out=%ld,%ld realDpi=%u virt=%u hwnd=0x%p",
                 in.x, in.y, pt->x, pt->y, realDpi, virt, hwnd);
    }
    return TRUE;
}

int __cdecl Hook_getContextDPI() {
    const int original = Real_getContextDPI ? Real_getContextDPI() : static_cast<int>(Shim_SystemDpi());
    const int spoofed = Shim_ShouldSpoof() ? static_cast<int>(Shim_VirtualDpi()) : original;
    if (Log_ShouldSample(&g_nGetContextDpi, 15, 200)) {
        LOG_INFO("getContextDPI original=%d spoofed=%d", original, spoofed);
    }
    return spoofed;
}

BOOL WINAPI Hook_CreateProcessInternalW(HANDLE token, LPCWSTR appName, LPWSTR cmdLine,
                                        LPSECURITY_ATTRIBUTES procAttr, LPSECURITY_ATTRIBUTES threadAttr,
                                        BOOL inherit, DWORD flags, LPVOID env, LPCWSTR cwd,
                                        LPSTARTUPINFOW si, LPPROCESS_INFORMATION pi, PHANDLE newToken) {
    wchar_t image[MAX_PATH] = {};
    const bool oneC = ShouldInjectImage(appName, cmdLine, image, MAX_PATH);
    Log_WriteW(L"CreateProcessInternalW oneC=%s image=%s cmd=%s", oneC ? L"yes" : L"no", image,
               cmdLine ? cmdLine : L"");

    bool addedSuspend = false;
    DWORD useFlags = flags;
    if (oneC && (useFlags & CREATE_SUSPENDED) == 0) {
        useFlags |= CREATE_SUSPENDED;
        addedSuspend = true;
    }

    PROCESS_INFORMATION localPi = {};
    LPPROCESS_INFORMATION usePi = pi ? pi : &localPi;
    const BOOL ok = Real_CreateProcessInternalW(token, appName, cmdLine, procAttr, threadAttr, inherit,
                                                useFlags, env, cwd, si, usePi, newToken);
    if (!ok) {
        LOG_INFO("CreateProcessInternalW failed GLE=%lu", GetLastError());
        return FALSE;
    }
    LOG_INFO("CreateProcessInternalW pid=%lu inject=%s", usePi->dwProcessId, oneC ? "yes" : "no");
    if (oneC) {
        InjectChild(usePi->hProcess, usePi->hThread, addedSuspend);
    }
    if (!pi) {
        CloseHandle(localPi.hThread);
        CloseHandle(localPi.hProcess);
    }
    return TRUE;
}

BOOL WINAPI Hook_CreateProcessW(LPCWSTR appName, LPWSTR cmdLine, LPSECURITY_ATTRIBUTES procAttr,
                                LPSECURITY_ATTRIBUTES threadAttr, BOOL inherit, DWORD flags,
                                LPVOID env, LPCWSTR cwd, LPSTARTUPINFOW si, LPPROCESS_INFORMATION pi) {
    wchar_t image[MAX_PATH] = {};
    const bool oneC = ShouldInjectImage(appName, cmdLine, image, MAX_PATH);
    Log_WriteW(L"CreateProcessW oneC=%s image=%s cmd=%s", oneC ? L"yes" : L"no", image,
               cmdLine ? cmdLine : L"");

    if (g_internalHooked) {
        return Real_CreateProcessW(appName, cmdLine, procAttr, threadAttr, inherit, flags, env, cwd, si, pi);
    }

    bool addedSuspend = false;
    DWORD useFlags = flags;
    if (oneC && (useFlags & CREATE_SUSPENDED) == 0) {
        useFlags |= CREATE_SUSPENDED;
        addedSuspend = true;
    }

    PROCESS_INFORMATION localPi = {};
    LPPROCESS_INFORMATION usePi = pi ? pi : &localPi;
    const BOOL ok = Real_CreateProcessW(appName, cmdLine, procAttr, threadAttr, inherit, useFlags, env, cwd, si, usePi);
    if (!ok) {
        LOG_INFO("CreateProcessW failed GLE=%lu", GetLastError());
        return FALSE;
    }
    LOG_INFO("CreateProcessW pid=%lu suspended=%s", usePi->dwProcessId, addedSuspend ? "injected" : "as-requested");
    if (oneC) {
        InjectChild(usePi->hProcess, usePi->hThread, addedSuspend);
    }
    if (!pi) {
        CloseHandle(localPi.hThread);
        CloseHandle(localPi.hProcess);
    }
    return TRUE;
}

BOOL WINAPI Hook_CreateProcessA(LPCSTR appName, LPSTR cmdLine, LPSECURITY_ATTRIBUTES procAttr,
                                LPSECURITY_ATTRIBUTES threadAttr, BOOL inherit, DWORD flags,
                                LPVOID env, LPCSTR cwd, LPSTARTUPINFOA si, LPPROCESS_INFORMATION pi) {
    wchar_t appW[MAX_PATH] = {};
    wchar_t cmdW[32768] = {};
    if (appName) MultiByteToWideChar(CP_ACP, 0, appName, -1, appW, MAX_PATH);
    if (cmdLine) MultiByteToWideChar(CP_ACP, 0, cmdLine, -1, cmdW, 32768);

    wchar_t image[MAX_PATH] = {};
    const bool oneC = ShouldInjectImage(appName ? appW : nullptr, cmdLine ? cmdW : nullptr, image, MAX_PATH);
    if (g_internalHooked) {
        if (oneC) {
            Log_WriteW(L"CreateProcessA oneC=yes image=%s (internal hook)", image);
        }
        return Real_CreateProcessA(appName, cmdLine, procAttr, threadAttr, inherit, flags, env, cwd, si, pi);
    }
    if (!oneC) {
        return Real_CreateProcessA(appName, cmdLine, procAttr, threadAttr, inherit, flags, env, cwd, si, pi);
    }
    Log_WriteW(L"CreateProcessA oneC=yes image=%s", image);

    bool addedSuspend = false;
    DWORD useFlags = flags;
    if ((useFlags & CREATE_SUSPENDED) == 0) {
        useFlags |= CREATE_SUSPENDED;
        addedSuspend = true;
    }
    PROCESS_INFORMATION localPi = {};
    LPPROCESS_INFORMATION usePi = pi ? pi : &localPi;
    const BOOL ok = Real_CreateProcessA(appName, cmdLine, procAttr, threadAttr, inherit, useFlags, env, cwd, si, usePi);
    if (!ok) {
        return FALSE;
    }
    InjectChild(usePi->hProcess, usePi->hThread, addedSuspend);
    if (!pi) {
        CloseHandle(localPi.hThread);
        CloseHandle(localPi.hProcess);
    }
    return TRUE;
}

bool LaunchOneCViaCreateProcess(LPCWSTR exe, LPCWSTR params, LPCWSTR dir, int show, PROCESS_INFORMATION* pi) {
    wchar_t cmd[32768] = {};
    if (exe && wcschr(exe, L' ')) {
        swprintf_s(cmd, L"\"%s\"", exe);
    } else {
        wcsncpy_s(cmd, exe ? exe : L"", _TRUNCATE);
    }
    if (params && params[0]) {
        wcsncat_s(cmd, L" ", _TRUNCATE);
        wcsncat_s(cmd, params, _TRUNCATE);
    }

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = static_cast<WORD>(show);

    PROCESS_INFORMATION local = {};
    PROCESS_INFORMATION* usePi = pi ? pi : &local;
    const BOOL ok = Hook_CreateProcessW(exe, cmd, nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT,
                                        nullptr, dir, &si, usePi);
    if (!ok) {
        return false;
    }
    if (!pi) {
        CloseHandle(local.hThread);
        CloseHandle(local.hProcess);
    }
    return true;
}

HMODULE WINAPI Hook_LoadLibraryExW(LPCWSTR name, HANDLE file, DWORD flags) {
    HMODULE mod = Real_LoadLibraryExW(name, file, flags);
    if (mod && name) {
        const wchar_t* base = wcsrchr(name, L'\\');
        base = base ? base + 1 : name;
        if (_wcsicmp(base, L"SHCore.dll") == 0 || _wcsicmp(name, L"SHCore.dll") == 0 ||
            _wcsicmp(base, L"wbase83.dll") == 0 || _wcsicmp(name, L"wbase83.dll") == 0) {
            Hooks_TryLateApis();
        }
    }
    return mod;
}

BOOL WINAPI Hook_ShellExecuteExW(SHELLEXECUTEINFOW* info) {
    if (!info || !info->lpFile) {
        return Real_ShellExecuteExW(info);
    }
    Log_WriteW(L"ShellExecuteExW verb=%s file=%s params=%s dir=%s",
               info->lpVerb ? info->lpVerb : L"",
               info->lpFile,
               info->lpParameters ? info->lpParameters : L"",
               info->lpDirectory ? info->lpDirectory : L"");

    const wchar_t* verb = info->lpVerb;
    if (verb && verb[0] && _wcsicmp(verb, L"open") != 0) {
        return Real_ShellExecuteExW(info);
    }

    wchar_t exe[MAX_PATH] = {};
    wchar_t params[32768] = {};
    if (!PrepareShellOneCLaunch(info->lpFile, info->lpParameters, info->lpDirectory,
                                exe, MAX_PATH, params, 32768)) {
        return Real_ShellExecuteExW(info);
    }

    Log_WriteW(L"ShellExecuteExW intercepted exe=%s params=%s", exe, params);
    PROCESS_INFORMATION pi = {};
    if (!LaunchOneCViaCreateProcess(exe, params[0] ? params : nullptr, info->lpDirectory,
                                    info->nShow, &pi)) {
        Log_WriteW(L"ShellExecuteExW fallback to original, GLE=%lu", GetLastError());
        return Real_ShellExecuteExW(info);
    }
    if (info->fMask & SEE_MASK_NOCLOSEPROCESS) {
        info->hProcess = pi.hProcess;
    } else {
        CloseHandle(pi.hProcess);
    }
    CloseHandle(pi.hThread);
    info->hInstApp = reinterpret_cast<HINSTANCE>(static_cast<INT_PTR>(32 + 1));
    return TRUE;
}

HINSTANCE WINAPI Hook_ShellExecuteW(HWND hwnd, LPCWSTR op, LPCWSTR file, LPCWSTR params, LPCWSTR dir, INT show) {
    Log_WriteW(L"ShellExecuteW verb=%s file=%s params=%s",
               op ? op : L"", file ? file : L"", params ? params : L"");
    const bool openVerb = !op || !op[0] || _wcsicmp(op, L"open") == 0;
    wchar_t exe[MAX_PATH] = {};
    wchar_t launchParams[32768] = {};
    if (openVerb && file &&
        PrepareShellOneCLaunch(file, params, dir, exe, MAX_PATH, launchParams, 32768)) {
        Log_WriteW(L"ShellExecuteW intercepted exe=%s params=%s", exe, launchParams);
        PROCESS_INFORMATION pi = {};
        if (LaunchOneCViaCreateProcess(exe, launchParams[0] ? launchParams : nullptr, dir, show, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return reinterpret_cast<HINSTANCE>(static_cast<INT_PTR>(32 + 1));
        }
    }
    return Real_ShellExecuteW(hwnd, op, file, params, dir, show);
}

bool HookApi(LPCWSTR module, LPCSTR name, LPVOID detour, LPVOID* original) {
    const MH_STATUS st = MH_CreateHookApi(module, name, detour, original);
    if (st != MH_OK && st != MH_ERROR_ALREADY_CREATED) {
        LOG_INFO("hook skip %s!%s status=%d", 
                 module && module[0] ? "mod" : "?", name, static_cast<int>(st));
        return false;
    }
    LOG_INFO("hook ok %s", name);
    return true;
}

} // namespace

bool Hooks_Install() {
    if (g_installed) {
        return true;
    }
    MH_STATUS st = MH_Initialize();
    if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) {
        LOG_INFO("MH_Initialize failed %d", static_cast<int>(st));
        return false;
    }

    HookApi(L"gdi32", "GetDeviceCaps", reinterpret_cast<LPVOID>(&Hook_GetDeviceCaps),
            reinterpret_cast<LPVOID*>(&Real_GetDeviceCaps));
    HookApi(L"gdi32", "GetStockObject", reinterpret_cast<LPVOID>(&Hook_GetStockObject),
            reinterpret_cast<LPVOID*>(&Real_GetStockObject));
    HookApi(L"gdi32", "CreateFontIndirectW", reinterpret_cast<LPVOID>(&Hook_CreateFontIndirectW),
            reinterpret_cast<LPVOID*>(&Real_CreateFontIndirectW));
    HookApi(L"user32", "GetSystemMetrics", reinterpret_cast<LPVOID>(&Hook_GetSystemMetrics),
            reinterpret_cast<LPVOID*>(&Real_GetSystemMetrics));
    HookApi(L"user32", "SystemParametersInfoW", reinterpret_cast<LPVOID>(&Hook_SystemParametersInfoW),
            reinterpret_cast<LPVOID*>(&Real_SystemParametersInfoW));
    HookApi(L"user32", "CreateWindowExW", reinterpret_cast<LPVOID>(&Hook_CreateWindowExW),
            reinterpret_cast<LPVOID*>(&Real_CreateWindowExW));
    HookApi(L"kernel32", "CreateProcessW", reinterpret_cast<LPVOID>(&Hook_CreateProcessW),
            reinterpret_cast<LPVOID*>(&Real_CreateProcessW));
    HookApi(L"kernel32", "CreateProcessA", reinterpret_cast<LPVOID>(&Hook_CreateProcessA),
            reinterpret_cast<LPVOID*>(&Real_CreateProcessA));
    g_internalHooked = HookApi(L"kernelbase", "CreateProcessInternalW",
                               reinterpret_cast<LPVOID>(&Hook_CreateProcessInternalW),
                               reinterpret_cast<LPVOID*>(&Real_CreateProcessInternalW));
    if (!g_internalHooked) {
        g_internalHooked = HookApi(L"kernel32", "CreateProcessInternalW",
                                   reinterpret_cast<LPVOID>(&Hook_CreateProcessInternalW),
                                   reinterpret_cast<LPVOID*>(&Real_CreateProcessInternalW));
    }
    LOG_INFO("CreateProcessInternalW hook %s", g_internalHooked ? "active" : "missing");
    HookApi(L"kernel32", "LoadLibraryExW", reinterpret_cast<LPVOID>(&Hook_LoadLibraryExW),
            reinterpret_cast<LPVOID*>(&Real_LoadLibraryExW));
    HookApi(L"shell32", "ShellExecuteExW", reinterpret_cast<LPVOID>(&Hook_ShellExecuteExW),
            reinterpret_cast<LPVOID*>(&Real_ShellExecuteExW));
    HookApi(L"shell32", "ShellExecuteW", reinterpret_cast<LPVOID>(&Hook_ShellExecuteW),
            reinterpret_cast<LPVOID*>(&Real_ShellExecuteW));

    HookApi(L"user32", "GetDpiForWindow", reinterpret_cast<LPVOID>(&Hook_GetDpiForWindow),
            reinterpret_cast<LPVOID*>(&Real_GetDpiForWindow));
    HookApi(L"user32", "GetDpiForSystem", reinterpret_cast<LPVOID>(&Hook_GetDpiForSystem),
            reinterpret_cast<LPVOID*>(&Real_GetDpiForSystem));
    HookApi(L"user32", "GetSystemMetricsForDpi", reinterpret_cast<LPVOID>(&Hook_GetSystemMetricsForDpi),
            reinterpret_cast<LPVOID*>(&Real_GetSystemMetricsForDpi));
    HookApi(L"user32", "SystemParametersInfoForDpi", reinterpret_cast<LPVOID>(&Hook_SystemParametersInfoForDpi),
            reinterpret_cast<LPVOID*>(&Real_SystemParametersInfoForDpi));
    HookApi(L"user32", "AdjustWindowRectExForDpi", reinterpret_cast<LPVOID>(&Hook_AdjustWindowRectExForDpi),
            reinterpret_cast<LPVOID*>(&Real_AdjustWindowRectExForDpi));
    HookApi(L"user32", "SetProcessDpiAwarenessContext", reinterpret_cast<LPVOID>(&Hook_SetProcessDpiAwarenessContext),
            reinterpret_cast<LPVOID*>(&Real_SetProcessDpiAwarenessContext));
    HookApi(L"user32", "SetThreadDpiAwarenessContext", reinterpret_cast<LPVOID>(&Hook_SetThreadDpiAwarenessContext),
            reinterpret_cast<LPVOID*>(&Real_SetThreadDpiAwarenessContext));
    HookApi(L"user32", "LogicalToPhysicalPointForPerMonitorDPI",
            reinterpret_cast<LPVOID>(&Hook_LogicalToPhysicalPointForPerMonitorDPI),
            reinterpret_cast<LPVOID*>(&Real_LogicalToPhysicalPointForPerMonitorDPI));
    HookApi(L"user32", "PhysicalToLogicalPointForPerMonitorDPI",
            reinterpret_cast<LPVOID>(&Hook_PhysicalToLogicalPointForPerMonitorDPI),
            reinterpret_cast<LPVOID*>(&Real_PhysicalToLogicalPointForPerMonitorDPI));
    HookApi(L"SHCore", "GetDpiForMonitor", reinterpret_cast<LPVOID>(&Hook_GetDpiForMonitor),
            reinterpret_cast<LPVOID*>(&Real_GetDpiForMonitor));
    HookApi(L"SHCore", "SetProcessDpiAwareness", reinterpret_cast<LPVOID>(&Hook_SetProcessDpiAwareness),
            reinterpret_cast<LPVOID*>(&Real_SetProcessDpiAwareness));
    HookApi(L"SHCore", "GetProcessDpiAwareness", reinterpret_cast<LPVOID>(&Hook_GetProcessDpiAwareness),
            reinterpret_cast<LPVOID*>(&Real_GetProcessDpiAwareness));

    st = MH_EnableHook(MH_ALL_HOOKS);
    if (st != MH_OK) {
        LOG_INFO("MH_EnableHook failed %d", static_cast<int>(st));
        return false;
    }
    g_installed = true;
    LOG_INFO("hooks enabled");
    Hooks_TryLateApis();
    return true;
}

void Hooks_TryLateApis() {
    if (!g_installed) {
        return;
    }
    bool added = false;
    if (!Real_GetDpiForMonitor) {
        added |= HookApi(L"SHCore", "GetDpiForMonitor", reinterpret_cast<LPVOID>(&Hook_GetDpiForMonitor),
                         reinterpret_cast<LPVOID*>(&Real_GetDpiForMonitor));
    }
    if (!Real_SetProcessDpiAwareness) {
        added |= HookApi(L"SHCore", "SetProcessDpiAwareness", reinterpret_cast<LPVOID>(&Hook_SetProcessDpiAwareness),
                         reinterpret_cast<LPVOID*>(&Real_SetProcessDpiAwareness));
    }
    if (!Real_getContextDPI && GetModuleHandleW(L"wbase83.dll")) {
        added |= HookApi(L"wbase83", "?getContextDPI@wbase@@YAHXZ",
                         reinterpret_cast<LPVOID>(&Hook_getContextDPI),
                         reinterpret_cast<LPVOID*>(&Real_getContextDPI));
        if (Real_getContextDPI) {
            LOG_INFO("getContextDPI hook active");
        }
    }
    if (added) {
        MH_EnableHook(MH_ALL_HOOKS);
        LOG_INFO("late hooks enabled");
    }
}

void Hooks_Uninstall() {
    if (!g_installed) {
        return;
    }
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    g_installed = false;
}

bool Hooks_AreInstalled() {
    return g_installed;
}
