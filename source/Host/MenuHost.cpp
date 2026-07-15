#include "Host/MenuHost.h"
#include "Menu/MenuFile.h"
#include "Render/PopupWindow.h"
#include "RainmeterAPI.h"

#include <commctrl.h>
#include <windowsx.h>
#include <unordered_map>
#include <algorithm>
#include <cwctype>
#include <map>
#include <set>
#include <string>

#pragma comment(lib, "comctl32.lib")

static std::unordered_map<HWND, MenuHost*> g_hosts;
static const UINT_PTR kSubclassId = 1;

// ---- Menu source preprocessor: @Include expansion + #variable# substitution ----
namespace {

std::wstring Trim(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a])) ++a;
    while (b > a && iswspace(s[b - 1])) --b;
    return s.substr(a, b - a);
}
std::wstring Lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    return s;
}

// Inline-expand @Include=<path> recursively into `out` (include order preserved).
// Include paths run through Rainmeter var/path resolution so #@#, #CURRENTPATH#
// etc. work. `visited` (lowercased absolute paths) guards against include cycles.
void ExpandInto(const std::wstring& path, void* rm,
                std::set<std::wstring>& visited, std::wstring& out) {
    std::wstring key = Lower(path);
    if (visited.count(key)) return;
    visited.insert(key);

    std::wstring text;
    if (!cm::ReadMenuFile(path, text)) return;

    size_t i = 0;
    while (i < text.size()) {
        size_t nl = text.find(L'\n', i);
        std::wstring line = text.substr(i, nl == std::wstring::npos ? std::wstring::npos : nl - i);
        i = (nl == std::wstring::npos) ? text.size() : nl + 1;

        std::wstring t = Trim(line);
        if (t.size() > 8 && t[0] == L'@' && Lower(t.substr(0, 8)) == L"@include") {
            size_t eq = t.find(L'=');
            if (eq != std::wstring::npos) {
                std::wstring val = Trim(t.substr(eq + 1));
                LPCWSTR v = RmReplaceVariables(rm, val.c_str());
                LPCWSTR abs = RmPathToAbsolute(rm, v ? v : val.c_str());
                ExpandInto(abs ? abs : (v ? v : val), rm, visited, out);
                continue;   // the directive line itself is not emitted
            }
        }
        out += line;
        out += L'\n';
    }
}

// Replace every #name# token from `vars` (name matched case-insensitively).
// Unknown #...# tokens (skin-level Rainmeter variables) are left untouched.
std::wstring SubVars(const std::wstring& in, const std::map<std::wstring, std::wstring>& vars) {
    std::wstring out; out.reserve(in.size());
    size_t i = 0;
    while (i < in.size()) {
        if (in[i] == L'#') {
            size_t e = in.find(L'#', i + 1);
            if (e != std::wstring::npos) {
                auto it = vars.find(Lower(in.substr(i + 1, e - i - 1)));
                if (it != vars.end()) { out += it->second; i = e + 1; continue; }
            }
        }
        out += in[i++];
    }
    return out;
}

// Read the root menu file, expand @Includes, collect every [Variables] section,
// then substitute #var# tokens throughout. Returns fully-resolved menu text.
std::wstring ExpandSource(const std::wstring& rootPath, void* rm) {
    std::set<std::wstring> visited;
    std::wstring merged;
    ExpandInto(rootPath, rm, visited, merged);

    // Collect [Variables] key=val across all included content (later wins).
    std::map<std::wstring, std::wstring> vars;
    bool inVars = false;
    size_t i = 0;
    while (i < merged.size()) {
        size_t nl = merged.find(L'\n', i);
        std::wstring line = Trim(merged.substr(i, nl == std::wstring::npos ? std::wstring::npos : nl - i));
        i = (nl == std::wstring::npos) ? merged.size() : nl + 1;
        if (line.empty() || line[0] == L';') continue;
        if (line.front() == L'[' && line.back() == L']') {
            inVars = Lower(Trim(line.substr(1, line.size() - 2))) == L"variables";
            continue;
        }
        if (!inVars) continue;
        size_t eq = line.find(L'=');
        if (eq != std::wstring::npos)
            vars[Lower(Trim(line.substr(0, eq)))] = Trim(line.substr(eq + 1));
    }

    return SubVars(merged, vars);
}

} // namespace

MenuHost* MenuHost::Attach(HWND hwnd, void* skin, void* rm, const std::wstring& menuPath) {
    auto it = g_hosts.find(hwnd);
    if (it != g_hosts.end()) {
        it->second->SetActive(skin, rm, menuPath);
        ++it->second->refs_;
        return it->second;
    }
    auto* host = new MenuHost();
    host->hwnd_ = hwnd;
    host->SetActive(skin, rm, menuPath);
    host->refs_ = 1;
    g_hosts[hwnd] = host;
    SetWindowSubclass(hwnd, &MenuHost::SubProc, kSubclassId, reinterpret_cast<DWORD_PTR>(host));
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
    if (--host->refs_ > 0) return;
    RemoveWindowSubclass(hwnd, &MenuHost::SubProc, kSubclassId);
    g_hosts.erase(it);
    delete host;
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
    std::wstring text = ExpandSource(menuPath_, rm_);
    if (text.empty()) return;
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
    RmExecute(skin_, resolved);
}
