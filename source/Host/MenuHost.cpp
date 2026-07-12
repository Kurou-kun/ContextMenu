#include "Host/MenuHost.h"
#include "Menu/MenuFile.h"
#include "Render/PopupWindow.h"
#include "RainmeterAPI.h"

#include <commctrl.h>
#include <windowsx.h>
#include <unordered_map>

#pragma comment(lib, "comctl32.lib")

static std::unordered_map<HWND, MenuHost*> g_hosts;
static const UINT_PTR kSubclassId = 1;

MenuHost* MenuHost::Attach(HWND hwnd, void* skin, void* rm, const std::wstring& menuPath) {
    auto it = g_hosts.find(hwnd);
    if (it != g_hosts.end()) {
        it->second->SetActive(skin, rm, menuPath);
        ++it->second->refs_;
        return it->second;
    }
    RmLog(rm, LOG_NOTICE, L"[CM] Attach: begin (new host)");
    auto* host = new MenuHost();
    host->hwnd_ = hwnd;
    host->SetActive(skin, rm, menuPath);
    host->refs_ = 1;
    g_hosts[hwnd] = host;
    SetWindowSubclass(hwnd, &MenuHost::SubProc, kSubclassId, reinterpret_cast<DWORD_PTR>(host));
    RmLog(rm, LOG_NOTICE, L"[CM] Attach: subclassed");
    return host;
}

void MenuHost::SetActive(void* skin, void* rm, const std::wstring& menuPath) {
    skin_ = skin;
    rm_ = rm;
    menuPath_ = menuPath;
}

void MenuHost::Detach(HWND hwnd) {
    auto it = g_hosts.find(hwnd);
    if (it == g_hosts.end()) return;
    MenuHost* host = it->second;
    RmLog(host->rm_, LOG_NOTICE, L"[CM] Detach: begin");
    if (--host->refs_ > 0) { RmLog(host->rm_, LOG_NOTICE, L"[CM] Detach: still refd"); return; }
    RemoveWindowSubclass(hwnd, &MenuHost::SubProc, kSubclassId);
    void* rm = host->rm_;
    RmLog(rm, LOG_NOTICE, L"[CM] Detach: subclass removed");
    g_hosts.erase(it);
    RmLog(rm, LOG_NOTICE, L"[CM] Detach: erased map");
    delete host;
    RmLog(rm, LOG_NOTICE, L"[CM] Detach: deleted host");
}

LRESULT CALLBACK MenuHost::SubProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                   UINT_PTR, DWORD_PTR ref) {
    auto* host = reinterpret_cast<MenuHost*>(ref);
    switch (msg) {
    // Rainmeter's borderless skin treats the whole window as non-client, so
    // right-clicks arrive as NC messages. Rainmeter shows its default menu from
    // WM_NCRBUTTONUP — swallow the pair and show ours instead. lParam is already
    // in screen coordinates for NC messages.
    case WM_NCRBUTTONDOWN:
        return 0; // swallow so Rainmeter starts no right-drag handling
    case WM_NCRBUTTONUP: {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        host->ShowMenu(pt);
        return 0;
    }
    // Client-area paths (skins with a real client region, keyboard-invoked menu).
    case WM_CONTEXTMENU: {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (pt.x == -1 && pt.y == -1) GetCursorPos(&pt);
        host->ShowMenu(pt);
        return 0;
    }
    case WM_RBUTTONUP: {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ClientToScreen(hwnd, &pt);
        host->ShowMenu(pt);
        return 0;
    }
    default:
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }
}

void MenuHost::ShowMenu(POINT screenPt) {
    std::wstring text;
    if (!cm::ReadMenuFile(menuPath_, text)) return;
    cm::MenuModel model = cm::ParseMenu(text);
    if (model.items.empty()) return;
    PopupWindow popup(hwnd_, skin_, rm_);
    std::wstring bang = popup.Show(model, screenPt);
    if (bang.empty()) return;

    // RmExecute is async (posts the bang), so calling it straight from the
    // right-click handler is safe: the WM_NCRBUTTONUP frame unwinds before the
    // refresh runs, and the DLL is pinned (Plugin.cpp) so an unload/reload
    // during refresh can't strand our subclass proc.
    LPCWSTR resolved = RmReplaceVariables(rm_, bang.c_str());
    RmLog(rm_, LOG_NOTICE, (L"[CM] ShowMenu: execute " + std::wstring(resolved)).c_str());
    RmExecute(skin_, resolved);
}
