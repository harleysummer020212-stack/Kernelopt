#include "power.h"
#include <windows.h>

void Power::CreateUltimatePerformanceScheme() {
    // Duplicate Ultimate Performance scheme (Win10 — not built-in, must duplicate from GUID)
    system("powercfg /duplicatescheme e9a42b02-d5df-448d-aa00-03f14749eb61 >nul 2>&1");
    // List schemes, find the new one and activate it
    // Ultimate Performance GUID
    system("powercfg /setactive e9a42b02-d5df-448d-aa00-03f14749eb61 >nul 2>&1");
    // Disable sleep
    system("powercfg /change standby-timeout-ac 0 >nul 2>&1");
    system("powercfg /change hibernate-timeout-ac 0 >nul 2>&1");
    // Disable USB selective suspend
    system("powercfg /setacvalueindex SCHEME_CURRENT 2a737441-1930-4402-8d77-b2bebba308a3 48e6b7a6-50f5-4782-a5d4-53bb8f07e226 0 >nul 2>&1");
    system("powercfg /setactive SCHEME_CURRENT >nul 2>&1");
}

void Power::DisableSleep() {
    system("powercfg /change standby-timeout-ac 0 >nul 2>&1");
    system("powercfg /change monitor-timeout-ac 0 >nul 2>&1");
    system("powercfg /hibernate off >nul 2>&1");
}
