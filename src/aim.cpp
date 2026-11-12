#include "aim.h"
#include <windows.h>
#include <cstring>
#include <cstdint>

namespace Aim {

void DisablePointerPrecision() {
    // Disable "Enhance pointer precision" (mouse acceleration)
    BOOL prevSpeed = 0;
    int params[3] = {0};
    SystemParametersInfoW(SPI_GETMOUSE, 0, params, 0);
    params[2] = 0; // disable acceleration
    SystemParametersInfoW(SPI_SETMOUSE, 0, params, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);

    // Also set via registry for persistence
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Control Panel\\Mouse",
                      0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        const wchar_t* zero = L"0";
        const wchar_t* one  = L"1";
        RegSetValueExW(hKey, L"MouseSpeed", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(zero),
                       static_cast<DWORD>((wcslen(zero) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"MouseThreshold1", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(zero),
                       static_cast<DWORD>((wcslen(zero) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"MouseThreshold2", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(zero),
                       static_cast<DWORD>((wcslen(zero) + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}

void ApplyRawInput() {
    // SmoothMouseXCurve and SmoothMouseYCurve control Windows' pointer ballistics.
    // Zeroing them removes all OS-side smoothing beyond the pointer speed setting.
    // These are 10-byte BINARY values that describe a piecewise linear curve.
    // All-zeros = identity: no ballistics applied.

    static const BYTE zeros[10] = {0};

    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Control Panel\\Mouse",
                      0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"SmoothMouseXCurve", 0, REG_BINARY, zeros, 10);
        RegSetValueExW(hKey, L"SmoothMouseYCurve", 0, REG_BINARY, zeros, 10);
        RegCloseKey(hKey);
    }

    // Set pointer speed to 6/11 (index 5, 0-based) — exact 1:1 mapping
    int speed = 6;
    SystemParametersInfoW(SPI_SETMOUSESPEED, 0,
                          reinterpret_cast<PVOID>(static_cast<uintptr_t>(speed)),
                          SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
}

void ZeroMouseCurve() {
    // Additional registry keys that some games / drivers read
    HKEY hKey = nullptr;

    // Per-user pointer settings backup path
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Windows",
                        0, nullptr, REG_OPTION_NON_VOLATILE,
                        KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD val = 0;
        RegSetValueExW(hKey, L"MouseSensitivity", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&val), sizeof(DWORD));
        RegCloseKey(hKey);
    }

    // Disable mouse trails (can add perceived input lag)
    SystemParametersInfoW(SPI_SETMOUSETRAILS, 0, nullptr,
                          SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);

    // Disable cursor shadow (small latency reduction on some configs)
    BOOL cursorShadow = FALSE;
    SystemParametersInfoW(SPI_SETCURSORSHADOW, 0, &cursorShadow,
                          SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
}

} // namespace Aim
