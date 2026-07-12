#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "Menu/MenuModel.h"

// A layered GDI+ popup that renders a MenuModel and blocks until dismissed.
// Handles hover highlight, click-to-invoke (resolves + runs the item bang via
// Rainmeter), and click-away / Esc / deactivate dismissal.
class PopupWindow {
public:
    PopupWindow(HWND owner, void* skin, void* rm);
    ~PopupWindow();

    // Renders the model at a screen anchor and runs a local pump until dismissed.
    // Returns the chosen item's (unresolved) bang, or empty if none was invoked.
    // The caller must run the bang *after* this returns (never inline in the
    // right-click handler) so a !Refresh can't tear the skin window down under us.
    std::wstring Show(const cm::MenuModel& model, POINT anchor);

private:
    HWND  owner_;
    void* skin_;
    void* rm_;
    HWND  hwnd_ = nullptr;
    bool  done_ = false;

    const cm::MenuModel* model_ = nullptr;
    std::vector<RECT>    itemRects_;  // client-relative row rects, per item
    int   hovered_ = -1;

    // Geometry cached for repaint (all pixels, DPI-scaled).
    double scale_ = 1.0;
    int winW_ = 0, winH_ = 0, winX_ = 0, winY_ = 0;
    int bodyW_ = 0, bodyH_ = 0, margin_ = 0, pad_ = 0, radius_ = 0;
    float emPx_ = 0;

    void Paint();
    int  HitTest(POINT clientPt) const;   // item index, or -1

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
};
