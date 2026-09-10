#include "io.h"

#include <windows.h>
#include <wchar.h>

bool IO::FileExists(const wchar_t* path) {
    if (!path || path[0] == 0) {
        return false;
    }
    const DWORD attr = GetFileAttributesW(path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool IO::DirExists(const wchar_t* path) {
    if (!path || path[0] == 0) {
        return false;
    }
    const DWORD attr = GetFileAttributesW(path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring IO::Join(const std::wstring& a, const wchar_t* b) {
    if (!b) {
        return a;
    }
    if (a.empty()) {
        return b;
    }
    if (a.back() == L'\\' || a.back() == L'/') {
        return a + b;
    }
    return a + L"\\" + b;
}

std::wstring IO::ModuleDir() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) {
        *slash = 0;
    }
    return path;
}

std::wstring IO::ReadIniString(const std::wstring& ini, const wchar_t* section, const wchar_t* key) {
    if (ini.empty() || !section || !key || !FileExists(ini.c_str())) {
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

std::wstring IO::Quote(const std::wstring& s) {
    if (s.find(L' ') == std::wstring::npos) {
        return s;
    }
    return L"\"" + s + L"\"";
}
