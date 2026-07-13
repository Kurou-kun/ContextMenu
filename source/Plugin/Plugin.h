#pragma once

#include <windows.h>
#include <string>

class MenuHost;

// One instance per Rainmeter measure.
struct MeasureContext
{
    void*        rm   = nullptr;  // Rainmeter measure handle (stored for variable resolution)
    void*        skin = nullptr;  // RmGetSkin — pass to RmExecute
    HWND         hwnd = nullptr;  // RMG_SKINWINDOWHANDLE — subclass target / popup anchor
    std::wstring menuPath;        // resolved absolute path to the Menu= file
    MenuHost*    host = nullptr;  // per-window subclass owner (shared across measures)
};
