╔══════════════════════════════════════════════════════════════════════════════╗
║                     KernelOpt — Zero-Delay Kernel Utility                  ║
╚══════════════════════════════════════════════════════════════════════════════╝

WHAT'S IN THIS ZIP
──────────────────
  src/                 — Userland C++ source (ImGui GUI + all tweak modules)
  kernelopt_drv/       — Optional: sub-0.5ms kernel timer driver (.sys)
  KernelAim/           — Optional: ring-0 mouse filter driver (.sys)
  CMakeLists.txt       — CMake build for KernelOpt.exe
  README.txt           — This file

══════════════════════════════════════════════════════════════════════════════
STEP 1 — BUILD KernelOpt.exe  (the main GUI)
══════════════════════════════════════════════════════════════════════════════

Requirements:
  • Visual Studio 2022 (C++ Desktop workload)
  • CMake 3.20+
  • ImGui source: download from https://github.com/ocornut/imgui
    Copy these files into src/imgui/:
      imgui.h  imgui.cpp  imgui_internal.h  imconfig.h
      imgui_draw.cpp  imgui_tables.cpp  imgui_widgets.cpp
      backends/imgui_impl_win32.h   backends/imgui_impl_win32.cpp
      backends/imgui_impl_dx11.h    backends/imgui_impl_dx11.cpp

Build commands (Developer Command Prompt for VS 2022):
  cd path\to\KernelOpt
  cmake -B build -G "Visual Studio 17 2022" -A x64
  cmake --build build --config Release

Output: build\Release\KernelOpt.exe

Right-click KernelOpt.exe → "Run as administrator" to launch.

══════════════════════════════════════════════════════════════════════════════
STEP 2 — OPTIONAL: Enable test-signing for kernel drivers
══════════════════════════════════════════════════════════════════════════════

The two .sys drivers (kernelopt_drv.sys, aim_drv.sys) are unsigned.
Windows requires test-signing mode to load them.

  Run as admin in CMD:
    bcdedit /set testsigning on
    Restart Windows.

  You'll see a "Test Mode" watermark bottom-right of desktop. That's normal.
  To disable later: bcdedit /set testsigning off + reboot.

══════════════════════════════════════════════════════════════════════════════
STEP 3 — OPTIONAL: Build & load kernelopt_drv.sys  (sub-0.5ms timer)
══════════════════════════════════════════════════════════════════════════════

Requirements: Windows Driver Kit (WDK) + VS2022

1. Open VS2022 → New Project → "Kernel Mode Driver, Empty (KMDF)"
   Name: kernelopt_drv
2. Add kernelopt_drv/kernelopt_drv.cpp to the project.
3. Build (Release / x64) → kernelopt_drv.sys

Load (run CMD as admin):
  sc create kernelopt_drv type= kernel binPath= "C:\full\path\kernelopt_drv.sys"
  sc start kernelopt_drv

Verify in DebugView (Sysinternals) — you'll see:
  "KernelOpt: timer resolution set to 1 ..."

Unload when done:
  sc stop kernelopt_drv
  sc delete kernelopt_drv

══════════════════════════════════════════════════════════════════════════════
STEP 4 — OPTIONAL: Build & load aim_drv.sys  (ring-0 mouse filter)
══════════════════════════════════════════════════════════════════════════════

1. Open VS2022 → New Project → "Kernel Mode Driver, Empty (KMDF)"
   Name: KernelAim
2. Add KernelAim/aim_drv.cpp to the project.
3. Under Project Properties:
     Linker → Input → Additional Dependencies: add  kbdmou.lib
4. Build (Release / x64) → KernelAim.sys

Load (CMD as admin):
  sc create KernelAim type= kernel binPath= "C:\full\path\KernelAim.sys"
  sc start KernelAim

5. Launch KernelOpt.exe → Aim tab → "Connect to driver" → configure & push.

Unload:
  sc stop KernelAim
  sc delete KernelAim

══════════════════════════════════════════════════════════════════════════════
QUICK-LOAD SCRIPT  (after all .sys files are built)
══════════════════════════════════════════════════════════════════════════════

Save as load_drivers.bat, edit paths, run as admin:

  @echo off
  sc create kernelopt_drv type= kernel binPath= "C:\KernelOpt\kernelopt_drv.sys"
  sc start kernelopt_drv
  sc create KernelAim type= kernel binPath= "C:\KernelOpt\KernelAim.sys"
  sc start KernelAim
  echo Drivers loaded. Launch KernelOpt.exe as admin.
  pause

══════════════════════════════════════════════════════════════════════════════
WHAT EACH TWEAK DOES
══════════════════════════════════════════════════════════════════════════════

Timer / CPU tab:
  0.5ms Timer Resolution     NtSetTimerResolution(5000) — min OS timer interval
  Disable Dynamic Tick       bcdedit /set disabledynamictick yes  (needs reboot)
  Disable HPET               bcdedit /set useplatformclock false  (needs reboot)
  Win32PrioritySeparation    0x26 = short variable quanta, max foreground boost
  Disable DPC Watchdog       prevents BSOD on heavy GPU/CPU loads
  Real-time priority         REALTIME_PRIORITY_CLASS + SeLockMemoryPrivilege
  Core isolation             SetProcessDefaultCpuSets — keep game on fast cores

Power tab:
  Ultimate Performance       Duplicates hidden power GUID, disables USB suspend

Network tab:
  Disable Nagle              TcpAckFrequency=1, TCPNoDelay=1 (all interfaces)
  Disable TCP auto-tuning    netsh int tcp set global autotuninglevel=disabled
  Remove QoS throttle        NonBestEffortLimit=0

GPU tab:
  HAGS                       HwSchMode=2 — GPU schedules its own work (WDDM 2.7+)
  Disable TDR                TdrLevel=0 — no driver reset on hang
  NVIDIA max perf            PowerMizerLevel=1, PowerMizerEnable=1

IRQ / USB tab:
  IRQ 8/9 priority           Prioritizes system timer / ACPI interrupt lines
  MSI mode                   MSISupported=1 on all PCI devices (lower latency)
  USB 1ms polling            PollingInterval=1 on HID devices (1000 Hz effective)

MMCSS tab:
  Gaming profile             SystemResponsiveness=0, NetworkThrottlingIndex=max
                             GPU Priority=8, CPU Priority=6 for Games task

Aim tab:
  OS tweaks                  SmoothMouseX/YCurve=0, pointer speed 6/11, no accel
  Kernel aim driver          Ring-0 mouclass filter, sub-millisecond delta scale

══════════════════════════════════════════════════════════════════════════════
NOTES
══════════════════════════════════════════════════════════════════════════════

• Dynamic tick + HPET changes require a reboot to take effect.
• USB polling rate change requires replug of your mouse.
• MSI mode + HAGS also require a reboot.
• Kernel drivers require test-signing mode and must be rebuilt for your Windows
  version; see WDK docs for KMDF target version.
• Real-time process priority can cause the UI to stutter — only enable when
  actually gaming, then revert by closing KernelOpt.

══════════════════════════════════════════════════════════════════════════════
