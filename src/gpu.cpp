#include "gpu.h"
#include "registry.h"
#include <windows.h>

void GPU::EnableHAGS() {
    // Hardware Accelerated GPU Scheduling — WDDM 2.7+, Win10 2004+
    Registry::SetDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
        L"HwSchMode", 2);
}

void GPU::DisableTDR() {
    Registry::SetDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
        L"TdrLevel", 0);
    Registry::SetDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
        L"TdrDelay", 60);
}

void GPU::SetNVMaxPerformance() {
    // NVIDIA PowerMizer — force max performance clocks
    HKEY hGPUs = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}",
        0, KEY_READ, &hGPUs) != ERROR_SUCCESS) return;

    wchar_t subkey[256];
    DWORD idx = 0, len = 256;
    while (RegEnumKeyExW(hGPUs, idx++, subkey, &len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        std::wstring path = std::wstring(
            L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\") + subkey;
        Registry::SetDWORD(HKEY_LOCAL_MACHINE, path, L"PerfLevelSrc", 0x3322);
        Registry::SetDWORD(HKEY_LOCAL_MACHINE, path, L"PowerMizerLevel", 1);
        Registry::SetDWORD(HKEY_LOCAL_MACHINE, path, L"PowerMizerLevelAC", 1);
        Registry::SetDWORD(HKEY_LOCAL_MACHINE, path, L"PowerMizerEnable", 1);
        len = 256;
    }
    RegCloseKey(hGPUs);
}
