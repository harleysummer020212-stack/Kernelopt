#include "mmcss.h"
#include "registry.h"
#include <windows.h>
#include <avrt.h>
#pragma comment(lib, "avrt.lib")

void MMCSS::ApplyGamingProfile() {
    const std::wstring base = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile";

    // Global MMCSS settings
    Registry::SetDWORD(HKEY_LOCAL_MACHINE, base, L"SystemResponsiveness", 0);
    Registry::SetDWORD(HKEY_LOCAL_MACHINE, base, L"NetworkThrottlingIndex", 0xFFFFFFFF);

    // Games task profile
    const std::wstring games = base + L"\\Tasks\\Games";
    Registry::SetDWORD(HKEY_LOCAL_MACHINE, games, L"GPU Priority", 8);
    Registry::SetDWORD(HKEY_LOCAL_MACHINE, games, L"Priority", 6);
    Registry::SetDWORD(HKEY_LOCAL_MACHINE, games, L"Scheduling Category", 2); // High
    Registry::SetDWORD(HKEY_LOCAL_MACHINE, games, L"SFIO Priority", 1);       // High
    Registry::SetSZ(HKEY_LOCAL_MACHINE, games, L"Affinity", L"0");
    Registry::SetSZ(HKEY_LOCAL_MACHINE, games, L"Background Only", L"False");
    Registry::SetDWORD(HKEY_LOCAL_MACHINE, games, L"Clock Rate", 10000); // 1ms
}

void MMCSS::RegisterCurrentThread() {
    DWORD taskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristicsW(L"Games", &taskIndex);
    if (hTask) {
        AvSetMmThreadPriority(hTask, AVRT_PRIORITY_CRITICAL);
    }
}
