#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include "Menu/MenuModel.h"

namespace Gdiplus { class Bitmap; class Graphics; class RectF; }

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
    cm::MenuModel        owned_;      // backing store when this is a submenu child
    std::vector<RECT>    itemRects_;  // client-relative row rects, per item
    int   hovered_ = -1;

    // Fly-out child. Each popup owns at most one child, so the open chain is a
    // linked list of arbitrary depth (root -> child -> child ...). The root's
    // Show() pump routes every mouse message to the deepest popup under it.
    std::unique_ptr<PopupWindow> child_;
    int childParent_ = -1;            // this popup's item index that owns child_

    // Per-item icons (aligned with model_->items; null slots = no/failed icon).
    std::vector<std::unique_ptr<Gdiplus::Bitmap>> icons_;
    bool hasIcons_ = false;

    // Box-style images: whole-menu background + per-item (aligned with items).
    std::unique_ptr<Gdiplus::Bitmap> bgImage_;
    std::vector<std::unique_ptr<Gdiplus::Bitmap>> itemImages_;

    // Geometry cached for repaint (all pixels, DPI-scaled).
    double scale_ = 1.0;
    int winW_ = 0, winH_ = 0, winX_ = 0, winY_ = 0;
    int bodyW_ = 0, bodyH_ = 0, margin_ = 0, pad_ = 0, radius_ = 0;
    int iconSlot_ = 0, iconPx_ = 0, chevronReserve_ = 0;
    float emPx_ = 0;

    // Create + render the window without pumping (root drives one shared loop).
    void Open(const cm::MenuModel& model, POINT anchor, bool asSubmenu, int parentLeftX);
    void Paint();
    // Draw one styled box (shadow -> fill -> image -> stroke). corner is device px.
    void DrawBox(Gdiplus::Graphics& g, Gdiplus::RectF box, const cm::BoxStyle& s,
                 int cornerPx, Gdiplus::Bitmap* image);
    int  HitTest(POINT clientPt) const;     // item index, or -1 (screen->client)
    int  HitTestScreen(POINT screenPt) const;
    bool ContainsScreen(POINT screenPt) const; // inside this popup's body rect
    PopupWindow* DeepestAt(POINT screenPt);     // deepest popup in chain over pt, or null
    void OpenChildFor(int itemIndex);
    void CloseChild();

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
};
