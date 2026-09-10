#include "dpi_math.h"
#include "io.h"
#include "launcher_args.h"
#include "onec_locator.h"
#include "process_utils.h"

#include <conio.h>
#include <initializer_list>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

namespace {

int g_failed = 0;
int g_passed = 0;

void PrintStatus(bool passed, const char* name) {
    const char* word = passed ? "PASS" : "FAIL";
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info = {};
    const bool color = console != INVALID_HANDLE_VALUE && console != nullptr
        && GetConsoleScreenBufferInfo(console, &info);

    fputs("[ ", stdout);
    if (color) {
        fflush(stdout);
        const WORD fg = passed
            ? static_cast<WORD>(FOREGROUND_GREEN | FOREGROUND_INTENSITY)
            : static_cast<WORD>(FOREGROUND_RED | FOREGROUND_INTENSITY);
        SetConsoleTextAttribute(console, static_cast<WORD>((info.wAttributes & ~0x0F) | fg));
        fputs(word, stdout);
        fflush(stdout);
        SetConsoleTextAttribute(console, info.wAttributes);
    } else {
        fputs(word, stdout);
    }
    printf(" ] %s\n", name);
}

void Check(bool cond, const char* expr, const char* file, int line) {
    if (cond) {
        ++g_passed;
        PrintStatus(true, expr);
        return;
    }
    ++g_failed;
    char name[512];
    sprintf_s(name, "%s  (%s:%d)", expr, file, line);
    PrintStatus(false, name);
}

#define CHECK(cond) Check(!!(cond), #cond, __FILE__, __LINE__)

std::vector<wchar_t*> MakeArgv(std::vector<std::wstring>& storage, std::initializer_list<const wchar_t*> args) {
    storage.clear();
    storage.emplace_back(L"1C-DPI-Shim.exe");
    for (const wchar_t* a : args) {
        storage.emplace_back(a);
    }
    std::vector<wchar_t*> argv;
    argv.reserve(storage.size());
    for (auto& s : storage) {
        argv.push_back(s.data());
    }
    return argv;
}

void TestDpiMath() {
    CHECK(DpiMath_PercentToDpi(100) == 96);
    CHECK(DpiMath_PercentToDpi(125) == 120);
    CHECK(DpiMath_PercentToDpi(150) == 144);
    CHECK(DpiMath_PercentToDpi(175) == 168);
    CHECK(DpiMath_PercentToDpi(200) == 192);
    CHECK(DpiMath_PercentToDpi(225) == 216);
    CHECK(DpiMath_PercentToDpi(250) == 240);
    CHECK(DpiMath_PercentToDpi(500) == 480);
    CHECK(DpiMath_PercentToDpi(0) == 96);
    CHECK(DpiMath_PercentToDpi(-10) == 96);

    CHECK(DpiMath_DpiToPercent(96) == 100);
    CHECK(DpiMath_DpiToPercent(168) == 175);
    CHECK(DpiMath_DpiToPercent(192) == 200);
    CHECK(DpiMath_DpiToPercent(216) == 225);
    CHECK(DpiMath_DpiToPercent(240) == 250);
    CHECK(DpiMath_DpiToPercent(480) == 500);
    CHECK(DpiMath_DpiToPercent(0) == 100);

    CHECK(DpiMath_PercentToDpi(DpiMath_DpiToPercent(168)) == 168);
    CHECK(DpiMath_PercentToDpi(DpiMath_DpiToPercent(216)) == 216);

    CHECK(DpiMath_Scale(40, 168, 216) == MulDiv(40, 216, 168));
    CHECK(DpiMath_Scale(51, 168, 168) == 51);
    CHECK(DpiMath_Scale(100, 0, 216) == 100);
    CHECK(DpiMath_Scale(-16, 168, 216) == MulDiv(-16, 216, 168));

    CHECK(DpiMath_PercentInRange(100));
    CHECK(DpiMath_PercentInRange(125));
    CHECK(DpiMath_PercentInRange(150));
    CHECK(DpiMath_PercentInRange(175));
    CHECK(DpiMath_PercentInRange(200));
    CHECK(DpiMath_PercentInRange(225));
    CHECK(DpiMath_PercentInRange(250));
    CHECK(DpiMath_PercentInRange(300));
    CHECK(DpiMath_PercentInRange(400));
    CHECK(DpiMath_PercentInRange(500));
    CHECK(!DpiMath_PercentInRange(50));
    CHECK(!DpiMath_PercentInRange(230));
    CHECK(!DpiMath_PercentInRange(350));
    CHECK(!DpiMath_PercentInRange(99));
    CHECK(!DpiMath_PercentInRange(501));
}

void TestProcessNames() {
    CHECK(Proc_IsOneCImagePath(L"1cv8.exe"));
    CHECK(Proc_IsOneCImagePath(L"1CV8.EXE"));
    CHECK(Proc_IsOneCImagePath(L"1cv8"));
    CHECK(Proc_IsOneCImagePath(L"C:\\Program Files (x86)\\1cv8\\8.3.27.2342\\bin\\1cv8.exe"));
    CHECK(Proc_IsOneCImagePath(L"C:/Program Files (x86)/1cv8/bin/1cv8s.exe"));
    CHECK(Proc_IsOneCImagePath(L"1cestart.exe"));
    CHECK(Proc_IsOneCImagePath(L"1cv8c.exe"));
    CHECK(Proc_IsOneCImagePath(L"1cv8a.exe"));
    CHECK(!Proc_IsOneCImagePath(L"notepad.exe"));
    CHECK(!Proc_IsOneCImagePath(L"1cv8t.exe"));
    CHECK(!Proc_IsOneCImagePath(L""));
    CHECK(!Proc_IsOneCImagePath(nullptr));

    CHECK(Proc_NameEquals(L"C:\\a\\1cv8.exe", L"1cv8.exe"));
    CHECK(Proc_NameEquals(L"1cv8.exe", L"1cv8.exe"));
    CHECK(!Proc_NameEquals(L"1cv8s.exe", L"1cv8.exe"));

    CHECK(Proc_LooksLike1CVersionDir(L"8.3.27.2342"));
    CHECK(Proc_LooksLike1CVersionDir(L"8.3.10"));
    CHECK(!Proc_LooksLike1CVersionDir(L"common"));
    CHECK(!Proc_LooksLike1CVersionDir(L"bin"));
    CHECK(!Proc_LooksLike1CVersionDir(L"8"));
    CHECK(!Proc_LooksLike1CVersionDir(L"v8.3"));
}

void TestLauncherArgs() {
    std::vector<std::wstring> store;
    {
        auto argv = MakeArgv(store, {L"200", L"--designer"});
        LauncherOptions opt;
        CHECK(Launcher_ParseArgs(static_cast<int>(argv.size()), argv.data(), opt));
        CHECK(opt.dpiPercent == 200);
        CHECK(opt.designer);
        CHECK(!opt.useStart);
        CHECK(!opt.help);
        CHECK(opt.passthrough.empty());
    }
    {
        auto argv = MakeArgv(store, {L"--dpi=250", L"--exe=C:\\1cv8\\1cv8.exe", L"--no-log"});
        LauncherOptions opt;
        CHECK(Launcher_ParseArgs(static_cast<int>(argv.size()), argv.data(), opt));
        CHECK(opt.dpiPercent == 250);
        CHECK(opt.exePath == L"C:\\1cv8\\1cv8.exe");
        CHECK(opt.logEnabled == 0);
    }
    {
        auto argv = MakeArgv(store, {L"--exe", L"D:\\app\\1cestart.exe", L"--log=off", L"--start"});
        LauncherOptions opt;
        CHECK(Launcher_ParseArgs(static_cast<int>(argv.size()), argv.data(), opt));
        CHECK(opt.exePath == L"D:\\app\\1cestart.exe");
        CHECK(opt.useStart);
        CHECK(!opt.designer);
        CHECK(opt.logEnabled == 0);
    }
    {
        auto argv = MakeArgv(store, {L"200", L"--", L"/NAdmin", L"DESIGNER"});
        LauncherOptions opt;
        CHECK(Launcher_ParseArgs(static_cast<int>(argv.size()), argv.data(), opt));
        CHECK(opt.dpiPercent == 200);
        CHECK(opt.passthrough.size() == 2);
        CHECK(opt.passthrough[0] == L"/NAdmin");
        CHECK(opt.passthrough[1] == L"DESIGNER");
    }
    {
        auto argv = MakeArgv(store, {L"--help"});
        LauncherOptions opt;
        CHECK(Launcher_ParseArgs(static_cast<int>(argv.size()), argv.data(), opt));
        CHECK(opt.help);
    }
    {
        auto argv = MakeArgv(store, {L"--log=C:\\Temp\\shim.log"});
        LauncherOptions opt;
        CHECK(Launcher_ParseArgs(static_cast<int>(argv.size()), argv.data(), opt));
        CHECK(opt.logEnabled == 1);
        CHECK(opt.logPath == L"C:\\Temp\\shim.log");
    }
    {
        auto argv = MakeArgv(store, {L"--ini=D:\\cfg\\1c-dpi.ini", L"--exe=C:\\1cv8\\1cv8.exe"});
        LauncherOptions opt;
        CHECK(Launcher_ParseArgs(static_cast<int>(argv.size()), argv.data(), opt));
        CHECK(opt.iniPath == L"D:\\cfg\\1c-dpi.ini");
        CHECK(opt.exePath == L"C:\\1cv8\\1cv8.exe");
    }
    {
        auto argv = MakeArgv(store, {L"--ini", L"E:\\a\\1c-dpi.ini", L"--exe", L"E:\\1cestart.exe"});
        LauncherOptions opt;
        CHECK(Launcher_ParseArgs(static_cast<int>(argv.size()), argv.data(), opt));
        CHECK(opt.iniPath == L"E:\\a\\1c-dpi.ini");
        CHECK(opt.exePath == L"E:\\1cestart.exe");
    }
}

void TouchEmptyFile(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
    }
}

void TestResolveExe() {
    wchar_t tmp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tmp);
    const std::wstring dir = IO::Join(tmp, L"1c-dpi-resolve-test");
    CreateDirectoryW(dir.c_str(), nullptr);

    const std::wstring startExe = IO::Join(dir, L"1cestart.exe");
    const std::wstring platformExe = IO::Join(dir, L"1cv8.exe");
    const std::wstring cliExe = IO::Join(dir, L"cli-1cv8.exe");
    TouchEmptyFile(startExe);
    TouchEmptyFile(platformExe);
    TouchEmptyFile(cliExe);

    const std::wstring ini = IO::Join(dir, L"1c-dpi.ini");
    const std::wstring quotedStart = L"\"" + startExe + L"\"";
    WritePrivateProfileStringW(L"launcher", L"exe", startExe.c_str(), ini.c_str());
    WritePrivateProfileStringW(L"launcher", L"start_exe", startExe.c_str(), ini.c_str());
    WritePrivateProfileStringW(L"launcher", L"platform_exe", platformExe.c_str(), ini.c_str());

    CHECK(OneCLocator::ResolveExe(L"", ini, false, true) == startExe);
    CHECK(OneCLocator::ResolveExe(L"", ini, true, false) == platformExe);
    CHECK(OneCLocator::ResolveExe(cliExe, ini, true, false) == cliExe);
    CHECK(OneCLocator::WorkingDir(platformExe) == dir);

    WritePrivateProfileStringW(L"launcher", L"exe", quotedStart.c_str(), ini.c_str());
    CHECK(IO::ReadIniString(ini, L"launcher", L"exe") == startExe);

    const std::wstring missingIni = IO::Join(dir, L"missing.ini");
    CHECK(OneCLocator::ResolveExe(cliExe, missingIni, false, true) == cliExe);

    DeleteFileW(ini.c_str());
    DeleteFileW(startExe.c_str());
    DeleteFileW(platformExe.c_str());
    DeleteFileW(cliExe.c_str());
    RemoveDirectoryW(dir.c_str());
}

void TestIniPresent() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) {
        *slash = 0;
    }
    wchar_t ini[MAX_PATH] = {};
    swprintf_s(ini, L"%s\\1c-dpi.ini", path);
    const DWORD attr = GetFileAttributesW(ini);
    CHECK(attr != INVALID_FILE_ATTRIBUTES);
    CHECK((attr & FILE_ATTRIBUTE_DIRECTORY) == 0);

    const int dpi = static_cast<int>(GetPrivateProfileIntW(L"shim", L"dpi", 0, ini));
    CHECK(DpiMath_PercentInRange(dpi));
    CHECK(GetPrivateProfileIntW(L"shim", L"enabled", 0, ini) != 0);

    const std::wstring exe = IO::ReadIniString(ini, L"launcher", L"exe");
    const std::wstring start = IO::ReadIniString(ini, L"launcher", L"start_exe");
    const std::wstring platform = IO::ReadIniString(ini, L"launcher", L"platform_exe");
    CHECK(!exe.empty() || !start.empty());
    CHECK(!platform.empty());
    CHECK(OneCLocator::ResolveExe(L"", ini, false, true) == (!exe.empty() ? exe : start));
    CHECK(OneCLocator::ResolveExe(L"", ini, true, false) == platform);
}

} // namespace

bool InCi() {
    char buf[4] = {};
    return GetEnvironmentVariableA("CI", buf, ARRAYSIZE(buf)) > 0;
}

void WaitForAnyKey() {
    if (InCi()) {
        return;
    }

    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    const bool hasConsole = in != INVALID_HANDLE_VALUE && in != nullptr && GetConsoleMode(in, &mode);
    if (!hasConsole && !IsDebuggerPresent()) {
        return;
    }

    printf("Press any key to exit...");
    fflush(stdout);

    if (!hasConsole) {
        _getch();
        return;
    }

    SetConsoleMode(in, mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_MOUSE_INPUT
        | ENABLE_WINDOW_INPUT | ENABLE_PROCESSED_INPUT));
    FlushConsoleInputBuffer(in);

    for (;;) {
        INPUT_RECORD rec = {};
        DWORD read = 0;
        if (!ReadConsoleInputW(in, &rec, 1, &read) || read == 0) {
            _getch();
            break;
        }
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) {
            continue;
        }
        const WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
        if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_CAPITAL
            || vk == VK_NUMLOCK || vk == VK_SCROLL || vk == VK_LWIN || vk == VK_RWIN) {
            continue;
        }
        break;
    }
    SetConsoleMode(in, mode);
}

int wmain() {
    TestDpiMath();
    TestProcessNames();
    TestLauncherArgs();
    TestResolveExe();
    TestIniPresent();
    printf("%d passed, %d failed (%s)\n", g_passed, g_failed, Proc_ArchitectureName());
    WaitForAnyKey();
    return g_failed == 0 ? 0 : 1;
}
