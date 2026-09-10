#pragma once

class Console {
public:
    static void Print(const wchar_t* fmt, ...);
    static void Ensure();
    static void ApplyIcon();
    static void Usage();
};
