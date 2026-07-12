#include "Render/PopupWindow.h"

#include <gdiplus.h>
#include <shellscalingapi.h>
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

void PopupWindow::Show(const cm::MenuModel& model, POINT anchor) {
    EnsureGdiplus();
    RegisterPopupClass();

    const cm::Theme& th = model.theme;

    // DPI scale for the target monitor.
    HMONITOR mon = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
    UINT dpiX = 96, dpiY = 96;
    GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    double scale = dpiX / 96.0;

    const int pad     = (int)(th.padding * scale);
    const int itemH   = (int)(th.itemHeight * scale);
    const int sepH    = (int)(9 * scale);
    const int margin  = (int)(th.shadowBlur * scale);   // shadow breathing room
    const int radius  = (int)(th.cornerRadius * scale);
    const REAL emPx   = (REAL)(th.fontSize * scale * 96.0 / 72.0);

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kClass, L"", WS_POPUP, 0, 0, 0, 0,
        owner_, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd_) return;

    // Measure text to size the body. Use a throwaway 1x1 surface for metrics.
    Bitmap probe(1, 1, PixelFormat32bppPARGB);
    Graphics gm(&probe);
    gm.SetTextRenderingHint(TextRenderingHintAntiAlias);
    FontFamily family(th.fontFace.c_str());
    Font font(&family, emPx, FontStyleRegular, UnitPixel);

    int textMax = 0, bodyH = 0;
    for (const auto& it : model.items) {
        if (it.separator) { bodyH += sepH; continue; }
        RectF box;
        gm.MeasureString(it.text.c_str(), -1, &font, PointF(0, 0), &box);
        textMax = (std::max)(textMax, (int)(box.Width + 0.5f));
        bodyH += itemH;
    }
    int bodyW = textMax + 2 * pad;
    int maxW = (int)(th.maxWidth * scale);
    if (bodyW > maxW) bodyW = maxW;
    if (bodyW < 2 * pad) bodyW = 2 * pad;

    const int winW = bodyW + 2 * margin;
    const int winH = bodyH + 2 * margin;

    // Position: body top-left at the cursor, clamped to the monitor work area.
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    int bodyX = anchor.x, bodyY = anchor.y;
    if (bodyX + bodyW > mi.rcWork.right)  bodyX = mi.rcWork.right - bodyW;
    if (bodyY + bodyH > mi.rcWork.bottom) bodyY = mi.rcWork.bottom - bodyH;
    if (bodyX < mi.rcWork.left) bodyX = mi.rcWork.left;
    if (bodyY < mi.rcWork.top)  bodyY = mi.rcWork.top;
    const int winX = bodyX - margin;
    const int winY = bodyY - margin;

    // Premultiplied ARGB DIB we render into and hand to UpdateLayeredWindow.
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = winW;
    bmi.bmiHeader.biHeight = -winH; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screenDC = GetDC(nullptr);
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC memDC = CreateCompatibleDC(screenDC);
    HGDIOBJ oldBmp = SelectObject(memDC, dib);

    {
        Bitmap surface(winW, winH, winW * 4, PixelFormat32bppPARGB, (BYTE*)bits);
        Graphics g(&surface);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        RectF bodyRect((REAL)margin, (REAL)margin, (REAL)bodyW, (REAL)bodyH);

        // Soft shadow: a single offset, semi-transparent rounded rect. v1 good-enough.
        {
            RectF sh = bodyRect;
            sh.Offset(0, (REAL)(2 * scale));
            GraphicsPath sp;
            AddRoundRect(sp, sh, (REAL)radius);
            SolidBrush shb(G(th.shadow));
            g.FillPath(&shb, &sp);
        }

        // Body fill + border.
        GraphicsPath body;
        AddRoundRect(body, bodyRect, (REAL)radius);
        SolidBrush bgb(G(th.bg));
        g.FillPath(&bgb, &body);
        if (th.borderWidth > 0) {
            Pen border(G(th.border), (REAL)(th.borderWidth * scale));
            g.DrawPath(&border, &body);
        }

        StringFormat sf;
        sf.SetAlignment(StringAlignmentNear);
        sf.SetLineAlignment(StringAlignmentCenter);
        sf.SetFormatFlags(StringFormatFlagsNoWrap);
        SolidBrush textBrush(G(th.text));
        SolidBrush disBrush(G(th.disabledText));
        Pen sepPen(G(th.separator), 1.0f * (REAL)scale);

        itemRects_.clear();
        int y = margin;
        for (const auto& it : model.items) {
            int h = it.separator ? sepH : itemH;
            RECT screenRect{ bodyX + pad, winY + y, bodyX + bodyW - pad, winY + y + h };
            itemRects_.push_back(screenRect);
            if (it.separator) {
                REAL cy = (REAL)(y + h / 2);
                g.DrawLine(&sepPen, (REAL)(margin + pad), cy,
                                    (REAL)(margin + bodyW - pad), cy);
            } else {
                RectF tr((REAL)(margin + pad), (REAL)y,
                         (REAL)(bodyW - 2 * pad), (REAL)h);
                g.DrawString(it.text.c_str(), -1, &font, tr,
                             &sf, it.disabled ? &disBrush : &textBrush);
            }
            y += h;
        }
    } // surface flushes to `bits`

    POINT ptDst{ winX, winY };
    SIZE  size{ winW, winH };
    POINT ptSrc{ 0, 0 };
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd_, screenDC, &ptDst, &size, memDC, &ptSrc, 0, &bf, ULW_ALPHA);

    SelectObject(memDC, oldBmp);
    DeleteObject(dib);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);

    // Local pump. Capture routes outside clicks to us so click-away dismisses.
    // Task 7 turns clicks into hit-test + invoke; for now any click closes.
    SetCapture(hwnd_);
    done_ = false;
    MSG msg;
    while (!done_ && GetMessageW(&msg, nullptr, 0, 0)) {
        switch (msg.message) {
        case WM_LBUTTONDOWN: case WM_LBUTTONUP:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
            done_ = true;
            break;
        case WM_KEYDOWN:
            if (msg.wParam == VK_ESCAPE) done_ = true;
            break;
        case WM_CAPTURECHANGED:
            done_ = true;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    ReleaseCapture();
}
