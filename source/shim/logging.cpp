#include "logging.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

namespace {

HANDLE g_file = INVALID_HANDLE_VALUE;
CRITICAL_SECTION g_cs;
bool g_csInit = false;
bool g_enabled = false;
bool g_verbose = false;

void EnsureCs() {
    if (!g_csInit) {
        InitializeCriticalSection(&g_cs);
        g_csInit = true;
    }
}

void WriteRaw(const char* data, DWORD len) {
    if (g_file == INVALID_HANDLE_VALUE || data == nullptr || len == 0) {
        return;
    }
    DWORD written = 0;
    WriteFile(g_file, data, len, &written, nullptr);
}

void Timestamp(char* buf, size_t bufSize) {
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    sprintf_s(buf, bufSize, "%04u-%02u-%02u %02u:%02u:%02u.%03u",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

} // namespace

bool Log_Initialize(const wchar_t* path, bool enabled, bool verbose) {
    EnsureCs();
    EnterCriticalSection(&g_cs);
    g_enabled = enabled;
    g_verbose = verbose && enabled;
    if (!enabled || path == nullptr || path[0] == 0) {
        LeaveCriticalSection(&g_cs);
        return true;
    }

    wchar_t dir[MAX_PATH] = {};
    wcsncpy_s(dir, path, _TRUNCATE);
    wchar_t* slash = wcsrchr(dir, L'\\');
    if (slash) {
        *slash = 0;
        CreateDirectoryW(dir, nullptr);
    }

    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        LeaveCriticalSection(&g_cs);
        return false;
    }
    if (g_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_file);
    }
    g_file = file;
    LeaveCriticalSection(&g_cs);
    return true;
}

void Log_Shutdown() {
    if (!g_csInit) {
        return;
    }
    EnterCriticalSection(&g_cs);
    if (g_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_file);
        g_file = INVALID_HANDLE_VALUE;
    }
    g_enabled = false;
    g_verbose = false;
    LeaveCriticalSection(&g_cs);
}

bool Log_IsEnabled() { return g_enabled; }
bool Log_IsVerbose() { return g_verbose; }
void Log_SetEnabled(bool enabled) { g_enabled = enabled; }
void Log_SetVerbose(bool verbose) { g_verbose = verbose && g_enabled; }

void Log_Write(const char* fmt, ...) {
    if (!g_enabled || fmt == nullptr) {
        return;
    }

    char timeBuf[64] = {};
    Timestamp(timeBuf, sizeof(timeBuf));

    char body[1600] = {};
    va_list args;
    va_start(args, fmt);
    vsprintf_s(body, fmt, args);
    va_end(args);

    char line[1800] = {};
    const int n = sprintf_s(line, "[%s] [pid %lu] %s\r\n", timeBuf, GetCurrentProcessId(), body);
    if (n <= 0) {
        return;
    }

    EnsureCs();
    EnterCriticalSection(&g_cs);
    WriteRaw(line, static_cast<DWORD>(n));
    LeaveCriticalSection(&g_cs);
}

void Log_WriteW(const wchar_t* fmt, ...) {
    if (!g_enabled || fmt == nullptr) {
        return;
    }
    wchar_t bodyW[1200] = {};
    va_list args;
    va_start(args, fmt);
    vswprintf_s(bodyW, fmt, args);
    va_end(args);

    char bodyA[1200] = {};
    WideCharToMultiByte(CP_UTF8, 0, bodyW, -1, bodyA, sizeof(bodyA), nullptr, nullptr);
    Log_Write("%s", bodyA);
}

bool Log_ShouldSample(volatile LONG* counter, LONG firstN, LONG everyN) {
    const LONG n = InterlockedIncrement(counter);
    if (n <= firstN) {
        return true;
    }
    if (everyN <= 0) {
        return false;
    }
    return (n % everyN) == 0;
}
