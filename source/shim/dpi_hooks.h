#pragma once

#include <windows.h>

bool Hooks_Install();
void Hooks_Uninstall();
bool Hooks_AreInstalled();
void Hooks_TryLateApis();
