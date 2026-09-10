#include "payload.h"

#include <stdio.h>
#include <string.h>
#include <vector>

namespace {

void SetError(wchar_t* error, size_t errorCch, const wchar_t* text, DWORD lastError) {
    if (!error || errorCch == 0) {
        return;
    }
    if (lastError == 0) {
        wcsncpy_s(error, errorCch, text, _TRUNCATE);
        return;
    }
    swprintf_s(error, errorCch, L"%s (GetLastError=%lu)", text, lastError);
}

bool JoinPath(wchar_t* out, size_t outCch, const wchar_t* dir, const wchar_t* file) {
    if (!out || outCch == 0 || !dir || !file) {
        return false;
    }
    wcsncpy_s(out, outCch, dir, _TRUNCATE);
    const size_t n = wcslen(out);
    if (n > 0 && out[n - 1] != L'\\' && out[n - 1] != L'/') {
        if (wcsncat_s(out, outCch, L"\\", _TRUNCATE) != 0) {
            return false;
        }
    }
    return wcsncat_s(out, outCch, file, _TRUNCATE) == 0;
}

bool GetModuleDir(wchar_t* dir, size_t cch) {
    wchar_t path[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return false;
    }
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) {
        return false;
    }
    *slash = 0;
    return wcsncpy_s(dir, cch, path, _TRUNCATE) == 0;
}

bool DirIsWritable(const wchar_t* dir) {
    wchar_t probe[MAX_PATH] = {};
    if (!JoinPath(probe, MAX_PATH, dir, L"1C_DPI_Shim.write-test")) {
        return false;
    }
    HANDLE file = CreateFileW(
        probe, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    CloseHandle(file);
    return true;
}

bool GetTempDir(wchar_t* dir, size_t cch) {
    const DWORD n = GetTempPathW(static_cast<DWORD>(cch), dir);
    if (n == 0 || n >= cch) {
        return false;
    }
    wchar_t* slash = wcsrchr(dir, L'\\');
    if (slash && slash[1] == 0) {
        *slash = 0;
    }
    return true;
}

bool GetPayloadDir(wchar_t* dir, size_t cch) {
    if (GetModuleDir(dir, cch) && DirIsWritable(dir)) {
        return true;
    }
    return GetTempDir(dir, cch);
}

void RemoveLegacyAppDataCache() {
    wchar_t root[MAX_PATH] = {};
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", root, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return;
    }
    wchar_t dir[MAX_PATH] = {};
    if (!JoinPath(dir, MAX_PATH, root, L"1C-DPI-Shim")) {
        return;
    }
    wchar_t pattern[MAX_PATH] = {};
    if (!JoinPath(pattern, MAX_PATH, dir, L"*")) {
        return;
    }
    WIN32_FIND_DATAW fd = {};
    HANDLE find = FindFirstFileW(pattern, &fd);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (fd.cFileName[0] == L'.') {
                continue;
            }
            wchar_t file[MAX_PATH] = {};
            if (JoinPath(file, MAX_PATH, dir, fd.cFileName)) {
                DeleteFileW(file);
            }
        } while (FindNextFileW(find, &fd));
        FindClose(find);
    }
    RemoveDirectoryW(dir);
}

bool ReadAll(const wchar_t* path, std::vector<unsigned char>& bytes) {
    HANDLE file = CreateFileW(
        path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > 64 * 1024 * 1024) {
        CloseHandle(file);
        return false;
    }
    bytes.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    const BOOL ok = bytes.empty() ||
        (ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) &&
         read == bytes.size());
    CloseHandle(file);
    return ok != FALSE;
}

bool SameContents(const wchar_t* path, const void* data, DWORD size) {
    std::vector<unsigned char> existing;
    if (!ReadAll(path, existing)) {
        return false;
    }
    return existing.size() == size &&
        (size == 0 || memcmp(existing.data(), data, size) == 0);
}

bool WriteAll(const wchar_t* path, const void* data, DWORD size) {
    HANDLE file = CreateFileW(
        path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(file, data, size, &written, nullptr) && written == size;
    CloseHandle(file);
    if (!ok) {
        DeleteFileW(path);
        return false;
    }
    return true;
}

} // namespace

bool Payload_EnsureShimDll(wchar_t* path, size_t pathCch, wchar_t* error, size_t errorCch) {
    if (!path || pathCch == 0) {
        SetError(error, errorCch, L"Invalid payload buffer", 0);
        return false;
    }
    path[0] = 0;
    RemoveLegacyAppDataCache();

    HMODULE exe = GetModuleHandleW(nullptr);
    HRSRC res = FindResourceW(exe, L"SHIMPAYLOAD", RT_RCDATA);
    if (!res) {
        SetError(error, errorCch, L"Embedded shim payload not found in 1C-DPI-Shim.exe", 0);
        return false;
    }
    const DWORD size = SizeofResource(exe, res);
    HGLOBAL glob = LoadResource(exe, res);
    const void* data = glob ? LockResource(glob) : nullptr;
    if (!data || size == 0) {
        SetError(error, errorCch, L"Embedded shim payload is empty", 0);
        return false;
    }

    wchar_t dir[MAX_PATH] = {};
    if (!GetPayloadDir(dir, MAX_PATH)) {
        SetError(error, errorCch, L"Failed to choose a writable folder for the embedded shim", GetLastError());
        return false;
    }

    wchar_t dest[MAX_PATH] = {};
    if (!JoinPath(dest, MAX_PATH, dir, L"1C_DPI_Shim.dll")) {
        SetError(error, errorCch, L"Payload path is too long", 0);
        return false;
    }

    if (SameContents(dest, data, size)) {
        wcsncpy_s(path, pathCch, dest, _TRUNCATE);
        if (error && errorCch) {
            error[0] = 0;
        }
        return true;
    }

    wchar_t tmp[MAX_PATH] = {};
    if (!JoinPath(tmp, MAX_PATH, dir, L"1C_DPI_Shim.dll.tmp")) {
        SetError(error, errorCch, L"Payload temp path is too long", 0);
        return false;
    }
    DeleteFileW(tmp);
    if (!WriteAll(tmp, data, size)) {
        SetError(error, errorCch, L"Failed to write embedded shim to disk", GetLastError());
        return false;
    }

    if (MoveFileExW(tmp, dest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
        wcsncpy_s(path, pathCch, dest, _TRUNCATE);
        if (error && errorCch) {
            error[0] = 0;
        }
        return true;
    }

    const DWORD moveErr = GetLastError();
    DeleteFileW(tmp);
    if (SameContents(dest, data, size)) {
        wcsncpy_s(path, pathCch, dest, _TRUNCATE);
        if (error && errorCch) {
            error[0] = 0;
        }
        return true;
    }

    wchar_t altName[64] = {};
    swprintf_s(altName, L"1C_DPI_Shim_%lu.dll", GetCurrentProcessId());
    wchar_t alt[MAX_PATH] = {};
    if (!JoinPath(alt, MAX_PATH, dir, altName) || !WriteAll(alt, data, size)) {
        SetError(error, errorCch, L"Failed to replace in-use shim payload", moveErr);
        return false;
    }
    wcsncpy_s(path, pathCch, alt, _TRUNCATE);
    if (error && errorCch) {
        error[0] = 0;
    }
    return true;
}
