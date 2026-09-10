#include "console.h"
#include "dpi_math.h"
#include "inject.h"
#include "io.h"
#include "launcher_args.h"
#include "onec_locator.h"
#include "payload.h"

#include <windows.h>
#include <string>
#include <vector>
#include <wchar.h>
#include <cstdio>

int wmain(int argc, wchar_t** argv) {
    Console::ApplyIcon();
    LauncherOptions opt;
    Launcher_ParseArgs(argc, argv, opt);
    if (opt.help) {
        Console::Ensure();
        Console::Usage();
        return 0;
    }
    if (opt.console) {
        Console::Ensure();
    }

    const std::wstring dir = IO::ModuleDir();
    wchar_t dllPathBuf[MAX_PATH] = {};
    wchar_t payloadErr[512] = {};
    if (!Payload_EnsureShimDll(dllPathBuf, MAX_PATH, payloadErr, 512)) {
        Console::Ensure();
        Console::Print(L"ERROR: failed to prepare the embedded shim:");
        Console::Print(payloadErr);
        return 1;
    }
    const std::wstring dllPath = dllPathBuf;

    std::wstring ini = opt.iniPath;
    if (ini.empty()) {
        ini = IO::Join(dir, L"1c-dpi.ini");
    }

    int dpi = opt.dpiPercent;
    if (dpi < 0 && IO::FileExists(ini.c_str())) {
        dpi = static_cast<int>(GetPrivateProfileIntW(L"shim", L"dpi", 200, ini.c_str()));
    }
    if (dpi < 0) {
        dpi = 200;
    }
    if (!DpiMath_PercentInRange(dpi)) {
        Console::Ensure();
        Console::Print(L"ERROR: dpi percent must be 100, 125, 150, 175, 200, 225, 250, 300, 400 or 500, got %d", dpi);
        return 1;
    }

    std::wstring exe = opt.exePath;
    if (exe.empty() && IO::FileExists(ini.c_str())) {
        if (opt.designer) {
            exe = IO::ReadIniString(ini, L"launcher", L"platform_exe");
            if (exe.empty()) {
                exe = IO::ReadIniString(ini, L"launcher", L"exe");
            }
        } else {
            exe = IO::ReadIniString(ini, L"launcher", L"exe");
            if (exe.empty()) {
                exe = IO::ReadIniString(ini, L"launcher", L"start_exe");
            }
        }
    }
    if (exe.empty()) {
        if (opt.designer || !opt.useStart) {
            exe = OneCLocator::FindLatest1cv8();
        } else {
            exe = L"C:\\Program Files (x86)\\1cv8\\common\\1cestart.exe";
            if (!IO::FileExists(exe.c_str())) {
                exe = OneCLocator::FindLatest1cv8();
            }
        }
    }
    if (exe.empty() || !IO::FileExists(exe.c_str())) {
        Console::Ensure();
        Console::Print(L"ERROR: 1C executable not found. Use --exe=PATH");
        Console::Print(exe.c_str());
        return 1;
    }

    std::wstring cmd = IO::Quote(exe);
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
        cmd += IO::Quote(a);
    }

    wchar_t dpiBuf[16] = {};
    swprintf_s(dpiBuf, L"%d", dpi);
    SetEnvironmentVariableW(L"ONEC_DPI", dpiBuf);
    SetEnvironmentVariableW(L"ONEC_DPI_ENABLED", L"1");
    if (IO::FileExists(ini.c_str())) {
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
        Console::Ensure();
        Console::Print(L"ERROR: failed to launch/inject:");
        Console::Print(error);
        return 1;
    }

    if (opt.console || GetConsoleWindow()) {
        Console::Print(L"Launched PID %lu", pi.dwProcessId);
        Console::Print(L"Target DPI: %d%%", dpi);
        Console::Print(L"Exe: %s", exe.c_str());
        Console::Print(L"Shim: %s", dllPath.c_str());
    }

    if (pi.hThread) {
        CloseHandle(pi.hThread);
    }
    if (pi.hProcess) {
        CloseHandle(pi.hProcess);
    }
    return 0;
}
