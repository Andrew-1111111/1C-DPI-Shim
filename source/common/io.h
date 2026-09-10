#pragma once

#include <string>

class IO {
public:
    static bool FileExists(const wchar_t* path);
    static bool DirExists(const wchar_t* path);
    static std::wstring Join(const std::wstring& a, const wchar_t* b);
    static std::wstring ModuleDir();
    static std::wstring ReadIniString(const std::wstring& ini, const wchar_t* section, const wchar_t* key);
    static std::wstring Quote(const std::wstring& s);
};
