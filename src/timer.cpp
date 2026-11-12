#include "timer.h"
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

typedef NTSTATUS(NTAPI* pNtSetTimerResolution)(ULONG, BOOLEAN, PULONG);

void Timer::SetMaxResolution() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return;
    auto NtSetTimerResolution = (pNtSetTimerResolution)GetProcAddress(ntdll, "NtSetTimerResolution");
    if (!NtSetTimerResolution) {
        timeBeginPeriod(1);
        return;
    }
    ULONG actualResolution = 0;
    NTSTATUS status = NtSetTimerResolution(5000, TRUE, &actualResolution);
    if (status != 0) {
        timeBeginPeriod(1);
    }
}

void Timer::Restore() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return;
    auto NtSetTimerResolution = (pNtSetTimerResolution)GetProcAddress(ntdll, "NtSetTimerResolution");
    if (NtSetTimerResolution) {
        ULONG actualResolution = 0;
        NtSetTimerResolution(156250, FALSE, &actualResolution);
    }
    timeEndPeriod(1);
}
