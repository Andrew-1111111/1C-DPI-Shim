#pragma once

#include <windows.h>

bool Log_Initialize(const wchar_t* path, bool enabled, bool verbose);
void Log_Shutdown();
bool Log_IsEnabled();
bool Log_IsVerbose();
void Log_SetEnabled(bool enabled);
void Log_SetVerbose(bool verbose);

void Log_Write(const char* fmt, ...);
void Log_WriteW(const wchar_t* fmt, ...);

// Rate-limited helper: logs the first `firstN` calls and every `everyN` thereafter.
bool Log_ShouldSample(volatile LONG* counter, LONG firstN, LONG everyN);

#define LOG_INFO(...)  do { if (Log_IsEnabled()) Log_Write(__VA_ARGS__); } while (0)
#define LOG_VERBOSE(...) do { if (Log_IsVerbose()) Log_Write(__VA_ARGS__); } while (0)
