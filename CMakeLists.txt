cmake_minimum_required(VERSION 3.20)
project(KernelOpt CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Sources ──────────────────────────────────────────────────────────────────

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/src/imgui)

set(SOURCES
    src/main_gui.cpp
    src/timer.cpp
    src/registry.cpp
    src/power.cpp
    src/network.cpp
    src/gpu.cpp
    src/irq.cpp
    src/mmcss.cpp
    src/process.cpp
    src/aim.cpp
    src/aim_client.cpp

    # ImGui
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_impl_win32.cpp
    ${IMGUI_DIR}/imgui_impl_dx11.cpp
)

add_executable(KernelOpt WIN32 ${SOURCES})

target_include_directories(KernelOpt PRIVATE
    src
    ${IMGUI_DIR}
)

target_compile_options(KernelOpt PRIVATE
    /W3
    /MP
    /EHsc
    /D_CRT_SECURE_NO_WARNINGS
    /DUNICODE
    /D_UNICODE
)

# Require admin in manifest
set_target_properties(KernelOpt PROPERTIES
    LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\""
)

target_link_libraries(KernelOpt PRIVATE
    d3d11
    dxgi
    winmm
    avrt
    setupapi
    cfgmgr32
    ntdll
)
