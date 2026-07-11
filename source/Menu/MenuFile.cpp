#include "Menu/MenuFile.h"
#include <algorithm>
#include <cwctype>
#include <cwchar>
#include <utility>

using KeyVals = std::vector<std::pair<std::wstring, std::wstring>>;
using Sections = std::vector<std::pair<std::wstring, KeyVals>>;

namespace cm {

static std::wstring Trim(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a])) ++a;
    while (b > a && iswspace(s[b - 1])) --b;
    return s.substr(a, b - a);
}
static std::wstring Lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    return s;
}

bool ParseColor(const std::wstring& value, Color& out) {
    int parts[4]; int n = 0;
    size_t i = 0;
    while (i <= value.size()) {
        size_t comma = value.find(L',', i);
        std::wstring tok = Trim(value.substr(i, comma == std::wstring::npos ? std::wstring::npos : comma - i));
        if (n >= 4) return false;
        if (tok.empty()) return false;
        wchar_t* end = nullptr;
        long v = wcstol(tok.c_str(), &end, 10);
        if (end == tok.c_str() || *end != L'\0' || v < 0 || v > 255) return false;
        parts[n++] = (int)v;
        if (comma == std::wstring::npos) break;
        i = comma + 1;
    }
    if (n < 3) return false;
    out.r = (uint8_t)parts[0]; out.g = (uint8_t)parts[1]; out.b = (uint8_t)parts[2];
    out.a = (n == 4) ? (uint8_t)parts[3] : (uint8_t)255;
    return true;
}

static int ParseInt(const std::wstring& v, int def) {
    wchar_t* end = nullptr; long r = wcstol(v.c_str(), &end, 10);
    return (end == v.c_str()) ? def : (int)r;
}

static void ApplyThemeKey(Theme& t, const std::wstring& key, const std::wstring& val) {
    std::wstring k = Lower(key); Color c;
    if      (k == L"bgcolor")           { if (ParseColor(val, c)) t.bg = c; }
    else if (k == L"textcolor")         { if (ParseColor(val, c)) t.text = c; }
    else if (k == L"hoverbgcolor")      { if (ParseColor(val, c)) t.hoverBg = c; }
    else if (k == L"hovertextcolor")    { if (ParseColor(val, c)) t.hoverText = c; }
    else if (k == L"disabledtextcolor") { if (ParseColor(val, c)) t.disabledText = c; }
    else if (k == L"separatorcolor")    { if (ParseColor(val, c)) t.separator = c; }
    else if (k == L"bordercolor")       { if (ParseColor(val, c)) t.border = c; }
    else if (k == L"shadowcolor")       { if (ParseColor(val, c)) t.shadow = c; }
    else if (k == L"borderwidth")       t.borderWidth = ParseInt(val, t.borderWidth);
    else if (k == L"fontface")          t.fontFace = val;
    else if (k == L"fontsize")          t.fontSize = ParseInt(val, t.fontSize);
    else if (k == L"itemheight")        t.itemHeight = ParseInt(val, t.itemHeight);
    else if (k == L"padding")           t.padding = ParseInt(val, t.padding);
    else if (k == L"cornerradius")      t.cornerRadius = ParseInt(val, t.cornerRadius);
    else if (k == L"shadowblur")        t.shadowBlur = ParseInt(val, t.shadowBlur);
    else if (k == L"maxwidth")          t.maxWidth = ParseInt(val, t.maxWidth);
}

static bool Truthy(const std::wstring& v) {
    std::wstring s = Lower(Trim(v));
    return s == L"1" || s == L"true" || s == L"yes";
}

static MenuItem BuildItem(const KeyVals& kv) {
    MenuItem it;
    for (auto& p : kv) {
        std::wstring k = Lower(p.first);
        if      (k == L"text")      it.text = p.second;
        else if (k == L"bang")      it.bang = p.second;
        else if (k == L"icon")      it.icon = p.second;
        else if (k == L"separator") it.separator = Truthy(p.second);
        else if (k == L"disabled")  it.disabled = Truthy(p.second);
    }
    return it;
}

MenuModel ParseMenu(const std::wstring& text) {
    MenuModel m;
    Sections sections;
    std::wstring section;
    size_t i = 0;
    while (i < text.size()) {
        size_t nl = text.find(L'\n', i);
        std::wstring line = Trim(text.substr(i, nl == std::wstring::npos ? std::wstring::npos : nl - i));
        i = (nl == std::wstring::npos) ? text.size() : nl + 1;
        if (line.empty() || line[0] == L';' || line[0] == L'#') continue;
        if (line.front() == L'[' && line.back() == L']') {
            section = Trim(line.substr(1, line.size() - 2));
            if (Lower(section) != L"menu")
                sections.emplace_back(section, KeyVals{});
            continue;
        }
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring key = Trim(line.substr(0, eq));
        std::wstring val = Trim(line.substr(eq + 1));
        if (Lower(section) == L"menu") ApplyThemeKey(m.theme, key, val);
        else if (!sections.empty()) sections.back().second.emplace_back(key, val);
    }

    // Top-level items: sections with no backslash, in file order.
    // Submenu resolution is added in Task 3.
    for (auto& s : sections) {
        if (s.first.find(L'\\') != std::wstring::npos) continue;
        m.items.push_back(BuildItem(s.second));
    }
    return m;
}

} // namespace cm
