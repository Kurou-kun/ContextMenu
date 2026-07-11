#pragma once

#include <windows.h>
#include <string>

// One instance per Rainmeter measure. The skin pointer + window are the two
// handles any context-menu implementation needs: skin to fire !bangs for the
// chosen item, hwnd to anchor the popup menu on screen.
struct MeasureContext
{
    void* skin = nullptr;  // RmGetSkin — pass to RmExecute
    HWND  hwnd = nullptr;  // RMG_SKINWINDOWHANDLE — TrackPopupMenu anchor

    std::wstring menuStr;  // raw "Menu" option, parsed when the menu is built
};
