#pragma once

#include <string>
#include <vector>
#include <wchar.h>
#include <cstdlib>

struct LauncherOptions {
    int dpiPercent = -1;
    bool designer = false;
    bool useStart = true;
    bool help = false;
    bool console = false;
    int logEnabled = -1;
    std::wstring iniPath;
    std::wstring exePath;
    std::wstring logPath;
    std::vector<std::wstring> passthrough;
};

inline bool Launcher_ParseArgs(int argc, wchar_t** argv, LauncherOptions& opt) {
    bool afterDash = false;
    for (int i = 1; i < argc; ++i) {
        const wchar_t* a = argv[i];
        if (!afterDash && wcscmp(a, L"--") == 0) {
            afterDash = true;
            continue;
        }
        if (!afterDash && (wcscmp(a, L"-h") == 0 || wcscmp(a, L"--help") == 0 || wcscmp(a, L"/?") == 0)) {
            opt.help = true;
            continue;
        }
        if (!afterDash && wcscmp(a, L"--designer") == 0) {
            opt.designer = true;
            opt.useStart = false;
            continue;
        }
        if (!afterDash && wcscmp(a, L"--start") == 0) {
            opt.useStart = true;
            opt.designer = false;
            continue;
        }
        if (!afterDash && wcscmp(a, L"--console") == 0) {
            opt.console = true;
            continue;
        }
        if (!afterDash && wcsncmp(a, L"--dpi=", 6) == 0) {
            opt.dpiPercent = _wtoi(a + 6);
            continue;
        }
        if (!afterDash && wcsncmp(a, L"--ini=", 6) == 0) {
            opt.iniPath = a + 6;
            continue;
        }
        if (!afterDash && wcsncmp(a, L"--exe=", 6) == 0) {
            opt.exePath = a + 6;
            continue;
        }
        if (!afterDash && wcscmp(a, L"--exe") == 0 && i + 1 < argc) {
            opt.exePath = argv[++i];
            continue;
        }
        if (!afterDash && (wcscmp(a, L"--no-log") == 0 || wcscmp(a, L"--nolog") == 0)) {
            opt.logEnabled = 0;
            continue;
        }
        if (!afterDash && wcsncmp(a, L"--log=", 6) == 0) {
            const wchar_t* v = a + 6;
            if (_wcsicmp(v, L"off") == 0 || _wcsicmp(v, L"0") == 0 || _wcsicmp(v, L"false") == 0) {
                opt.logEnabled = 0;
            } else if (_wcsicmp(v, L"on") == 0 || _wcsicmp(v, L"1") == 0 || _wcsicmp(v, L"true") == 0) {
                opt.logEnabled = 1;
            } else {
                opt.logPath = v;
                opt.logEnabled = 1;
            }
            continue;
        }
        if (!afterDash && opt.dpiPercent < 0) {
            bool digits = a[0] != 0;
            for (const wchar_t* p = a; *p; ++p) {
                if (*p < L'0' || *p > L'9') {
                    digits = false;
                    break;
                }
            }
            if (digits) {
                opt.dpiPercent = _wtoi(a);
                continue;
            }
        }
        opt.passthrough.emplace_back(a);
    }
    return true;
}
