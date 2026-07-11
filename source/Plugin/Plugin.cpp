#include <windows.h>

#include "Plugin/Plugin.h"

#include "RainmeterAPI.h"

// ContextMenu — customizable right-click menu for Rainmeter skins.
//
// Skeleton only. The measure reads its options here; the menu build + display
// (TrackPopupMenu on ctx->hwnd, RmExecute on ctx->skin for the chosen item)
// lands in the next rite. Entry points below are the full Rainmeter plugin
// contract so the DLL loads and reloads cleanly today.

static void ReadOptions(MeasureContext* ctx, void* rm)
{
    // RmReadString returns a pointer into Rainmeter's buffer — copy immediately.
    ctx->menuStr = RmReadString(rm, L"Menu", L"", FALSE);
}

PLUGIN_EXPORT void Initialize(void** data, void* rm)
{
    auto* ctx = new MeasureContext();
    ctx->skin = RmGetSkin(rm);
    ctx->hwnd = static_cast<HWND>(RmGet(rm, RMG_SKINWINDOWHANDLE));
    ReadOptions(ctx, rm);
    *data = ctx;
}

PLUGIN_EXPORT void Reload(void* data, void* rm, double* /*maxValue*/)
{
    auto* ctx = static_cast<MeasureContext*>(data);
    if (!ctx)
        return;
    ReadOptions(ctx, rm);
}

PLUGIN_EXPORT double Update(void* data)
{
    auto* ctx = static_cast<MeasureContext*>(data);
    if (!ctx)
        return 0.0;
    return 0.0;
}

PLUGIN_EXPORT void Finalize(void* data)
{
    delete static_cast<MeasureContext*>(data);
}
