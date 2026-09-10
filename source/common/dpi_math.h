#pragma once

#include <windows.h>

constexpr int DpiMath_MinPercent = 100;
constexpr int DpiMath_MaxPercent = 500;

inline UINT DpiMath_PercentToDpi(int percent) {
    if (percent <= 0) {
        return 96;
    }
    return static_cast<UINT>((96 * percent + 50) / 100);
}

inline int DpiMath_DpiToPercent(UINT dpi) {
    if (dpi == 0) {
        return 100;
    }
    return static_cast<int>((dpi * 100 + 48) / 96);
}

inline int DpiMath_Scale(int value, UINT fromDpi, UINT toDpi) {
    if (fromDpi == 0 || toDpi == 0 || fromDpi == toDpi) {
        return value;
    }
    return MulDiv(value, static_cast<int>(toDpi), static_cast<int>(fromDpi));
}

inline bool DpiMath_PercentInRange(int percent) {
    switch (percent) {
    case 100:
    case 125:
    case 150:
    case 175:
    case 200:
    case 225:
    case 250:
    case 300:
    case 400:
    case 500:
        return true;
    default:
        return false;
    }
}
