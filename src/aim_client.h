#pragma once
#include <windows.h>

#define IOCTL_SET_AIM_CONFIG CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_AIM_CONFIG CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct AIM_CONFIG {
    float sensitivity;   // global scale applied in ring-0
    float xScale;        // per-axis X multiplier
    float yScale;        // per-axis Y multiplier
    float accelFactor;   // 0 = disabled; >0 = raw speed multiplier
    float accelCap;      // max delta capped at this value (0 = no cap)
    ULONG flags;         // bit 0: flip Y axis
};

namespace AimClient {
    bool Connect();
    void Disconnect();
    bool IsConnected();
    bool SetConfig(const AIM_CONFIG& cfg);
    bool GetConfig(AIM_CONFIG& cfg);
}
