#include "registry.h"
#include <windows.h>
#include <string>

bool Registry::SetDWORD(HKEY root, const std::wstring& path, const std::wstring& name, DWORD value) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(root, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return false;
    bool ok = RegSetValueExW(hKey, name.c_str(), 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD)) == ERROR_SUCCESS;
    RegCloseKey(hKey);
    return ok;
}

bool Registry::SetSZ(HKEY root, const std::wstring& path, const std::wstring& name, const std::wstring& value) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(root, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return false;
    bool ok = RegSetValueExW(hKey, name.c_str(), 0, REG_SZ,
        (const BYTE*)value.c_str(), (DWORD)((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    RegCloseKey(hKey);
    return ok;
}

void Registry::ApplySchedulerTweaks() {
    // Win32PrioritySeparation = 0x26 — short variable quanta, max foreground boost
    SetDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\PriorityControl",
        L"Win32PrioritySeparation", 0x26);

    // Disable NTFS last access timestamp
    SetDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\FileSystem",
        L"NtfsDisableLastAccessUpdate", 1);

    // LargeSystemCache = 0 — favor app memory
    SetDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management",
        L"LargeSystemCache", 0);
}

void Registry::ApplyBcdeditTweaks() {
    // Disable dynamic tick
    system("bcdedit /set disabledynamictick yes >nul 2>&1");
    // Disable platform clock (HPET)
    system("bcdedit /set useplatformclock false >nul 2>&1");
    // TSC sync enhanced
    system("bcdedit /set tscsyncpolicy enhanced >nul 2>&1");
}

void Registry::DisableDPCWatchdog() {
    SetDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\kernel",
        L"DpcWatchdogProfileOffset", 0);
    SetDWORD(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\kernel",
        L"DpcTimeout", 0);
}

void Registry::DisableDeliveryOptimization() {
    SetDWORD(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Policies\\Microsoft\\Windows\\DeliveryOptimization",
        L"DODownloadMode", 0);
}
