#pragma once

#include <windows.h>

// Injects `dllPath` into `process` via CreateRemoteThread(LoadLibraryW).
// The target must match the injector bitness (this project is Win32 / x86).
bool Inject_LoadLibraryW(HANDLE process, const wchar_t* dllPath, DWORD timeoutMs, wchar_t* error, size_t errorCch);

// CREATE_SUSPENDED + inject + resume helper used by the launcher.
bool Inject_CreateAndInject(
    const wchar_t* exePath,
    wchar_t* commandLine, // CreateProcess may modify this buffer
    const wchar_t* workingDir,
    const wchar_t* dllPath,
    DWORD timeoutMs,
    PROCESS_INFORMATION* pi,
    wchar_t* error,
    size_t errorCch);
