#pragma once

#include <windows.h>

bool Proc_GetExeName(wchar_t* out, size_t outCch);
bool Proc_GetExePath(wchar_t* out, size_t outCch);
bool Proc_IsAllowedTarget();
bool Proc_IsStarterProcess();
bool Proc_IsPlatformProcess();
bool Proc_NameEquals(const wchar_t* fileName, const wchar_t* expectedExe);
bool Proc_IsOneCImagePath(const wchar_t* imagePath);
bool Proc_LooksLike1CVersionDir(const wchar_t* name);
bool Proc_IsWow64Process(HANDLE process, bool* isWow64);
bool Proc_Is32BitProcess(HANDLE process);
const char* Proc_ArchitectureName();
DWORD Proc_CurrentPid();
