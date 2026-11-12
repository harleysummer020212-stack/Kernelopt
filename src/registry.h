#pragma once
#include <windows.h>
#include <string>

namespace Registry {
    bool SetDWORD(HKEY root, const std::wstring& path, const std::wstring& name, DWORD value);
    bool SetSZ(HKEY root, const std::wstring& path, const std::wstring& name, const std::wstring& value);
    void ApplySchedulerTweaks();
    void ApplyBcdeditTweaks();
    void DisableDPCWatchdog();
    void DisableDeliveryOptimization();
}
