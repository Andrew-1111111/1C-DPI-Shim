#include "console_icon.h"
#include "dpi_math.h"
#include "inject.h"
#include "launcher_args.h"
#include "payload.h"
#include "process_utils.h"

#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>
#include <wchar.h>

namespace {

void Print(const wchar_t* fmt, ...) {
    wchar_t buf[1024] = {};
    va_list args;
    va_start(args, fmt);
    vswprintf_s(buf, fmt, args);
    va_end(args);
    fputws(buf, stdout);
    fputws(L"\n", stdout);
}

void EnsureConsole() {
    if (!GetConsoleWindow()) {
        if (AllocConsole()) {
            FILE* fp = nullptr;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);
            freopen_s(&fp, "CONIN$", "r", stdin);
        }
    }
    ApplyConsoleIcon();
}

bool FileExists(const wchar_t* path) {
    const DWORD attr = GetFileAttributesW(path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool DirExists(const wchar_t* path) {
    const DWORD attr = GetFileAttributesW(path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring ModuleDir() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) {
        *slash = 0;
    }
    return path;
}

std::wstring Join(const std::wstring& a, const wchar_t* b) {
    if (a.empty()) {
        return b;
    }
    if (a.back() == L'\\' || a.back() == L'/') {
        return a + b;
    }
    return a + L"\\" + b;
}

std::wstring FindLatest1cv8() {
    const wchar_t* roots[] = {
        L"C:\\Program Files (x86)\\1cv8",
        L"C:\\Program Files\\1cv8",
    };
    std::wstring bestVer;
    std::wstring bestExe;
    for (const wchar_t* root : roots) {
        if (!DirExists(root)) {
            continue;
        }
        WIN32_FIND_DATAW fd = {};
        const std::wstring pattern = std::wstring(root) + L"\\*";
        HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) {
            continue;
        }
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || fd.cFileName[0] == L'.') {
                continue;
            }
            if (!Proc_LooksLike1CVersionDir(fd.cFileName)) {
                continue;
            }
            const std::wstring exe = Join(Join(root, fd.cFileName), L"bin\\1cv8.exe");
            if (!FileExists(exe.c_str())) {
                continue;
            }
            if (bestVer.empty() || wcscmp(fd.cFileName, bestVer.c_str()) > 0) {
                bestVer = fd.cFileName;
                bestExe = exe;
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (FileExists(L"C:\\Program Files (x86)\\1cv8\\8.3.27.2342\\bin\\1cv8.exe")) {
        return L"C:\\Program Files (x86)\\1cv8\\8.3.27.2342\\bin\\1cv8.exe";
    }
    return bestExe;
}

std::wstring ReadIniString(const std::wstring& ini, const wchar_t* section, const wchar_t* key) {
    if (ini.empty() || !FileExists(ini.c_str())) {
        return {};
    }
    wchar_t buf[MAX_PATH] = {};
    GetPrivateProfileStringW(section, key, L"", buf, MAX_PATH, ini.c_str());
    std::wstring s = buf;
    if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"') {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}

std::wstring Quote(const std::wstring& s) {
    if (s.find(L' ') == std::wstring::npos) {
        return s;
    }
    return L"\"" + s + L"\"";
}

void Usage() {
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

} // namespace

int wmain(int argc, wchar_t** argv) {
    ApplyConsoleIcon();
    LauncherOptions opt;
    Launcher_ParseArgs(argc, argv, opt);
    if (opt.help) {
        EnsureConsole();
        Usage();
        return 0;
    }
    if (opt.console) {
        EnsureConsole();
    }

    const std::wstring dir = ModuleDir();
    wchar_t dllPathBuf[MAX_PATH] = {};
    wchar_t payloadErr[512] = {};
    if (!Payload_EnsureShimDll(dllPathBuf, MAX_PATH, payloadErr, 512)) {
        EnsureConsole();
        Print(L"ERROR: failed to prepare the embedded shim:");
        Print(payloadErr);
        return 1;
    }
    const std::wstring dllPath = dllPathBuf;

    std::wstring ini = opt.iniPath;
    if (ini.empty()) {
        ini = Join(dir, L"1c-dpi.ini");
    }

    int dpi = opt.dpiPercent;
    if (dpi < 0 && FileExists(ini.c_str())) {
        dpi = static_cast<int>(GetPrivateProfileIntW(L"shim", L"dpi", 225, ini.c_str()));
    }
    if (dpi < 0) {
        dpi = 225;
    }
    if (!DpiMath_PercentInRange(dpi)) {
        EnsureConsole();
        Print(L"ERROR: dpi percent must be 50..400, got %d", dpi);
        return 1;
    }

    std::wstring exe = opt.exePath;
    if (exe.empty() && FileExists(ini.c_str())) {
        if (opt.designer) {
            exe = ReadIniString(ini, L"launcher", L"platform_exe");
            if (exe.empty()) {
                exe = ReadIniString(ini, L"launcher", L"exe");
            }
        } else {
            exe = ReadIniString(ini, L"launcher", L"exe");
            if (exe.empty()) {
                exe = ReadIniString(ini, L"launcher", L"start_exe");
            }
        }
    }
    if (exe.empty()) {
        if (opt.designer || !opt.useStart) {
            exe = FindLatest1cv8();
        } else {
            exe = L"C:\\Program Files (x86)\\1cv8\\common\\1cestart.exe";
            if (!FileExists(exe.c_str())) {
                exe = FindLatest1cv8();
            }
        }
    }
    if (exe.empty() || !FileExists(exe.c_str())) {
        EnsureConsole();
        Print(L"ERROR: 1C executable not found. Use --exe=PATH");
        Print(exe.c_str());
        return 1;
    }

    std::wstring cmd = Quote(exe);
    if (opt.designer) {
        bool hasDesigner = false;
        for (const auto& a : opt.passthrough) {
            if (_wcsicmp(a.c_str(), L"DESIGNER") == 0) {
                hasDesigner = true;
            }
        }
        if (!hasDesigner) {
            cmd += L" DESIGNER";
        }
    }
    for (const auto& a : opt.passthrough) {
        cmd += L" ";
        cmd += Quote(a);
    }

    wchar_t dpiBuf[16] = {};
    swprintf_s(dpiBuf, L"%d", dpi);
    SetEnvironmentVariableW(L"ONEC_DPI", dpiBuf);
    SetEnvironmentVariableW(L"ONEC_DPI_ENABLED", L"1");
    if (FileExists(ini.c_str())) {
        SetEnvironmentVariableW(L"ONEC_DPI_INI", ini.c_str());
    }
    if (opt.logEnabled >= 0) {
        SetEnvironmentVariableW(L"ONEC_DPI_LOG_ENABLED", opt.logEnabled ? L"1" : L"0");
    }
    if (!opt.logPath.empty()) {
        SetEnvironmentVariableW(L"ONEC_DPI_LOG", opt.logPath.c_str());
    }

    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    PROCESS_INFORMATION pi = {};
    wchar_t error[512] = {};
    if (!Inject_CreateAndInject(exe.c_str(), cmdBuf.data(), nullptr, dllPath.c_str(), 20000, &pi, error, 512)) {
        EnsureConsole();
        Print(L"ERROR: failed to launch/inject:");
        Print(error);
        return 1;
    }

    if (opt.console || GetConsoleWindow()) {
        Print(L"Launched PID %lu", pi.dwProcessId);
        Print(L"Target DPI: %d%%", dpi);
        Print(L"Exe: %s", exe.c_str());
        Print(L"Shim: %s", dllPath.c_str());
    }

    if (pi.hThread) {
        CloseHandle(pi.hThread);
    }
    if (pi.hProcess) {
        CloseHandle(pi.hProcess);
    }
    return 0;
}
