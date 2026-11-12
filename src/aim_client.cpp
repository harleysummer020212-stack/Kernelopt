#include "aim_client.h"
#include <cstdio>

namespace AimClient {

static HANDLE g_hDevice = INVALID_HANDLE_VALUE;

bool Connect() {
    if (g_hDevice != INVALID_HANDLE_VALUE) return true;

    g_hDevice = CreateFileW(
        L"\\\\.\\KernelAim",
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    return g_hDevice != INVALID_HANDLE_VALUE;
}

void Disconnect() {
    if (g_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hDevice);
        g_hDevice = INVALID_HANDLE_VALUE;
    }
}

bool IsConnected() {
    return g_hDevice != INVALID_HANDLE_VALUE;
}

bool SetConfig(const AIM_CONFIG& cfg) {
    if (g_hDevice == INVALID_HANDLE_VALUE) return false;

    DWORD bytesReturned = 0;
    return DeviceIoControl(
        g_hDevice,
        IOCTL_SET_AIM_CONFIG,
        const_cast<AIM_CONFIG*>(&cfg), sizeof(AIM_CONFIG),
        nullptr, 0,
        &bytesReturned,
        nullptr
    ) == TRUE;
}

bool GetConfig(AIM_CONFIG& cfg) {
    if (g_hDevice == INVALID_HANDLE_VALUE) return false;

    DWORD bytesReturned = 0;
    return DeviceIoControl(
        g_hDevice,
        IOCTL_GET_AIM_CONFIG,
        nullptr, 0,
        &cfg, sizeof(AIM_CONFIG),
        &bytesReturned,
        nullptr
    ) == TRUE;
}

} // namespace AimClient
