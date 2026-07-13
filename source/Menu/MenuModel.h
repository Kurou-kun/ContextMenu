#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace cm {

struct Color { uint8_t r = 0, g = 0, b = 0, a = 255; };

// Shared style for the menu background box and each item box.
struct BoxStyle {
    bool  hasColor = false;    Color color;                 // solid fill
    bool  hasGradient = false; double gradAngle = 0;        // linear gradient
    std::vector<Color> gradStops;                           // >=2 to apply
    std::wstring image;                                     // over the fill; empty = none
    int   strokeW = 0;         Color stroke{80, 80, 80, 255};
    bool  cornerSet = false;   int cornerRadius = 0;
    int   shadowSize = 0;      Color shadow{0, 0, 0, 120};  int shadowOffX = 0, shadowOffY = 0;
    bool  padSet = false;      int padL = 0, padT = 0, padR = 0, padB = 0;
};

struct Theme {
    Color text{230, 230, 230, 255};
    Color hoverBg{60, 60, 90, 255};
    Color hoverText{230, 230, 230, 255};
    Color disabledText{120, 120, 120, 255};
    Color separator{80, 80, 80, 255};
    std::wstring fontFace = L"Segoe UI";
    int fontSize = 10;
    int itemHeight = 28;
    int maxWidth = 320;
    BoxStyle background;   // defaults resolved in ParseMenu
};

struct MenuItem {
    std::wstring text;
    std::wstring bang;
    std::wstring icon;
    bool separator = false;
    bool disabled = false;
    BoxStyle box;                    // per-item style (defaults resolved by renderer)
    std::vector<MenuItem> submenu;   // non-empty => fly-out
};

struct MenuModel {
    Theme theme;
    std::vector<MenuItem> items;
};

} // namespace cm
