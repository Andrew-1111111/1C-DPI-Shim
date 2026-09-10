#include "dpi_shim.h"

#include <windows.h>

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(module);
        return DpiShim_OnAttach(module);
    case DLL_PROCESS_DETACH:
        DpiShim_OnDetach(reserved != nullptr);
        break;
    default:
        break;
    }
    return TRUE;
}
