#include "Render/PopupWindow.h"

#include <gdiplus.h>
#include <shellscalingapi.h>
#include <windowsx.h>
#include <mutex>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shcore.lib")

using namespace Gdiplus;

// GDI+ started once, never shut down: the DLL lives for the whole Rainmeter
// process and GdiplusShutdown on unload is fragile. ponytail: leak the token.
static void EnsureGdiplus() {
    static std::once_flag once;
    static ULONG_PTR token = 0;
    std::call_once(once, [] {
        GdiplusStartupInput in;
        GdiplusStartup(&token, &in, nullptr);
    });
}

static const wchar_t* kClass = L"ContextMenuPopup";

static void RegisterPopupClass() {
    static std::once_flag once;
    std::call_once(once, [] {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &PopupWindow::WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kClass;
        RegisterClassExW(&wc);
    });
}

static Gdiplus::Color G(const cm::Color& c) {
    return Gdiplus::Color(c.a, c.r, c.g, c.b);
}

static void AddRoundRect(GraphicsPath& path, RectF r, REAL rad) {
    if (rad <= 0) { path.AddRectangle(r); return; }
    REAL d = rad * 2;
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
    path.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
    path.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
    path.CloseFigure();
}

LRESULT CALLBACK PopupWindow::WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    return DefWindowProcW(h, m, w, l);
}

PopupWindow::PopupWindow(HWND owner, void* skin, void* rm)
    : owner_(owner), skin_(skin), rm_(rm) {}

PopupWindow::~PopupWindow() {
    if (hwnd_) DestroyWindow(hwnd_);
}

int PopupWindow::HitTest(POINT p) const {
    for (size_t i = 0; i < itemRects_.size(); ++i) {
        const RECT& r = itemRects_[i];
        if (p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom) {
            const cm::MenuItem& it = model_->items[i];
            if (it.separator || it.disabled) return -1;
            return (int)i;
        }
    }
    return -1;
}

void PopupWindow::Paint() {
    const cm::Theme& th = model_->theme;
    const int pad = pad_, margin = margin_, radius = radius_;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = winW_;
    bmi.bmiHeader.biHeight = -winH_; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screenDC = GetDC(nullptr);
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC memDC = CreateCompatibleDC(screenDC);
    HGDIOBJ oldBmp = SelectObject(memDC, dib);

    {
        Bitmap surface(winW_, winH_, winW_ * 4, PixelFormat32bppPARGB, (BYTE*)bits);
        Graphics g(&surface);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        FontFamily family(th.fontFace.c_str());
        Font font(&family, emPx_, FontStyleRegular, UnitPixel);

        RectF bodyRect((REAL)margin, (REAL)margin, (REAL)bodyW_, (REAL)bodyH_);

        // Soft shadow: single offset, semi-transparent rounded rect. v1 good-enough.
        {
            RectF sh = bodyRect;
            sh.Offset(0, (REAL)(2 * scale_));
            GraphicsPath sp;
            AddRoundRect(sp, sh, (REAL)radius);
            SolidBrush shb(G(th.shadow));
            g.FillPath(&shb, &sp);
        }

        GraphicsPath body;
        AddRoundRect(body, bodyRect, (REAL)radius);
        SolidBrush bgb(G(th.bg));
        g.FillPath(&bgb, &body);
        if (th.borderWidth > 0) {
            Pen border(G(th.border), (REAL)(th.borderWidth * scale_));
            g.DrawPath(&border, &body);
        }

        StringFormat sf;
        sf.SetAlignment(StringAlignmentNear);
        sf.SetLineAlignment(StringAlignmentCenter);
        sf.SetFormatFlags(StringFormatFlagsNoWrap);
        SolidBrush textBrush(G(th.text));
        SolidBrush hoverText(G(th.hoverText));
        SolidBrush disBrush(G(th.disabledText));
        SolidBrush hoverBg(G(th.hoverBg));
        Pen sepPen(G(th.separator), 1.0f * (REAL)scale_);

        for (size_t i = 0; i < model_->items.size(); ++i) {
            const cm::MenuItem& it = model_->items[i];
            const RECT& r = itemRects_[i]; // client coords
            if (it.separator) {
                REAL cy = (REAL)((r.top + r.bottom) / 2);
                g.DrawLine(&sepPen, (REAL)(margin + pad), cy,
                                    (REAL)(margin + bodyW_ - pad), cy);
                continue;
            }
            bool hot = ((int)i == hovered_);
            if (hot) {
                GraphicsPath hp;
                RectF hr((REAL)r.left, (REAL)r.top,
                         (REAL)(r.right - r.left), (REAL)(r.bottom - r.top));
                AddRoundRect(hp, hr, (REAL)(radius / 2));
                g.FillPath(&hoverBg, &hp);
            }
            RectF tr((REAL)(margin + pad), (REAL)r.top,
                     (REAL)(bodyW_ - 2 * pad), (REAL)(r.bottom - r.top));
            SolidBrush* brush = it.disabled ? &disBrush : (hot ? &hoverText : &textBrush);
            g.DrawString(it.text.c_str(), -1, &font, tr, &sf, brush);
        }
    } // surface flushes to `bits`

    POINT ptDst{ winX_, winY_ };
    SIZE  size{ winW_, winH_ };
    POINT ptSrc{ 0, 0 };
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd_, screenDC, &ptDst, &size, memDC, &ptSrc, 0, &bf, ULW_ALPHA);

    SelectObject(memDC, oldBmp);
    DeleteObject(dib);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

std::wstring PopupWindow::Show(const cm::MenuModel& model, POINT anchor) {
    EnsureGdiplus();
    RegisterPopupClass();
    model_ = &model;

    const cm::Theme& th = model.theme;

    HMONITOR mon = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
    UINT dpiX = 96, dpiY = 96;
    GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    scale_ = dpiX / 96.0;

    pad_    = (int)(th.padding * scale_);
    margin_ = (int)(th.shadowBlur * scale_);
    radius_ = (int)(th.cornerRadius * scale_);
    emPx_   = (REAL)(th.fontSize * scale_ * 96.0 / 72.0);
    const int itemH = (int)(th.itemHeight * scale_);
    const int sepH  = (int)(9 * scale_);

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kClass, L"", WS_POPUP, 0, 0, 0, 0,
        owner_, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd_) return L"";

    // Measure text to size the body.
    Bitmap probe(1, 1, PixelFormat32bppPARGB);
    Graphics gm(&probe);
    gm.SetTextRenderingHint(TextRenderingHintAntiAlias);
    FontFamily family(th.fontFace.c_str());
    Font font(&family, emPx_, FontStyleRegular, UnitPixel);

    int textMax = 0;
    bodyH_ = 0;
    for (const auto& it : model.items) {
        if (it.separator) { bodyH_ += sepH; continue; }
        RectF box;
        gm.MeasureString(it.text.c_str(), -1, &font, PointF(0, 0), &box);
        textMax = (std::max)(textMax, (int)(box.Width + 0.5f));
        bodyH_ += itemH;
    }
    bodyW_ = textMax + 2 * pad_;
    int maxW = (int)(th.maxWidth * scale_);
    if (bodyW_ > maxW) bodyW_ = maxW;
    if (bodyW_ < 2 * pad_) bodyW_ = 2 * pad_;

    winW_ = bodyW_ + 2 * margin_;
    winH_ = bodyH_ + 2 * margin_;

    // Position: body top-left at cursor, clamped to the monitor work area.
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    int bodyX = anchor.x, bodyY = anchor.y;
    if (bodyX + bodyW_ > mi.rcWork.right)  bodyX = mi.rcWork.right - bodyW_;
    if (bodyY + bodyH_ > mi.rcWork.bottom) bodyY = mi.rcWork.bottom - bodyH_;
    if (bodyX < mi.rcWork.left) bodyX = mi.rcWork.left;
    if (bodyY < mi.rcWork.top)  bodyY = mi.rcWork.top;
    winX_ = bodyX - margin_;
    winY_ = bodyY - margin_;

    // Item row rects in client coords (0,0 = window top-left = winX_,winY_).
    itemRects_.clear();
    int y = margin_;
    for (const auto& it : model.items) {
        int h = it.separator ? sepH : itemH;
        itemRects_.push_back(RECT{ margin_, y, margin_ + bodyW_, y + h });
        y += h;
    }

    Paint();
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);

    // Local pump. Capture routes outside clicks to us so click-away dismisses.
    SetCapture(hwnd_);
    done_ = false;
    int clicked = -1;
    MSG msg;
    while (!done_ && GetMessageW(&msg, nullptr, 0, 0)) {
        switch (msg.message) {
        case WM_MOUSEMOVE: {
            POINT p{ GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam) };
            int h = HitTest(p);
            if (h != hovered_) { hovered_ = h; Paint(); }
            break;
        }
        case WM_LBUTTONUP: {
            POINT p{ GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam) };
            int h = HitTest(p);
            if (h >= 0) { clicked = h; done_ = true; }   // invoke after teardown
            else        { done_ = true; }                // click-away
            break;
        }
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
            done_ = true;
            break;
        case WM_KEYDOWN:
            if (msg.wParam == VK_ESCAPE) done_ = true;
            break;
        case WM_CAPTURECHANGED:
            done_ = true;
            break;
        case WM_ACTIVATEAPP:
            if (msg.wParam == FALSE) done_ = true;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    ReleaseCapture();

    // Tear the window down first so the bang's effect shows against a clean
    // screen. The caller runs the bang after we return (see header).
    if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
    return clicked >= 0 ? model_->items[clicked].bang : std::wstring();
}
