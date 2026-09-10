#pragma once

#include <windows.h>

// Writes the shim module embedded in this EXE to %LOCALAPPDATA%\1C-DPI-Shim\
// (or %TEMP%\1C-DPI-Shim\). Reuses the file when contents already match.
bool Payload_EnsureShimDll(wchar_t* path, size_t pathCch, wchar_t* error, size_t errorCch);
