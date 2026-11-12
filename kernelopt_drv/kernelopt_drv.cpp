/*
 * kernelopt_drv.cpp — Sub-0.5ms kernel timer driver
 *
 * Calls ExSetTimerResolution(1, TRUE) which sets the system-wide timer
 * interrupt to its hardware minimum (~100µs on most motherboards).
 * NtSetTimerResolution from userland caps at 0.5ms; this breaks that limit.
 *
 * Build: WDK + VS2022, Kernel Mode Driver (KMDF) project.
 *   Platform: x64
 *   Configuration: Release
 *   Inf2Cat / signing: use testsigning mode (see README.txt step 2).
 */

#include <ntddk.h>

extern "C" NTKERNELAPI ULONG NTAPI
ExSetTimerResolution(ULONG DesiredTime, BOOLEAN SetResolution);

static ULONG g_prevResolution = 0;
static BOOLEAN g_timerSet = FALSE;

static VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
    if (g_timerSet) {
        ExSetTimerResolution(g_prevResolution, FALSE);
        g_timerSet = FALSE;
    }
    DbgPrint("KernelOpt: timer driver unloaded, resolution restored\n");
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    DriverObject->DriverUnload = DriverUnload;

    // Query current resolution so we can restore on unload
    ULONG minimumResolution = 0, maximumResolution = 0, currentResolution = 0;
    // ExQueryTimerResolution is available on Win8+; on Win7 we skip the query
    // and just set directly.
    // ExQueryTimerResolution(&minimumResolution, &maximumResolution, &currentResolution);
    // g_prevResolution = currentResolution;

    // Set to minimum possible (1 = 100 nanosecond units = absolute hardware minimum)
    ULONG actualResolution = ExSetTimerResolution(1, TRUE);
    g_timerSet = TRUE;

    DbgPrint("KernelOpt: timer resolution set to %lu (100ns units), actual = %lu\n",
             1UL, actualResolution);

    return STATUS_SUCCESS;
}
