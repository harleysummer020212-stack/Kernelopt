#pragma once
#include <windows.h>
namespace Process {
    void SetRealtimePriority();
    void IsolateGameCores(HANDLE gameProc);
}
