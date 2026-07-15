#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace cm {

struct Color { uint8_t r = 0, g = 0, b = 0, a = 255; };

// Shared style for the menu background box, item boxes, and separator rows.
struct BoxStyle {
    bool  hasColor = false;    Color color;                 // BgColor
    bool  hasGradient = false; double gradAngle = 0;        // Gradient
    std::vector<Color> gradStops;                           // >=2 to apply
    bool  hasHoverColor = false;    Color hoverColor;       // BgHoverColor
    bool  hasHoverGradient = false; double hoverGradAngle = 0; // GradientHover
    std::vector<Color> hoverGradStops;
    std::wstring image;                                     // BgImage; empty = none
    int   strokeW = 0;         Color stroke{80, 80, 80, 255}; // StrokeWidth/StrokeColor
    bool  cornerSet = false;   int cornerRadius = 0;
    int   shadowSize = 0;      Color shadow{0, 0, 0, 120};  int shadowOffX = 0, shadowOffY = 0;
    bool  padSet = false;      int padL = 0, padT = 0, padR = 0, padB = 0;
    bool  heightSet = false;   int height = 0;              // separator bar thickness
    bool  aaSet = false;       bool aa = false;             // AntiAlias (per-section; default off)
};

struct Theme {
    Color text{230, 230, 230, 255};        // FontColor default
    Color hoverText{230, 230, 230, 255};   // FontHoverColor default
    std::wstring fontFace = L"Segoe UI";
    int  fontSize = 10;
    int  fontAlign = 0;                    // 0=left 1=center 2=right
    int  itemHeight = 28;
    int  maxWidth = 320;
    bool widthFixed = false; int fixedWidth = 0;   // Width=<px>
    bool animFade = false;                         // AnimFade: fade popup in/out
    bool iconPadding = false;                       // IconPadding: icon-less rows reserve the icon column
    Color separatorFallback{80, 80, 80, 255};      // bar color when a separator sets none
    BoxStyle background;   // includes BgHoverColor/GradientHover defaults; resolved in ParseMenu
};

struct MenuItem {
    std::wstring text;
    std::wstring bang;               // Action
    std::wstring icon;
    bool separator = false;
    bool title     = false;          // [Title] non-interactive header row
    bool disabled  = false;
    bool iconRight = false;          // IconPos=Right
    bool showChevron = true;         // Chevron (default on)
    std::wstring chevronIcon;        // ChevronIcon: custom image, else drawn triangle
    bool rowHeightSet = false; int rowHeight = 0;   // item Height / separator ItemHeight

    // Text overrides; *Set false => fall back to Theme.
    bool fontColorSet = false;      Color fontColor;
    bool fontHoverColorSet = false; Color fontHoverColor;
    bool fontSizeSet = false; int fontSize = 0;
    bool fontFaceSet = false; std::wstring fontFace;
    int  fontAlign = -1;            // -1 unset, else 0/1/2
    int  fontCase = 0;             // FontCase: 0=none 1=upper 2=lower 3=proper
    int  fontStyle = 0;            // FontWeight -> Gdiplus FontStyle: 0=regular 1=bold 2=italic 4=underline

    BoxStyle box;                    // per-row box (incl. hover fill, and bar thickness for separators)
    std::vector<MenuItem> submenu;   // non-empty => fly-out
};

struct MenuModel {
    Theme theme;
    std::vector<MenuItem> items;
};

} // namespace cm
