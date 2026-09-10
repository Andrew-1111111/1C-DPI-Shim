#pragma once

#include <windows.h>

// Writes the embedded shim DLL next to this EXE (or %TEMP% if that folder
// is not writable). Reuses the file when contents already match.
bool Payload_EnsureShimDll(wchar_t* path, size_t pathCch, wchar_t* error, size_t errorCch);
