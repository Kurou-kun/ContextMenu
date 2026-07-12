#include <windows.h>

#include "Plugin/Plugin.h"
#include "Host/MenuHost.h"

#include "RainmeterAPI.h"

// ContextMenu — customizable right-click menu for Rainmeter skins.
//
// On load the plugin subclasses the skin window and takes over its right-click,
// showing a custom menu built from the Menu= file. See MenuHost / PopupWindow.

static std::wstring ReadMenuPath(void* rm)
{
    LPCWSTR p = RmReadPath(rm, L"Menu", L"");
    return p ? std::wstring(p) : std::wstring();
}

// Rainmeter FreeLibrary()s the plugin on skin refresh, but our window subclass,
// popup window class, and GDI+ leave code/callbacks that must outlive that. Pin
// the module so it stays mapped for the process lifetime — a stale wndproc in an
// unloaded image is a guaranteed crash-on-refresh otherwise.
static void PinSelf()
{
    static bool pinned = false;
    if (pinned) return;
    pinned = true;
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                       reinterpret_cast<LPCWSTR>(&PinSelf), &self);
}

PLUGIN_EXPORT void Initialize(void** data, void* rm)
{
    PinSelf();
    auto* ctx = new MeasureContext();
    ctx->rm   = rm;
    ctx->skin = RmGetSkin(rm);
    ctx->hwnd = static_cast<HWND>(RmGet(rm, RMG_SKINWINDOWHANDLE));
    ctx->menuPath = ReadMenuPath(rm);

    if (ctx->hwnd && !ctx->menuPath.empty())
        ctx->host = MenuHost::Attach(ctx->hwnd, ctx->skin, rm, ctx->menuPath);

    *data = ctx;
}

PLUGIN_EXPORT void Reload(void* data, void* rm, double* /*maxValue*/)
{
    auto* ctx = static_cast<MeasureContext*>(data);
    if (!ctx)
        return;

    ctx->rm   = rm;
    ctx->skin = RmGetSkin(rm);
    HWND hwnd = static_cast<HWND>(RmGet(rm, RMG_SKINWINDOWHANDLE));
    std::wstring path = ReadMenuPath(rm);

    // Window changed (rare) — release the old claim before attaching the new one.
    if (ctx->host && hwnd != ctx->hwnd)
    {
        MenuHost::Detach(ctx->hwnd);
        ctx->host = nullptr;
    }
    ctx->hwnd = hwnd;
    ctx->menuPath = path;

    if (ctx->hwnd && !ctx->menuPath.empty())
    {
        if (ctx->host)
            ctx->host->SetActive(ctx->skin, rm, ctx->menuPath);
        else
            ctx->host = MenuHost::Attach(ctx->hwnd, ctx->skin, rm, ctx->menuPath);
    }
}

PLUGIN_EXPORT double Update(void* data)
{
    auto* ctx = static_cast<MeasureContext*>(data);
    return ctx ? 0.0 : 0.0;
}

PLUGIN_EXPORT void Finalize(void* data)
{
    auto* ctx = static_cast<MeasureContext*>(data);
    if (!ctx)
        return;
    if (ctx->host && ctx->hwnd)
        MenuHost::Detach(ctx->hwnd);
    delete ctx;
}
