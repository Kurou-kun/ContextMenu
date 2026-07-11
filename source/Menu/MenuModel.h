#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace cm {

struct Color { uint8_t r = 0, g = 0, b = 0, a = 255; };

struct Theme {
    Color bg{30, 30, 30, 255};
    Color text{230, 230, 230, 255};
    Color hoverBg{60, 60, 90, 255};
    Color hoverText{230, 230, 230, 255};
    Color disabledText{120, 120, 120, 255};
    Color separator{80, 80, 80, 255};
    Color border{80, 80, 80, 255};
    Color shadow{0, 0, 0, 120};
    int borderWidth = 1;
    std::wstring fontFace = L"Segoe UI";
    int fontSize = 10;
    int itemHeight = 28;
    int padding = 8;
    int cornerRadius = 8;
    int shadowBlur = 12;
    int maxWidth = 320;
};

struct MenuItem {
    std::wstring text;
    std::wstring bang;
    std::wstring icon;
    bool separator = false;
    bool disabled = false;
    std::vector<MenuItem> submenu; // non-empty => fly-out
};

struct MenuModel {
    Theme theme;
    std::vector<MenuItem> items;
};

} // namespace cm
