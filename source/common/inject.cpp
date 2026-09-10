#include "inject.h"

#include <stddef.h>
#include <stdio.h>

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

} // namespace

bool Inject_LoadLibraryW(HANDLE process, const wchar_t* dllPath, DWORD timeoutMs, wchar_t* error, size_t errorCch) {
    if (!process || !dllPath || dllPath[0] == 0) {
        SetError(error, errorCch, L"Invalid inject arguments", 0);
        return false;
    }

    const size_t bytes = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        SetError(error, errorCch, L"VirtualAllocEx failed", GetLastError());
        return false;
    }

    SIZE_T written = 0;
    if (!WriteProcessMemory(process, remote, dllPath, bytes, &written) || written != bytes) {
        const DWORD err = GetLastError();
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        SetError(error, errorCch, L"WriteProcessMemory failed", err);
        return false;
    }

    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    auto loadLibraryW = k32 ? GetProcAddress(k32, "LoadLibraryW") : nullptr;
    if (!loadLibraryW) {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        SetError(error, errorCch, L"GetProcAddress(LoadLibraryW) failed", GetLastError());
        return false;
    }

    HANDLE thread = CreateRemoteThread(
        process, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibraryW),
        remote, 0, nullptr);
    if (!thread) {
        const DWORD err = GetLastError();
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        SetError(error, errorCch, L"CreateRemoteThread failed", err);
        return false;
    }

    const DWORD wait = WaitForSingleObject(thread, timeoutMs == 0 ? 15000 : timeoutMs);
    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);
    CloseHandle(thread);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);

    if (wait != WAIT_OBJECT_0) {
        SetError(error, errorCch, L"Timed out waiting for LoadLibraryW in the target process", 0);
        return false;
    }
    if (exitCode == 0) {
        SetError(error, errorCch, L"LoadLibraryW returned NULL in the target process", 0);
        return false;
    }
    if (error && errorCch) {
        error[0] = 0;
    }
    return true;
}

bool Inject_CreateAndInject(
    const wchar_t* exePath,
    wchar_t* commandLine,
    const wchar_t* workingDir,
    const wchar_t* dllPath,
    DWORD timeoutMs,
    PROCESS_INFORMATION* pi,
    wchar_t* error,
    size_t errorCch) {

    if (!exePath || !pi) {
        SetError(error, errorCch, L"Invalid CreateAndInject arguments", 0);
        return false;
    }

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION localPi = {};

    DWORD flags = CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT;
    if (!CreateProcessW(exePath, commandLine, nullptr, nullptr, FALSE, flags,
                        nullptr, workingDir, &si, &localPi)) {
        SetError(error, errorCch, L"CreateProcessW failed", GetLastError());
        return false;
    }

    wchar_t injectError[512] = {};
    if (!Inject_LoadLibraryW(localPi.hProcess, dllPath, timeoutMs, injectError, 512)) {
        TerminateProcess(localPi.hProcess, 1);
        CloseHandle(localPi.hThread);
        CloseHandle(localPi.hProcess);
        SetError(error, errorCch, injectError, 0);
        return false;
    }

    if (ResumeThread(localPi.hThread) == static_cast<DWORD>(-1)) {
        const DWORD err = GetLastError();
        TerminateProcess(localPi.hProcess, 1);
        CloseHandle(localPi.hThread);
        CloseHandle(localPi.hProcess);
        SetError(error, errorCch, L"ResumeThread failed", err);
        return false;
    }

    *pi = localPi;
    if (error && errorCch) {
        error[0] = 0;
    }
    return true;
}
