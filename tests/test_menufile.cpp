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
    MenuModel m = ParseMenu(L"[Menu]\nColor=1,2,3\nFontFace=Arial\nItemHeight=40\n");
    assert(m.theme.background.hasColor && m.theme.background.color.r==1 && m.theme.background.color.b==3);
    assert(m.theme.fontFace==L"Arial");
    assert(m.theme.itemHeight==40);
    assert(m.theme.text.r==230);                    // untouched default
    assert(m.theme.background.cornerRadius==8);      // background default
}

static void test_boxstyle() {
    MenuModel m = ParseMenu(
        L"[Menu]\nFill=LinearGradient 90 ; 10,10,10,255 ; 40,40,40,255\nStroke=1,80,80,80\nPadding=4,2\n"
        L"[Btn]\nText=Go\nColor=60,60,90,255\nImage=x.png\nCornerRadius=6\n");
    assert(m.theme.background.hasGradient && m.theme.background.gradStops.size()==2);
    assert(m.theme.background.gradAngle==90);
    assert(m.theme.background.strokeW==1 && m.theme.background.stroke.r==80);
    assert(m.theme.background.padL==4 && m.theme.background.padR==4 && m.theme.background.padT==2 && m.theme.background.padB==2);
    assert(m.theme.background.cornerRadius==8);                              // default (unset)
    assert(m.theme.background.shadowSize==8 && m.theme.background.shadowOffY==2); // default (unset)
    assert(m.items.size()==1 && m.items[0].text==L"Go");
    assert(m.items[0].box.hasColor && m.items[0].box.color.r==60 && m.items[0].box.color.b==90);
    assert(m.items[0].box.image==L"x.png");
    assert(m.items[0].box.cornerSet && m.items[0].box.cornerRadius==6);
}

static void test_items() {
    MenuModel m = ParseMenu(
        L"[Menu]\nColor=1,2,3\n"
        L"[Refresh]\nText=Refresh skin\nBang=!Refresh\nIcon=a.png\n"
        L"[Sep1]\nSeparator=1\n"
        L"[Gone]\nText=Nope\nDisabled=1\n");
    assert(m.items.size()==3);
    assert(m.items[0].text==L"Refresh skin" && m.items[0].bang==L"!Refresh" && m.items[0].icon==L"a.png");
    assert(m.items[0].separator==false && m.items[0].disabled==false);
    assert(m.items[1].separator==true);
    assert(m.items[2].disabled==true && m.items[2].text==L"Nope");
}

static void test_submenu() {
    MenuModel m = ParseMenu(
        L"[Tools]\nText=Tools\nSubmenu=Tools\n"
        L"[Tools\\Edit]\nText=Edit\nBang=!EditSkin\n"
        L"[Tools\\More]\nText=More\nSubmenu=Tools\\More\n"
        L"[Tools\\More\\Log]\nText=Log\nBang=!Log\n");
    assert(m.items.size()==1);
    assert(m.items[0].text==L"Tools" && m.items[0].submenu.size()==2);
    assert(m.items[0].submenu[0].text==L"Edit" && m.items[0].submenu[0].bang==L"!EditSkin");
    assert(m.items[0].submenu[1].text==L"More" && m.items[0].submenu[1].submenu.size()==1);
    assert(m.items[0].submenu[1].submenu[0].text==L"Log");
}

static void test_decode() {
    std::string utf8 = "\xEF\xBB\xBF[Menu]\nFontFace=\xC3\x81""bc\n"; // UTF-8 BOM + U+00C1 "bc"
    std::wstring w = DecodeBytes(utf8);
    MenuModel m = ParseMenu(w);
    assert(m.theme.fontFace == L"Ábc");
    std::string u16; // UTF-16LE BOM "AB"
    u16.push_back((char)0xFF); u16.push_back((char)0xFE);
    u16 += std::string("A\0B\0", 4);
    assert(DecodeBytes(u16) == L"AB");
}

int main() {
    test_color();
    test_theme_defaults_and_overrides();
    test_boxstyle();
    test_items();
    test_submenu();
    test_decode();
    std::printf("ALL PASS\n");
    return 0;
}
