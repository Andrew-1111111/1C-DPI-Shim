#include "onec_locator.h"
#include "io.h"
#include "process_utils.h"

#include <windows.h>
#include <wchar.h>

std::wstring OneCLocator::FindLatest1cv8() {
    const wchar_t* roots[] = {
        L"C:\\Program Files (x86)\\1cv8",
        L"C:\\Program Files\\1cv8",
    };
    std::wstring bestVer;
    std::wstring bestExe;
    for (const wchar_t* root : roots) {
        if (!IO::DirExists(root)) {
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
            const std::wstring exe = IO::Join(IO::Join(root, fd.cFileName), L"bin\\1cv8.exe");
            if (!IO::FileExists(exe.c_str())) {
                continue;
            }
            if (bestVer.empty() || wcscmp(fd.cFileName, bestVer.c_str()) > 0) {
                bestVer = fd.cFileName;
                bestExe = exe;
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (IO::FileExists(L"C:\\Program Files (x86)\\1cv8\\8.3.27.2342\\bin\\1cv8.exe")) {
        return L"C:\\Program Files (x86)\\1cv8\\8.3.27.2342\\bin\\1cv8.exe";
    }
    return bestExe;
}
