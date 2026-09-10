#include "console_icon.h"
#include "resource.h"

#include <windows.h>

void ApplyConsoleIcon() {
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
