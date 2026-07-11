#include "Menu/MenuFile.h"
#include <cassert>
#include <cstdio>
using namespace cm;

static void test_color() {
    Color c;
    assert(ParseColor(L"10,20,30", c) && c.r==10 && c.g==20 && c.b==30 && c.a==255);
    assert(ParseColor(L"1,2,3,4", c) && c.a==4);
    assert(ParseColor(L" 255 , 0 , 128 ", c) && c.r==255 && c.b==128); // whitespace tolerant
    assert(!ParseColor(L"1,2", c));        // too few
    assert(!ParseColor(L"x,y,z", c));      // non-numeric
    assert(!ParseColor(L"300,0,0", c));    // out of range
}

static void test_theme_defaults_and_overrides() {
    MenuModel m = ParseMenu(L"[Menu]\nBgColor=1,2,3\nFontFace=Arial\nItemHeight=40\n");
    assert(m.theme.bg.r==1 && m.theme.bg.g==2 && m.theme.bg.b==3);
    assert(m.theme.fontFace==L"Arial");
    assert(m.theme.itemHeight==40);
    assert(m.theme.text.r==230);           // untouched default
    assert(m.theme.cornerRadius==8);       // untouched default
}

int main() {
    test_color();
    test_theme_defaults_and_overrides();
    std::printf("ALL PASS\n");
    return 0;
}
