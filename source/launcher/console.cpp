#include "console.h"
#include "resource.h"

#include <windows.h>
#include <cstdio>
#include <cstdarg>

void Console::Print(const wchar_t* fmt, ...) {
    wchar_t buf[1024] = {};
    va_list args;
    va_start(args, fmt);
    vswprintf_s(buf, fmt, args);
    va_end(args);
    fputws(buf, stdout);
    fputws(L"\n", stdout);
}

void Console::ApplyIcon() {
    HWND hwnd = GetConsoleWindow();
    if (!hwnd) {
        return;
    }
    const HINSTANCE inst = GetModuleHandleW(nullptr);
    const int smallCx = GetSystemMetrics(SM_CXSMICON);
    const int smallCy = GetSystemMetrics(SM_CYSMICON);
    HICON bigIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APPICON));
    HICON smallIcon = reinterpret_cast<HICON>(LoadImageW(
        inst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, smallCx, smallCy, LR_DEFAULTCOLOR));
    if (bigIcon) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
    }
    if (smallIcon) {
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
    }
}

void Console::Ensure() {
    if (!GetConsoleWindow()) {
        if (AllocConsole()) {
            FILE* fp = nullptr;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);
            freopen_s(&fp, "CONIN$", "r", stdin);
        }
    }
    ApplyIcon();
}

void Console::Usage() {
    Print(L"1C DPI Shim");
    Print(L"Usage: 1C-DPI-Shim.exe [dpiPercent] [options] [--] [1C arguments...]");
    Print(L"");
    Print(L"Examples:");
    Print(L"  1C-DPI-Shim.exe 200");
    Print(L"  1C-DPI-Shim.exe 225 --designer");
    Print(L"  1C-DPI-Shim.exe 250 --designer -- /N\"Admin\"");
    Print(L"  1C-DPI-Shim.exe 300 --designer");
    Print(L"  1C-DPI-Shim.exe --dpi=175");
    Print(L"");
    Print(L"Options:");
    Print(L"  N                   Target scale percent (50-400, may exceed 255)");
    Print(L"  --dpi=N             Same as a bare percent");
    Print(L"  --exe=PATH          Path to 1cestart.exe / 1cv8.exe");
    Print(L"  --exe PATH          Same, as a separate argument");
    Print(L"  --designer          Launch 1cv8.exe DESIGNER directly");
    Print(L"  --start             Launch 1cestart.exe (default)");
    Print(L"  --ini=PATH          Path to 1c-dpi.ini");
    Print(L"  --log=PATH          Shim log path");
    Print(L"  --log=off           Disable logging");
    Print(L"  --no-log            Disable logging");
    Print(L"  --console           Keep a console attached");
    Print(L"  --help              This help");
}
