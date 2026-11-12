#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>

// ImGui
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#include "timer.h"
#include "registry.h"
#include "power.h"
#include "network.h"
#include "gpu.h"
#include "irq.h"
#include "mmcss.h"
#include "process.h"
#include "aim.h"
#include "aim_client.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

// Forward declare
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ─── State ────────────────────────────────────────────────────────────────────

struct TweakState {
    bool timerRes      = false;
    bool dynTick       = false;
    bool hpet          = false;
    bool ultPerf       = false;
    bool nagle         = false;
    bool tcpAutoTune   = false;
    bool qos           = false;
    bool hwGpuSched    = false;
    bool tdr           = false;
    bool nvPower       = false;
    bool irqPrio       = false;
    bool msiMode       = false;
    bool usbPoll       = false;
    bool dpcWatchdog   = false;
    bool mmcss         = false;
    bool procPrio      = false;
    bool coreIsolate   = false;
    bool schedulerTwk  = false;
    bool aimTweaks     = false;
    bool kernelAim     = false;
};

static TweakState g_state;
static std::vector<std::string> g_log;
static AIM_CONFIG g_aimCfg = {1.0f, 1.0f, 1.0f, 0.0f, 50.0f, 0};
static float g_dpi    = 800.0f;
static float g_sens   = 1.0f;
static float g_edpi   = 800.0f;
static float g_cm360  = 45.0f;

// D3D11 globals
static ID3D11Device*           g_pd3dDevice           = nullptr;
static ID3D11DeviceContext*    g_pd3dDeviceContext     = nullptr;
static IDXGISwapChain*         g_pSwapChain            = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView  = nullptr;

static void AddLog(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    g_log.push_back(std::string(buf));
    if (g_log.size() > 200) g_log.erase(g_log.begin());
}

// ─── D3D helpers ─────────────────────────────────────────────────────────────

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        0, featureLevelArray, 2,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain,
        &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);

    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            0, featureLevelArray, 2,
            D3D11_SDK_VERSION, &sd, &g_pSwapChain,
            &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);

    if (res != S_OK) return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain)        { g_pSwapChain->Release();        g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice)        { g_pd3dDevice->Release();        g_pd3dDevice = nullptr; }
}

// ─── WndProc ─────────────────────────────────────────────────────────────────

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
        case WM_SIZE:
            if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_KEYMENU) return 0;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ─── Apply helpers ───────────────────────────────────────────────────────────

static void ApplyAll() {
    if (g_state.timerRes)     { Timer::SetMaxResolution();             AddLog("[+] Timer resolution set to 0.5ms"); }
    if (g_state.schedulerTwk) { Registry::ApplySchedulerTweaks();     AddLog("[+] Scheduler tweaks applied"); }
    if (g_state.dynTick)      { Registry::ApplyBcdeditTweaks();       AddLog("[+] Dynamic tick / HPET tweaks queued (requires reboot)"); }
    if (g_state.dpcWatchdog)  { Registry::DisableDPCWatchdog();       AddLog("[+] DPC watchdog disabled"); }
    if (g_state.ultPerf)      { Power::CreateUltimatePerformanceScheme(); AddLog("[+] Ultimate Performance power scheme activated"); }
    if (g_state.nagle)        { Network::DisableNagle();              AddLog("[+] Nagle disabled on all interfaces"); }
    if (g_state.tcpAutoTune)  { Network::DisableAutoTuning();         AddLog("[+] TCP auto-tuning disabled"); }
    if (g_state.qos)          { Network::DisableQoSThrottle();        AddLog("[+] QoS throttle removed"); }
    if (g_state.hwGpuSched)   { GPU::EnableHAGS();                    AddLog("[+] Hardware-accelerated GPU scheduling on"); }
    if (g_state.tdr)          { GPU::DisableTDR();                    AddLog("[+] TDR disabled"); }
    if (g_state.nvPower)      { GPU::SetNVMaxPerformance();           AddLog("[+] NVIDIA max performance mode set"); }
    if (g_state.irqPrio)      { IRQ::SetIRQPriority();               AddLog("[+] IRQ 8/9 priority boosted"); }
    if (g_state.msiMode)      { IRQ::EnableMSIMode();                 AddLog("[+] MSI mode enabled on PCI devices"); }
    if (g_state.usbPoll)      { IRQ::PatchUSBPollingRate();           AddLog("[+] USB polling rate set to 1ms"); }
    if (g_state.mmcss)        { MMCSS::ApplyGamingProfile();         AddLog("[+] MMCSS gaming profile applied"); }
    if (g_state.procPrio)     { Process::SetRealtimePriority();       AddLog("[+] Process priority set to real-time"); }
    if (g_state.coreIsolate)  { Process::IsolateGameCores();          AddLog("[+] Game cores isolated"); }
    if (g_state.aimTweaks) {
        Aim::DisablePointerPrecision();
        Aim::ApplyRawInput();
        Aim::ZeroMouseCurve();
        AddLog("[+] OS aim tweaks applied — raw input, no acceleration");
    }
    if (g_state.kernelAim) {
        if (AimClient::Connect()) {
            AimClient::SetConfig(g_aimCfg);
            AddLog("[+] Kernel aim config pushed to ring-0 driver");
        } else {
            AddLog("[!] Kernel aim driver not found — load aim_drv.sys first");
        }
    }
    AddLog("[✓] Apply complete — some tweaks require reboot");
}

// ─── GUI render ──────────────────────────────────────────────────────────────

static void RenderUI() {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize   |
                 ImGuiWindowFlags_NoMove     |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Header
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.85f, 1.0f, 1.0f));
    ImGui::SetWindowFontScale(1.4f);
    ImGui::Text("KernelOpt  |  Zero-Delay Kernel Utility");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::Separator();

    // Apply All button
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f, 0.55f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.7f,  1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.0f, 0.4f,  0.75f, 1.0f));
    if (ImGui::Button("APPLY ALL ENABLED", {200, 36})) ApplyAll();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::TextDisabled("(tick what you want then click)");
    ImGui::Spacing();

    if (ImGui::BeginTabBar("MainTabs")) {

        // ── Timer & Scheduler ──────────────────────────────────────────────
        if (ImGui::BeginTabItem("Timer / CPU")) {
            ImGui::Spacing();
            ImGui::Checkbox("0.5ms Timer Resolution (NtSetTimerResolution)", &g_state.timerRes);
            ImGui::SameLine(); ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Calls NtSetTimerResolution(5000) for minimum OS timer interval.");

            ImGui::Checkbox("Disable Dynamic Tick + Disable HPET (bcdedit)", &g_state.dynTick);
            ImGui::SameLine(); ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Requires reboot. Disables power-saving CPU tick suppression.");

            ImGui::Checkbox("Win32PrioritySeparation = 0x26 (max foreground boost)", &g_state.schedulerTwk);
            ImGui::Checkbox("Disable DPC Watchdog", &g_state.dpcWatchdog);
            ImGui::Checkbox("Real-time process priority + SeLockMemory", &g_state.procPrio);
            ImGui::Checkbox("Isolate game cores (SetProcessDefaultCpuSets)", &g_state.coreIsolate);
            ImGui::EndTabItem();
        }

        // ── Power ─────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Power")) {
            ImGui::Spacing();
            ImGui::Checkbox("Activate Ultimate Performance power scheme", &g_state.ultPerf);
            ImGui::SameLine(); ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Duplicates the hidden e9a42b02 GUID and sets it active.\nAlso disables USB selective suspend.");
            ImGui::EndTabItem();
        }

        // ── Network ───────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Network")) {
            ImGui::Spacing();
            ImGui::Checkbox("Disable Nagle (TcpAckFrequency + TCPNoDelay)", &g_state.nagle);
            ImGui::Checkbox("Disable TCP Auto-Tuning (netsh)", &g_state.tcpAutoTune);
            ImGui::Checkbox("Remove QoS throttle (NonBestEffortLimit=0)", &g_state.qos);
            ImGui::Checkbox("Disable NIC interrupt moderation", &g_state.hwGpuSched);
            ImGui::SameLine(); ImGui::TextDisabled("Note: also toggles HAGS below");
            ImGui::EndTabItem();
        }

        // ── GPU ───────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("GPU")) {
            ImGui::Spacing();
            ImGui::Checkbox("Hardware-Accelerated GPU Scheduling (HAGS)", &g_state.hwGpuSched);
            ImGui::SameLine(); ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Requires WDDM 2.7+ and reboot.");

            ImGui::Checkbox("Disable TDR (TdrLevel=0)", &g_state.tdr);
            ImGui::Checkbox("NVIDIA Max Performance mode (PowerMizerLevel=1)", &g_state.nvPower);
            ImGui::EndTabItem();
        }

        // ── IRQ / MSI ─────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("IRQ / USB")) {
            ImGui::Spacing();
            ImGui::Checkbox("Boost IRQ 8 / 9 priority", &g_state.irqPrio);
            ImGui::Checkbox("Enable MSI mode on all PCI devices", &g_state.msiMode);
            ImGui::SameLine(); ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Message Signaled Interrupts: lower latency than legacy INTx.");

            ImGui::Checkbox("USB HID polling rate → 1ms (125 → 1000 Hz)", &g_state.usbPoll);
            ImGui::EndTabItem();
        }

        // ── MMCSS ─────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("MMCSS")) {
            ImGui::Spacing();
            ImGui::Checkbox("Apply MMCSS Gaming profile (SystemResponsiveness=0)", &g_state.mmcss);
            ImGui::SameLine(); ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sets GPU Priority=8, CPU Priority=6, NetworkThrottlingIndex=0xFFFFFFFF.\nAlso registers this process thread under MMCSS 'Games'.");
            ImGui::EndTabItem();
        }

        // ── Aim ───────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Aim")) {
            ImGui::Spacing();

            // OS tweaks
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.9f, 0.5f, 1.0f));
            ImGui::Text("OS-Level Aim Tweaks");
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Checkbox("Disable pointer precision (mouse accel off)", &g_state.aimTweaks);
            ImGui::SameLine(); ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Sets SmoothMouseX/YCurve to zero, disables enhance pointer precision,\nsets pointer speed to 6/11 (1:1 mapping).");

            ImGui::Spacing();

            // eDPI calculator
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.9f, 0.5f, 1.0f));
            ImGui::Text("eDPI Calculator");
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::InputFloat("Mouse DPI", &g_dpi,  100.0f, 400.0f, "%.0f");
            ImGui::InputFloat("In-game Sensitivity", &g_sens, 0.1f, 0.5f, "%.3f");
            if (g_dpi < 1.0f) g_dpi = 1.0f;
            if (g_sens < 0.001f) g_sens = 0.001f;
            g_edpi  = g_dpi * g_sens;
            g_cm360 = 36000.0f / g_edpi;
            ImGui::Text("eDPI: %.1f   |   cm/360: %.2f", g_edpi, g_cm360);

            ImGui::Spacing();

            // Kernel aim driver
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.1f, 1.0f));
            ImGui::Text("Ring-0 Kernel Aim Driver (aim_drv.sys)");
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::TextWrapped("Load aim_drv.sys manually (sc create / OSR Driver Loader) then connect:");

            if (AimClient::IsConnected()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.4f, 1.0f));
                ImGui::Text("Driver: CONNECTED");
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                ImGui::Text("Driver: NOT LOADED");
                ImGui::PopStyleColor();
                if (ImGui::Button("Connect to driver")) {
                    if (AimClient::Connect())
                        AddLog("[+] Connected to KernelAim driver");
                    else
                        AddLog("[!] Failed to open \\\\.\\KernelAim — driver not loaded");
                }
            }

            ImGui::Spacing();
            ImGui::Checkbox("Enable kernel aim (push config to ring-0)", &g_state.kernelAim);
            if (g_state.kernelAim) {
                ImGui::Indent();
                ImGui::SliderFloat("Sensitivity scale", &g_aimCfg.sensitivity, 0.1f, 3.0f, "%.2f");
                ImGui::SliderFloat("X scale",           &g_aimCfg.xScale,      0.1f, 3.0f, "%.2f");
                ImGui::SliderFloat("Y scale",           &g_aimCfg.yScale,      0.1f, 3.0f, "%.2f");
                ImGui::SliderFloat("Accel factor (0=off)", &g_aimCfg.accelFactor, 0.0f, 5.0f, "%.2f");
                ImGui::SliderFloat("Accel cap",         &g_aimCfg.accelCap,    1.0f, 200.0f, "%.1f");
                bool flipY = (g_aimCfg.flags & 1) != 0;
                if (ImGui::Checkbox("Flip Y axis", &flipY))
                    g_aimCfg.flags = flipY ? (g_aimCfg.flags | 1) : (g_aimCfg.flags & ~1u);
                if (ImGui::Button("Push config now")) {
                    if (!AimClient::IsConnected()) AimClient::Connect();
                    if (AimClient::SetConfig(g_aimCfg))
                        AddLog("[+] Aim config pushed");
                    else
                        AddLog("[!] Push failed — driver not connected");
                }
                ImGui::Unindent();
            }

            ImGui::EndTabItem();
        }

        // ── Log ───────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Log")) {
            ImGui::Spacing();
            if (ImGui::Button("Clear")) g_log.clear();
            ImGui::Separator();
            ImGui::BeginChild("LogScroll", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);
            for (auto& line : g_log) {
                if (line.size() > 2 && line[1] == '+')
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.4f, 1.0f));
                else if (line.size() > 2 && line[1] == '!')
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.1f, 1.0f));
                else
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
                ImGui::TextUnformatted(line.c_str());
                ImGui::PopStyleColor();
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ─── Dark theme ──────────────────────────────────────────────────────────────

static void ApplyDarkTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 4.0f;
    s.FrameRounding     = 3.0f;
    s.ScrollbarRounding = 3.0f;
    s.GrabRounding      = 3.0f;
    s.WindowBorderSize  = 0.0f;
    s.FrameBorderSize   = 1.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]         = {0.08f, 0.08f, 0.09f, 1.0f};
    c[ImGuiCol_ChildBg]          = {0.08f, 0.08f, 0.09f, 1.0f};
    c[ImGuiCol_PopupBg]          = {0.08f, 0.08f, 0.09f, 1.0f};
    c[ImGuiCol_Border]           = {0.18f, 0.18f, 0.22f, 1.0f};
    c[ImGuiCol_FrameBg]          = {0.14f, 0.14f, 0.17f, 1.0f};
    c[ImGuiCol_FrameBgHovered]   = {0.20f, 0.20f, 0.24f, 1.0f};
    c[ImGuiCol_FrameBgActive]    = {0.24f, 0.24f, 0.28f, 1.0f};
    c[ImGuiCol_TitleBg]          = {0.08f, 0.08f, 0.09f, 1.0f};
    c[ImGuiCol_TitleBgActive]    = {0.08f, 0.08f, 0.09f, 1.0f};
    c[ImGuiCol_MenuBarBg]        = {0.10f, 0.10f, 0.12f, 1.0f};
    c[ImGuiCol_ScrollbarBg]      = {0.08f, 0.08f, 0.09f, 1.0f};
    c[ImGuiCol_ScrollbarGrab]    = {0.24f, 0.24f, 0.28f, 1.0f};
    c[ImGuiCol_CheckMark]        = {0.0f,  0.85f, 1.0f,  1.0f};
    c[ImGuiCol_SliderGrab]       = {0.0f,  0.65f, 0.9f,  1.0f};
    c[ImGuiCol_SliderGrabActive] = {0.0f,  0.85f, 1.0f,  1.0f};
    c[ImGuiCol_Button]           = {0.14f, 0.14f, 0.17f, 1.0f};
    c[ImGuiCol_ButtonHovered]    = {0.20f, 0.44f, 0.70f, 1.0f};
    c[ImGuiCol_ButtonActive]     = {0.0f,  0.55f, 0.85f, 1.0f};
    c[ImGuiCol_Header]           = {0.14f, 0.35f, 0.60f, 1.0f};
    c[ImGuiCol_HeaderHovered]    = {0.20f, 0.45f, 0.72f, 1.0f};
    c[ImGuiCol_HeaderActive]     = {0.14f, 0.42f, 0.68f, 1.0f};
    c[ImGuiCol_Tab]              = {0.11f, 0.11f, 0.13f, 1.0f};
    c[ImGuiCol_TabHovered]       = {0.0f,  0.6f,  0.9f,  1.0f};
    c[ImGuiCol_TabActive]        = {0.0f,  0.5f,  0.8f,  1.0f};
    c[ImGuiCol_TabUnfocused]     = {0.11f, 0.11f, 0.13f, 1.0f};
    c[ImGuiCol_TabUnfocusedActive]= {0.0f, 0.4f,  0.7f,  1.0f};
    c[ImGuiCol_Text]             = {0.90f, 0.90f, 0.92f, 1.0f};
    c[ImGuiCol_TextDisabled]     = {0.45f, 0.45f, 0.50f, 1.0f};
    c[ImGuiCol_Separator]        = {0.18f, 0.18f, 0.22f, 1.0f};
    c[ImGuiCol_ResizeGrip]       = {0.0f,  0.55f, 0.85f, 0.4f};
    c[ImGuiCol_ResizeGripHovered]= {0.0f,  0.65f, 0.95f, 0.7f};
    c[ImGuiCol_ResizeGripActive] = {0.0f,  0.75f, 1.0f,  1.0f};
}

// ─── WinMain ─────────────────────────────────────────────────────────────────

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Require admin
    {
        HANDLE token = nullptr;
        OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
        TOKEN_ELEVATION elev{};
        DWORD sz = sizeof(elev);
        GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &sz);
        CloseHandle(token);
        if (!elev.TokenIsElevated) {
            MessageBoxW(nullptr,
                L"KernelOpt requires administrator privileges.\n"
                L"Right-click and choose 'Run as administrator'.",
                L"Elevation Required", MB_ICONERROR);
            return 1;
        }
    }

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"KernelOpt";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowW(
        wc.lpszClassName, L"KernelOpt",
        WS_OVERLAPPEDWINDOW, 100, 100, 1050, 680,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ApplyDarkTheme();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    AddLog("[*] KernelOpt started — select tweaks and click APPLY ALL");

    // Try connecting to optional kernel drivers on startup
    if (AimClient::Connect())
        AddLog("[+] KernelAim driver detected");
    else
        AddLog("[~] KernelAim driver not loaded (optional)");

    ImVec4 clearColor = {0.06f, 0.06f, 0.07f, 1.0f};

    MSG msg{};
    while (true) {
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) goto done;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RenderUI();

        ImGui::Render();
        const float cc[4] = {clearColor.x, clearColor.y, clearColor.z, clearColor.w};
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, cc);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

done:
    AimClient::Disconnect();
    Timer::Restore();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
