/*
 * aim_drv.cpp — Ring-0 mouse filter driver
 *
 * Attaches as a WDM upper filter over mouclass/PointerClass0.
 * Intercepts every MOUSE_INPUT_DATA packet and applies the AIM_CONFIG
 * (sensitivity scale, per-axis scale, optional acceleration cap).
 *
 * Communicates with KernelOpt.exe via a named device: \\.\KernelAim
 * IOCTL_SET_AIM_CONFIG  (0x800) — push new config from userland
 * IOCTL_GET_AIM_CONFIG  (0x801) — read current config
 *
 * Build: WDK + VS2022, Kernel Mode Driver (KMDF) project.
 *   Platform: x64  |  Configuration: Release
 *   Signing: testsigning mode (README.txt step 2)
 */

#include <ntddk.h>
#include <wdm.h>
#include <kbdmou.h>   // MOUSE_INPUT_DATA — part of WDK

// ─── IOCTL codes (must match aim_client.h) ───────────────────────────────────

#define IOCTL_SET_AIM_CONFIG CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_AIM_CONFIG CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

// ─── Config ──────────────────────────────────────────────────────────────────

typedef struct _AIM_CONFIG {
    FLOAT sensitivity;
    FLOAT xScale;
    FLOAT yScale;
    FLOAT accelFactor;
    FLOAT accelCap;
    ULONG flags;        // bit 0: flip Y
} AIM_CONFIG;

static AIM_CONFIG g_config = {1.0f, 1.0f, 1.0f, 0.0f, 50.0f, 0};
static KSPIN_LOCK g_cfgLock;

// ─── Device / filter state ───────────────────────────────────────────────────

typedef struct _FILTER_EXT {
    PDEVICE_OBJECT  LowerDevice;
    PDEVICE_OBJECT  ControlDevice;   // \Device\KernelAim
} FILTER_EXT, * PFILTER_EXT;

static PDEVICE_OBJECT g_filterDevice  = nullptr;
static PDEVICE_OBJECT g_controlDevice = nullptr;
static PDRIVER_OBJECT g_driverObject  = nullptr;

// ─── Read completion (the hot path) ─────────────────────────────────────────

static NTSTATUS ReadCompletion(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context) {
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Context);

    if (Irp->IoStatus.Status == STATUS_SUCCESS) {
        PMOUSE_INPUT_DATA pData = (PMOUSE_INPUT_DATA)Irp->AssociatedIrp.SystemBuffer;
        ULONG numPackets = (ULONG)(Irp->IoStatus.Information / sizeof(MOUSE_INPUT_DATA));

        KIRQL irql;
        KeAcquireSpinLock(&g_cfgLock, &irql);
        AIM_CONFIG cfg = g_config;
        KeReleaseSpinLock(&g_cfgLock, irql);

        for (ULONG i = 0; i < numPackets; i++) {
            FLOAT dx = (FLOAT)pData[i].LastX;
            FLOAT dy = (FLOAT)pData[i].LastY;

            // Apply sensitivity scale
            dx *= cfg.sensitivity * cfg.xScale;
            dy *= cfg.sensitivity * cfg.yScale;

            // Optional acceleration cap
            if (cfg.accelFactor > 0.0f) {
                FLOAT speed = dx * dx + dy * dy;   // squared speed is fine for capping
                if (cfg.accelCap > 0.0f) {
                    if (speed > cfg.accelCap * cfg.accelCap) {
                        FLOAT scale = cfg.accelCap / (FLOAT)__builtin_sqrtf(speed);
                        dx *= scale;
                        dy *= scale;
                    }
                }
            }

            // Flip Y if requested
            if (cfg.flags & 1) dy = -dy;

            pData[i].LastX = (LONG)dx;
            pData[i].LastY = (LONG)dy;
        }
    }

    if (Irp->PendingReturned)
        IoMarkIrpPending(Irp);

    return Irp->IoStatus.Status;
}

// ─── Pass-through read (attach filter to read IRPs) ─────────────────────────

static NTSTATUS DispatchRead(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PFILTER_EXT ext = (PFILTER_EXT)DeviceObject->DeviceExtension;
    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(Irp, ReadCompletion, nullptr, TRUE, TRUE, TRUE);
    return IoCallDriver(ext->LowerDevice, Irp);
}

// ─── Pass-through for all other IRPs ────────────────────────────────────────

static NTSTATUS DispatchPassthrough(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PFILTER_EXT ext = (PFILTER_EXT)DeviceObject->DeviceExtension;
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(ext->LowerDevice, Irp);
}

// ─── IOCTL handler (control device) ─────────────────────────────────────────

static NTSTATUS DispatchControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG code    = stack->Parameters.DeviceIoControl.IoControlCode;
    ULONG inLen   = stack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen  = stack->Parameters.DeviceIoControl.OutputBufferLength;
    PVOID buf     = Irp->AssociatedIrp.SystemBuffer;

    NTSTATUS status = STATUS_SUCCESS;
    ULONG_PTR info  = 0;

    if (code == IOCTL_SET_AIM_CONFIG) {
        if (inLen >= sizeof(AIM_CONFIG)) {
            KIRQL irql;
            KeAcquireSpinLock(&g_cfgLock, &irql);
            RtlCopyMemory(&g_config, buf, sizeof(AIM_CONFIG));
            KeReleaseSpinLock(&g_cfgLock, irql);
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
    } else if (code == IOCTL_GET_AIM_CONFIG) {
        if (outLen >= sizeof(AIM_CONFIG)) {
            KIRQL irql;
            KeAcquireSpinLock(&g_cfgLock, &irql);
            RtlCopyMemory(buf, &g_config, sizeof(AIM_CONFIG));
            KeReleaseSpinLock(&g_cfgLock, irql);
            info = sizeof(AIM_CONFIG);
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
    } else {
        status = STATUS_INVALID_DEVICE_REQUEST;
    }

    Irp->IoStatus.Status      = status;
    Irp->IoStatus.Information = info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

// ─── Create / Close for control device ──────────────────────────────────────

static NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// ─── Unload ──────────────────────────────────────────────────────────────────

static VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);

    // Detach and delete filter device
    if (g_filterDevice) {
        PFILTER_EXT ext = (PFILTER_EXT)g_filterDevice->DeviceExtension;
        if (ext->LowerDevice) IoDetachDevice(ext->LowerDevice);
        IoDeleteDevice(g_filterDevice);
        g_filterDevice = nullptr;
    }

    // Delete control device symlink and device
    UNICODE_STRING symLink;
    RtlInitUnicodeString(&symLink, L"\\DosDevices\\KernelAim");
    IoDeleteSymbolicLink(&symLink);

    if (g_controlDevice) {
        IoDeleteDevice(g_controlDevice);
        g_controlDevice = nullptr;
    }

    DbgPrint("KernelAim: unloaded\n");
}

// ─── DriverEntry ─────────────────────────────────────────────────────────────

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    g_driverObject = DriverObject;
    KeInitializeSpinLock(&g_cfgLock);

    // Set dispatch routines for all major functions
    for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
        DriverObject->MajorFunction[i] = DispatchPassthrough;

    DriverObject->MajorFunction[IRP_MJ_READ]           = DispatchRead;
    DriverObject->MajorFunction[IRP_MJ_CREATE]         = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]          = DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchControl;
    DriverObject->DriverUnload                         = DriverUnload;

    // ── Create control device (\Device\KernelAim / \\.\KernelAim) ──────────
    UNICODE_STRING devName;
    RtlInitUnicodeString(&devName, L"\\Device\\KernelAim");
    NTSTATUS status = IoCreateDevice(DriverObject, 0, &devName,
                                     FILE_DEVICE_UNKNOWN, 0, FALSE, &g_controlDevice);
    if (!NT_SUCCESS(status)) {
        DbgPrint("KernelAim: failed to create control device: %08X\n", status);
        return status;
    }
    g_controlDevice->Flags |= DO_BUFFERED_IO;
    g_controlDevice->Flags &= ~DO_DEVICE_INITIALIZING;

    UNICODE_STRING symLink;
    RtlInitUnicodeString(&symLink, L"\\DosDevices\\KernelAim");
    status = IoCreateSymbolicLink(&symLink, &devName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(g_controlDevice);
        g_controlDevice = nullptr;
        return status;
    }

    // ── Attach to mouclass (PointerClass0) ──────────────────────────────────
    UNICODE_STRING mouseDevName;
    RtlInitUnicodeString(&mouseDevName, L"\\Device\\PointerClass0");

    status = IoCreateDevice(DriverObject, sizeof(FILTER_EXT),
                            nullptr, FILE_DEVICE_MOUSE,
                            FILE_DEVICE_SECURE_OPEN, FALSE, &g_filterDevice);
    if (!NT_SUCCESS(status)) {
        DbgPrint("KernelAim: failed to create filter device: %08X\n", status);
        IoDeleteSymbolicLink(&symLink);
        IoDeleteDevice(g_controlDevice);
        g_controlDevice = nullptr;
        return status;
    }

    PFILTER_EXT ext = (PFILTER_EXT)g_filterDevice->DeviceExtension;
    RtlZeroMemory(ext, sizeof(FILTER_EXT));
    ext->ControlDevice = g_controlDevice;

    PDEVICE_OBJECT lowerDevice = nullptr;
    status = IoAttachDevice(g_filterDevice, &mouseDevName, &lowerDevice);
    if (!NT_SUCCESS(status)) {
        DbgPrint("KernelAim: IoAttachDevice failed: %08X — mouclass not present?\n", status);
        IoDeleteDevice(g_filterDevice);
        g_filterDevice = nullptr;
        IoDeleteSymbolicLink(&symLink);
        IoDeleteDevice(g_controlDevice);
        g_controlDevice = nullptr;
        return status;
    }

    ext->LowerDevice = lowerDevice;

    // Propagate flags from lower device
    g_filterDevice->DeviceType   = lowerDevice->DeviceType;
    g_filterDevice->Characteristics = lowerDevice->Characteristics;
    g_filterDevice->Flags       |= lowerDevice->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO);
    g_filterDevice->Flags       &= ~DO_DEVICE_INITIALIZING;

    DbgPrint("KernelAim: loaded, attached to PointerClass0\n");
    return STATUS_SUCCESS;
}
