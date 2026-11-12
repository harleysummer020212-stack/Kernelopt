#include "irq.h"
#include "registry.h"
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

void IRQ::SetIRQPriority() {
    // Elevate IRQ 8 and 9 to Priority 1 (highest)
    Registry::SetDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\PriorityControl",
        L"IRQ8Priority", 1);
    Registry::SetDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\PriorityControl",
        L"IRQ9Priority", 1);
}

void IRQ::EnableMSIMode() {
    // Enable Message Signaled Interrupts for PCI devices
    HKEY hPCI = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Enum\\PCI",
        0, KEY_READ, &hPCI) != ERROR_SUCCESS) return;

    wchar_t devKey[512];
    DWORD idx = 0, len = 512;
    while (RegEnumKeyExW(hPCI, idx++, devKey, &len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        std::wstring devPath = std::wstring(L"SYSTEM\\CurrentControlSet\\Enum\\PCI\\") + devKey;
        HKEY hDev = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, devPath.c_str(), 0, KEY_READ, &hDev) == ERROR_SUCCESS) {
            wchar_t instKey[512];
            DWORD idx2 = 0, len2 = 512;
            while (RegEnumKeyExW(hDev, idx2++, instKey, &len2, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                std::wstring intPath = devPath + L"\\" + instKey + L"\\Device Parameters\\Interrupt Management\\MessageSignaledInterruptProperties";
                Registry::SetDWORD(HKEY_LOCAL_MACHINE, intPath, L"MSISupported", 1);
                Registry::SetDWORD(HKEY_LOCAL_MACHINE, intPath, L"MessageNumberLimit", 1);
                len2 = 512;
            }
            RegCloseKey(hDev);
        }
        len = 512;
    }
    RegCloseKey(hPCI);
}

void IRQ::PatchUSBPollingRate() {
    // Set USB HID polling interval to 1ms (1000Hz)
    HKEY hHID = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Services\\mouhid\\Parameters",
        0, KEY_SET_VALUE, &hHID) == ERROR_SUCCESS) {
        DWORD val = 1;
        RegSetValueExW(hHID, L"MouseDataQueueSize", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hHID);
    }

    // HID device polling
    HKEY hClass = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Class\\{745a17a0-74d3-11d0-b6fe-00a0c90f57da}",
        0, KEY_READ, &hClass) != ERROR_SUCCESS) return;

    wchar_t subkey[256];
    DWORD idx = 0, len = 256;
    while (RegEnumKeyExW(hClass, idx++, subkey, &len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        std::wstring path = std::wstring(
            L"SYSTEM\\CurrentControlSet\\Control\\Class\\{745a17a0-74d3-11d0-b6fe-00a0c90f57da}\\") + subkey;
        Registry::SetDWORD(HKEY_LOCAL_MACHINE, path, L"PollingInterval", 1);
        len = 256;
    }
    RegCloseKey(hClass);
}
