#include "process.h"
#include <windows.h>
#include <tlhelp32.h>

void Process::SetRealtimePriority() {
    HANDLE hProc = GetCurrentProcess();
    SetPriorityClass(hProc, REALTIME_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    // Lock pages in memory privilege
    HANDLE hToken = nullptr;
    if (OpenProcessToken(hProc, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        LUID luid{};
        if (LookupPrivilegeValueW(nullptr, L"SeLockMemoryPrivilege", &luid)) {
            TOKEN_PRIVILEGES tp{};
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
        }
        CloseHandle(hToken);
    }

    // Disable priority boost
    SetProcessPriorityBoost(hProc, TRUE);
}

void Process::IsolateGameCores(HANDLE gameProc) {
    if (!gameProc) return;

    ULONG bufferSize = 0;
    GetSystemCpuSetInformation(nullptr, 0, &bufferSize, GetCurrentProcess(), 0);
    if (!bufferSize) return;

    auto buffer = std::make_unique<BYTE[]>(bufferSize);
    if (!GetSystemCpuSetInformation(
        reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buffer.get()),
        bufferSize, &bufferSize, GetCurrentProcess(), 0)) return;

    ULONG cpuCount = 0;
    PSYSTEM_CPU_SET_INFORMATION info = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buffer.get());
    ULONG totalSize = bufferSize;
    while (totalSize >= sizeof(SYSTEM_CPU_SET_INFORMATION)) {
        if (info->Type == CpuSetInformation) cpuCount++;
        totalSize -= info->Size;
        info = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(
            reinterpret_cast<BYTE*>(info) + info->Size);
    }

    if (cpuCount < 4) return;

    // Give game all cores, system processes get cores 0-1 only
    ULONG halfCores = cpuCount / 2;
    std::vector<ULONG> gameCpuSets, sysCpuSets;

    info = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buffer.get());
    totalSize = bufferSize;
    ULONG idx = 0;
    while (totalSize >= sizeof(SYSTEM_CPU_SET_INFORMATION)) {
        if (info->Type == CpuSetInformation) {
            if (idx < halfCores) sysCpuSets.push_back(info->CpuSet.Id);
            gameCpuSets.push_back(info->CpuSet.Id);
            idx++;
        }
        totalSize -= info->Size;
        info = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(
            reinterpret_cast<BYTE*>(info) + info->Size);
    }

    SetProcessDefaultCpuSets(gameProc, gameCpuSets.data(), (ULONG)gameCpuSets.size());
}
