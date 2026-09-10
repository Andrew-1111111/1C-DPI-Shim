#include "process_utils.h"

#include <stdio.h>
#include <string.h>

namespace {

bool EqualsIgnoreCase(const wchar_t* a, const wchar_t* b) {
    if (!a || !b) {
        return false;
    }
    return _wcsicmp(a, b) == 0;
}

const wchar_t* FileNameOf(const wchar_t* path) {
    if (!path) {
        return L"";
    }
    const wchar_t* slash = wcsrchr(path, L'\\');
    const wchar_t* slash2 = wcsrchr(path, L'/');
    if (slash2 && (!slash || slash2 > slash)) {
        slash = slash2;
    }
    return slash ? slash + 1 : path;
}

bool MatchesBaseName(const wchar_t* name, const wchar_t* base) {
    if (!name || !base) {
        return false;
    }
    if (_wcsicmp(name, base) == 0) {
        return true;
    }
    wchar_t withExe[32] = {};
    swprintf_s(withExe, L"%s.exe", base);
    return _wcsicmp(name, withExe) == 0;
}

bool IsAllowedExeName(const wchar_t* name) {
    return MatchesBaseName(name, L"1cestart") ||
           MatchesBaseName(name, L"1cv8") ||
           MatchesBaseName(name, L"1cv8c") ||
           MatchesBaseName(name, L"1cv8s") ||
           MatchesBaseName(name, L"1cv8a");
}

} // namespace

bool Proc_GetExePath(wchar_t* out, size_t outCch) {
    if (!out || outCch == 0) {
        return false;
    }
    const DWORD n = GetModuleFileNameW(nullptr, out, static_cast<DWORD>(outCch));
    return n > 0 && n < outCch;
}

bool Proc_GetExeName(wchar_t* out, size_t outCch) {
    wchar_t path[MAX_PATH] = {};
    if (!Proc_GetExePath(path, MAX_PATH)) {
        return false;
    }
    const wchar_t* name = FileNameOf(path);
    wcsncpy_s(out, outCch, name, _TRUNCATE);
    return true;
}

bool Proc_NameEquals(const wchar_t* fileName, const wchar_t* expectedExe) {
    return EqualsIgnoreCase(FileNameOf(fileName), expectedExe);
}

bool Proc_IsOneCImagePath(const wchar_t* imagePath) {
    return IsAllowedExeName(FileNameOf(imagePath));
}

bool Proc_LooksLike1CVersionDir(const wchar_t* name) {
    if (!name || !name[0]) {
        return false;
    }
    int dots = 0;
    int digits = 0;
    for (const wchar_t* p = name; *p; ++p) {
        if (*p == L'.') {
            ++dots;
        } else if (*p >= L'0' && *p <= L'9') {
            ++digits;
        } else {
            return false;
        }
    }
    return dots >= 2 && digits >= 3;
}

bool Proc_IsAllowedTarget() {
    wchar_t name[MAX_PATH] = {};
    if (!Proc_GetExeName(name, MAX_PATH)) {
        return false;
    }
    return IsAllowedExeName(name);
}

bool Proc_IsStarterProcess() {
    wchar_t name[MAX_PATH] = {};
    if (!Proc_GetExeName(name, MAX_PATH)) {
        return false;
    }
    return EqualsIgnoreCase(name, L"1cestart.exe");
}

bool Proc_IsPlatformProcess() {
    wchar_t name[MAX_PATH] = {};
    if (!Proc_GetExeName(name, MAX_PATH)) {
        return false;
    }
    return EqualsIgnoreCase(name, L"1cv8.exe") ||
           EqualsIgnoreCase(name, L"1cv8c.exe") ||
           EqualsIgnoreCase(name, L"1cv8s.exe") ||
           EqualsIgnoreCase(name, L"1cv8a.exe");
}

bool Proc_IsWow64Process(HANDLE process, bool* isWow64) {
    if (!isWow64) {
        return false;
    }
    BOOL wow = FALSE;
    using Fn = BOOL(WINAPI*)(HANDLE, PBOOL);
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    auto fn = k32 ? reinterpret_cast<Fn>(GetProcAddress(k32, "IsWow64Process")) : nullptr;
    if (!fn) {
        *isWow64 = false;
        return true;
    }
    if (!fn(process, &wow)) {
        return false;
    }
    *isWow64 = wow != FALSE;
    return true;
}

bool Proc_Is32BitProcess(HANDLE process) {
#if defined(_WIN64)
    bool wow = false;
    if (!Proc_IsWow64Process(process, &wow)) {
        return false;
    }
    return wow;
#else
    (void)process;
    return true;
#endif
}

const char* Proc_ArchitectureName() {
#if defined(_M_X64) || defined(_WIN64)
    return "x64";
#else
    return "x86";
#endif
}

DWORD Proc_CurrentPid() {
    return GetCurrentProcessId();
}
