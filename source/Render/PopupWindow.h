#pragma once
#include <windows.h>
#include <vector>
#include "Menu/MenuModel.h"

// A layered GDI+ popup that renders a MenuModel and blocks until dismissed.
// Task 6: render + close on any click / capture loss. Interaction (hover,
// invoke, submenus) arrives in later tasks.
class PopupWindow {
public:
    PopupWindow(HWND owner, void* skin, void* rm);
    ~PopupWindow();

    // Renders the model at a screen anchor and runs a local pump until dismissed.
    void Show(const cm::MenuModel& model, POINT anchor);

private:
    HWND  owner_;
    void* skin_;
    void* rm_;
    HWND  hwnd_ = nullptr;
    bool  done_ = false;
    std::vector<RECT> itemRects_; // screen-relative, per item (for Task 7 hit-test)

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
};
