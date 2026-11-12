#include "network.h"
#include "registry.h"
#include <windows.h>
#include <iphlpapi.h>
#include <setupapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "setupapi.lib")

void Network::DisableNagle() {
    // Enumerate all TCP interfaces and set TcpAckFrequency + TCPNoDelay
    HKEY hInterfaces = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces",
        0, KEY_READ, &hInterfaces) != ERROR_SUCCESS) return;

    wchar_t subkey[256];
    DWORD idx = 0, len = 256;
    while (RegEnumKeyExW(hInterfaces, idx++, subkey, &len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        std::wstring path = std::wstring(L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\") + subkey;
        Registry::SetDWORD(HKEY_LOCAL_MACHINE, path, L"TcpAckFrequency", 1);
        Registry::SetDWORD(HKEY_LOCAL_MACHINE, path, L"TCPNoDelay", 1);
        len = 256;
    }
    RegCloseKey(hInterfaces);
}

void Network::DisableAutoTuning() {
    system("netsh int tcp set global autotuninglevel=disabled >nul 2>&1");
    system("netsh int tcp set global ecncapability=disabled >nul 2>&1");
    system("netsh int tcp set global rss=enabled >nul 2>&1");
}

void Network::DisableQoSThrottle() {
    Registry::SetDWORD(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Policies\\Microsoft\\Windows\\Psched",
        L"NonBestEffortLimit", 0);
}

void Network::DisableInterruptModeration() {
    // Enumerate NIC class devices and set interrupt moderation = disabled
    HKEY hNICs = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e972-e325-11ce-bfc1-08002be10318}",
        0, KEY_READ, &hNICs) != ERROR_SUCCESS) return;

    wchar_t subkey[256];
    DWORD idx = 0, len = 256;
    while (RegEnumKeyExW(hNICs, idx++, subkey, &len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        std::wstring path = std::wstring(
            L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e972-e325-11ce-bfc1-08002be10318}\\") + subkey;
        Registry::SetDWORD(HKEY_LOCAL_MACHINE, path, L"*InterruptModeration", 0);
        Registry::SetDWORD(HKEY_LOCAL_MACHINE, path, L"*ReceiveBuffers", 256);
        Registry::SetDWORD(HKEY_LOCAL_MACHINE, path, L"*TransmitBuffers", 64);
        len = 256;
    }
    RegCloseKey(hNICs);
}
