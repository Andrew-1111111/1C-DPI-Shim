#pragma once

#include <windows.h>

struct ShimConfig {
    bool enabled = true;
    bool logEnabled = true;
    bool verboseLog = true;
    bool scaleSystemMetrics = false;
    bool scaleNonClientMetrics = false;
    bool scaleStockFonts = true;
    bool blockPerMonitor = true;
    int dpiPercent = 200;
    UINT virtualDpi = 192;
    UINT systemDpi = 96;
    wchar_t logPath[MAX_PATH] = {};
    wchar_t iniPath[MAX_PATH] = {};
    wchar_t dllPath[MAX_PATH] = {};
    wchar_t launchExe[MAX_PATH] = {};
};

ShimConfig& Shim_Config();
HMODULE Shim_Module();

BOOL DpiShim_OnAttach(HMODULE module);
void DpiShim_OnDetach(bool processExit);

UINT Shim_PercentToDpi(int percent);
int Shim_DpiToPercent(UINT dpi);
UINT Shim_VirtualDpi();
UINT Shim_SystemDpi();
bool Shim_ShouldSpoof();
UINT Shim_SpoofDpi(UINT originalDpi);
int Shim_ScaleFromSystem(int value);

const char* Shim_AwarenessName(HANDLE context);
void Shim_DescribeAwareness(char* out, size_t outBytes);
