#pragma once

#include <string>

class OneCLocator {
public:
    static std::wstring FindLatest1cv8();
    static std::wstring ResolveExe(const std::wstring& cliExe, const std::wstring& ini, bool designer, bool useStart);
    static std::wstring WorkingDir(const std::wstring& exe);
};
