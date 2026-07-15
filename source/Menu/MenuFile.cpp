#include "Menu/MenuFile.h"
#include <windows.h>
#include <algorithm>
#include <cwctype>
#include <cwchar>
#include <utility>
#include <vector>

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

// Split "a,b,c" into up to `max` trimmed integer tokens; returns count parsed.
static int ParseInts(const std::wstring& v, int* out, int max) {
    int n = 0; size_t i = 0;
    while (i <= v.size() && n < max) {
        size_t comma = v.find(L',', i);
        std::wstring tok = Trim(v.substr(i, comma == std::wstring::npos ? std::wstring::npos : comma - i));
        wchar_t* end = nullptr; long r = wcstol(tok.c_str(), &end, 10);
        if (end == tok.c_str()) break;
        out[n++] = (int)r;
        if (comma == std::wstring::npos) break;
        i = comma + 1;
    }
    return n;
}

// Strip exactly one outer pair of double quotes: "hi" -> hi, ""hi"" -> "hi".
static std::wstring StripQuotes(const std::wstring& v) {
    if (v.size() >= 2 && v.front() == L'"' && v.back() == L'"')
        return v.substr(1, v.size() - 2);
    return v;
}

// 0=left 1=center 2=right; def when unrecognized.
static int ParseAlign(const std::wstring& v, int def) {
    std::wstring s = Lower(Trim(v));
    if (s == L"left") return 0;
    if (s == L"center" || s == L"centre") return 1;
    if (s == L"right") return 2;
    return def;
}

// FontCase: 0=none 1=upper 2=lower 3=proper.
static int ParseCase(const std::wstring& v) {
    std::wstring s = Lower(Trim(v));
    if (s == L"upper")  return 1;
    if (s == L"lower")  return 2;
    if (s == L"proper") return 3;
    return 0;
}

// FontWeight -> Gdiplus FontStyle int: bold=1 italic=2 underscored/underline=4; none/else=0.
static int ParseFontStyle(const std::wstring& v) {
    std::wstring s = Lower(Trim(v));
    if (s == L"bold")   return 1;
    if (s == L"italic") return 2;
    if (s == L"underscored" || s == L"underline") return 4;
    return 0;
}

// Parse "Linear <angle> ; r,g,b,a ; r,g,b,a [; ...]" into angle+stops.
static bool ParseGradient(const std::wstring& val, double& angle, std::vector<Color>& stops) {
    Color c; double a = 0; int seg = 0; size_t i = 0;
    while (i <= val.size()) {
        size_t semi = val.find(L';', i);
        std::wstring tok = Trim(val.substr(i, semi == std::wstring::npos ? std::wstring::npos : semi - i));
        if (seg == 0) {
            size_t sp = tok.find_last_of(L" \t");
            if (sp != std::wstring::npos) a = wcstod(Trim(tok.substr(sp + 1)).c_str(), nullptr);
        } else if (ParseColor(tok, c)) {
            stops.push_back(c);
        }
        ++seg;
        if (semi == std::wstring::npos) break;
        i = semi + 1;
    }
    angle = a;
    return stops.size() >= 2;
}

static bool Truthy(const std::wstring& v);

static void ApplyBoxKey(BoxStyle& s, const std::wstring& key, const std::wstring& val) {
    std::wstring k = Lower(key); Color c;
    if (k == L"antialias") { s.aa = Truthy(val); s.aaSet = true; return; }
    if (k == L"bgcolor") { if (ParseColor(val, c)) { s.color = c; s.hasColor = true; } }
    else if (k == L"bghovercolor") { if (ParseColor(val, c)) { s.hoverColor = c; s.hasHoverColor = true; } }
    else if (k == L"gradient") {
        std::vector<Color> stops; double angle = 0;
        if (ParseGradient(val, angle, stops)) { s.hasGradient = true; s.gradAngle = angle; s.gradStops = std::move(stops); }
    }
    else if (k == L"gradienthover") {
        std::vector<Color> stops; double angle = 0;
        if (ParseGradient(val, angle, stops)) { s.hasHoverGradient = true; s.hoverGradAngle = angle; s.hoverGradStops = std::move(stops); }
    }
    else if (k == L"bgimage")     s.image = val;
    else if (k == L"height")      { s.height = ParseInt(val, 0); s.heightSet = true; }
    else if (k == L"strokewidth") s.strokeW = ParseInt(val, 0);
    else if (k == L"strokecolor") { if (ParseColor(val, c)) s.stroke = c; }
    else if (k == L"cornerradius"){ s.cornerRadius = ParseInt(val, 0); s.cornerSet = true; }
    else if (k == L"shadow")      {
        // "size ; r,g,b,a"
        size_t semi = val.find(L';');
        if (semi != std::wstring::npos) {
            s.shadowSize = ParseInt(Trim(val.substr(0, semi)), 0);
            if (ParseColor(Trim(val.substr(semi + 1)), c)) s.shadow = c;
        }
    }
    else if (k == L"shadowoffset"){ int xy[2]; if (ParseInts(val, xy, 2) == 2) { s.shadowOffX = xy[0]; s.shadowOffY = xy[1]; } }
    else if (k == L"padding")     {
        int p[4]; int n = ParseInts(val, p, 4);
        if (n == 1)      { s.padL = s.padT = s.padR = s.padB = p[0]; s.padSet = true; }
        else if (n == 2) { s.padL = s.padR = p[0]; s.padT = s.padB = p[1]; s.padSet = true; }
        else if (n == 4) { s.padL = p[0]; s.padT = p[1]; s.padR = p[2]; s.padB = p[3]; s.padSet = true; }
    }
}

static BoxStyle ParseBoxStyle(const KeyVals& kv) {
    BoxStyle s;
    for (auto& p : kv) ApplyBoxKey(s, p.first, p.second);
    return s;
}

static void ApplyBackgroundDefaults(BoxStyle& s) {
    if (!s.hasColor && !s.hasGradient) { s.color = Color{30, 30, 30, 255}; s.hasColor = true; }
    if (!s.cornerSet) { s.cornerRadius = 8; s.cornerSet = true; }
    if (s.shadowSize == 0) { s.shadowSize = 8; s.shadow = Color{0, 0, 0, 90}; s.shadowOffX = 0; s.shadowOffY = 2; }
}

static void ApplyThemeKey(Theme& t, const std::wstring& key, const std::wstring& val) {
    std::wstring k = Lower(key); Color c;
    if      (k == L"fontcolor")       { if (ParseColor(val, c)) t.text = c; }
    else if (k == L"fonthovercolor")  { if (ParseColor(val, c)) t.hoverText = c; }
    else if (k == L"fontface")        t.fontFace = val;
    else if (k == L"fontsize")        t.fontSize = ParseInt(val, t.fontSize);
    else if (k == L"fontalign")       t.fontAlign = ParseAlign(val, t.fontAlign);
    else if (k == L"itemheight")      t.itemHeight = ParseInt(val, t.itemHeight);
    else if (k == L"maxwidth")        t.maxWidth = ParseInt(val, t.maxWidth);
    else if (k == L"width")           {
        std::wstring s = Lower(Trim(val));
        if (s == L"auto") t.widthFixed = false;
        else { t.fixedWidth = ParseInt(val, 0); t.widthFixed = t.fixedWidth > 0; }
    }
    else if (k == L"animfade")         t.animFade = Truthy(val);
}

static bool Truthy(const std::wstring& v) {
    std::wstring s = Lower(Trim(v));
    return s == L"1" || s == L"true" || s == L"yes";
}

static MenuItem BuildItem(const KeyVals& kv) {
    MenuItem it; Color c;
    for (auto& p : kv) {
        std::wstring k = Lower(p.first);
        if      (k == L"text")           it.text = StripQuotes(p.second);
        else if (k == L"action")         it.bang = p.second;
        else if (k == L"icon")           it.icon = p.second;
        else if (k == L"iconpos")        it.iconRight = (Lower(Trim(p.second)) == L"right");
        else if (k == L"disabled")       it.disabled = Truthy(p.second);
        else if (k == L"chevron")        it.showChevron = Truthy(p.second);
        else if (k == L"chevronicon")    it.chevronIcon = p.second;
        else if (k == L"fontweight")     it.fontStyle = ParseFontStyle(p.second);
        else if (k == L"height")         { it.rowHeight = ParseInt(p.second, 0); it.rowHeightSet = true; }
        else if (k == L"fontcolor")      { if (ParseColor(p.second, c)) { it.fontColor = c; it.fontColorSet = true; } }
        else if (k == L"fonthovercolor"){ if (ParseColor(p.second, c)) { it.fontHoverColor = c; it.fontHoverColorSet = true; } }
        else if (k == L"fontsize")       { it.fontSize = ParseInt(p.second, 0); it.fontSizeSet = true; }
        else if (k == L"fontface")       { it.fontFace = p.second; it.fontFaceSet = true; }
        else if (k == L"fontalign")      it.fontAlign = ParseAlign(p.second, -1);
        else if (k == L"fontcase")       it.fontCase = ParseCase(p.second);
    }
    it.box = ParseBoxStyle(kv);
    return it;
}

// A section is a separator when its name's last '\'-segment starts with
// "separator" or "seperator" (case-insensitive).
static bool IsSeparatorName(const std::wstring& section) {
    size_t bs = section.find_last_of(L'\\');
    std::wstring leaf = Lower(bs == std::wstring::npos ? section : section.substr(bs + 1));
    return leaf.compare(0, 9, L"separator") == 0 || leaf.compare(0, 9, L"seperator") == 0;
}

// Separator row: box keys via ParseBoxStyle map the row background; Height=bar
// thickness; Color=bar color (stored in the text-less row's free fontColor);
// ItemHeight=row height override.
static MenuItem BuildSeparator(const KeyVals& kv) {
    MenuItem it; it.separator = true;
    it.box = ParseBoxStyle(kv);   // Height => box.height = bar thickness
    Color c;
    for (auto& p : kv) {
        std::wstring k = Lower(p.first);
        if      (k == L"color")      { if (ParseColor(p.second, c)) { it.fontColor = c; it.fontColorSet = true; } }
        else if (k == L"itemheight") { it.rowHeight = ParseInt(p.second, 0); it.rowHeightSet = true; }
    }
    return it;
}

// The single Title section: last name-segment equals "title" (case-insensitive).
static bool IsTitleName(const std::wstring& section) {
    size_t bs = section.find_last_of(L'\\');
    return Lower(bs == std::wstring::npos ? section : section.substr(bs + 1)) == L"title";
}

// Title row: reuse BuildItem for box/font/icon/height/text, then mark it a
// non-interactive header (drop any interactive fields a stray key set).
static MenuItem BuildTitle(const KeyVals& kv) {
    MenuItem it = BuildItem(kv);
    it.title = true;
    it.disabled = false; it.bang.clear(); it.showChevron = false;
    return it;
}

static const std::wstring* FindVal(const KeyVals& kv, const wchar_t* name) {
    for (auto& p : kv) if (Lower(p.first) == name) return &p.second;
    return nullptr;
}

// Direct children of group `prefix`: sections named  prefix + "\\" + seg  (seg has no backslash),
// in file order. A child that itself declares Submenu recurses on its own full-path group.
static std::vector<MenuItem> BuildGroup(const Sections& sections, const std::wstring& prefix) {
    std::vector<MenuItem> out;
    std::wstring want = prefix + L"\\";
    for (auto& s : sections) {
        if (s.first.size() <= want.size()) continue;
        if (s.first.compare(0, want.size(), want) != 0) continue;
        if (s.first.find(L'\\', want.size()) != std::wstring::npos) continue; // deeper, not direct
        MenuItem it = IsSeparatorName(s.first) ? BuildSeparator(s.second) : BuildItem(s.second);
        if (!it.separator) {
            if (const std::wstring* sub = FindVal(s.second, L"submenu"))
                it.submenu = BuildGroup(sections, *sub);
        }
        out.push_back(std::move(it));
    }
    return out;
}

MenuModel ParseMenu(const std::wstring& text) {
    MenuModel m;
    Sections sections;
    KeyVals menuKv;
    std::wstring section;
    size_t i = 0;
    while (i < text.size()) {
        size_t nl = text.find(L'\n', i);
        std::wstring line = Trim(text.substr(i, nl == std::wstring::npos ? std::wstring::npos : nl - i));
        i = (nl == std::wstring::npos) ? text.size() : nl + 1;
        if (line.empty() || line[0] == L';' || line[0] == L'#') continue;
        if (line.front() == L'[' && line.back() == L']') {
            section = Trim(line.substr(1, line.size() - 2));
            std::wstring sl = Lower(section);
            if (sl != L"menu" && sl != L"variables")
                sections.emplace_back(section, KeyVals{});
            continue;
        }
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring key = Trim(line.substr(0, eq));
        std::wstring val = Trim(line.substr(eq + 1));
        std::wstring sl = Lower(section);
        if (sl == L"menu") menuKv.emplace_back(key, val);
        else if (sl == L"variables") continue;   // consumed by the preprocessor
        else if (!sections.empty()) sections.back().second.emplace_back(key, val);
    }

    for (auto& p : menuKv) ApplyThemeKey(m.theme, p.first, p.second);
    m.theme.background = ParseBoxStyle(menuKv);
    ApplyBackgroundDefaults(m.theme.background);

    // Top-level items: sections with no backslash, in file order.
    bool titleSeen = false;
    for (auto& s : sections) {
        if (s.first.find(L'\\') != std::wstring::npos) continue;
        MenuItem it;
        if (IsSeparatorName(s.first))                it = BuildSeparator(s.second);
        else if (!titleSeen && IsTitleName(s.first)) { it = BuildTitle(s.second); titleSeen = true; }
        else                                         it = BuildItem(s.second);
        if (!it.separator && !it.title) {
            if (const std::wstring* sub = FindVal(s.second, L"submenu"))
                it.submenu = BuildGroup(sections, *sub);
        }
        m.items.push_back(std::move(it));
    }
    return m;
}

std::wstring DecodeBytes(const std::string& bytes) {
    // UTF-16LE BOM
    if (bytes.size() >= 2 && (unsigned char)bytes[0] == 0xFF && (unsigned char)bytes[1] == 0xFE) {
        const wchar_t* p = reinterpret_cast<const wchar_t*>(bytes.data() + 2);
        size_t n = (bytes.size() - 2) / sizeof(wchar_t);
        return std::wstring(p, n);
    }
    // UTF-8, optionally with BOM
    size_t off = 0;
    if (bytes.size() >= 3 && (unsigned char)bytes[0] == 0xEF &&
        (unsigned char)bytes[1] == 0xBB && (unsigned char)bytes[2] == 0xBF)
        off = 3;
    int len = (int)(bytes.size() - off);
    if (len <= 0) return std::wstring();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, bytes.data() + off, len, nullptr, 0);
    std::wstring out(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, bytes.data() + off, len, out.data(), wlen);
    return out;
}

bool ReadMenuFile(const std::wstring& path, std::wstring& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    std::string bytes;
    char buf[4096];
    DWORD got = 0;
    while (ReadFile(h, buf, sizeof(buf), &got, nullptr) && got > 0)
        bytes.append(buf, got);
    CloseHandle(h);
    out = DecodeBytes(bytes);
    return true;
}

} // namespace cm
