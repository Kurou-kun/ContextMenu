#include "Render/PopupWindow.h"

#include <gdiplus.h>
#include <shellscalingapi.h>
#include <windowsx.h>
#include <mutex>
#include <algorithm>
#include <cstdlib>
#include "RainmeterAPI.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shcore.lib")

using namespace Gdiplus;

// Load an icon file into a GDI+ bitmap. .ico goes via LoadImage (picks the
// closest embedded size); .png/.bmp/others load directly. Returns null on any
// failure so a bad path just leaves the row text-only.
static Bitmap* LoadIconBitmap(const std::wstring& path, int px) {
    size_t dot = path.find_last_of(L'.');
    std::wstring ext = dot == std::wstring::npos ? L"" : path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

    if (ext == L"ico") {
        HICON h = (HICON)LoadImageW(nullptr, path.c_str(), IMAGE_ICON, px, px, LR_LOADFROMFILE);
        if (!h) return nullptr;
        Bitmap* bmp = Bitmap::FromHICON(h);
        DestroyIcon(h);
        if (bmp && bmp->GetLastStatus() != Ok) { delete bmp; return nullptr; }
        return bmp;
    }
    Bitmap* bmp = Bitmap::FromFile(path.c_str());
    if (bmp && bmp->GetLastStatus() != Ok) { delete bmp; return nullptr; }
    return bmp;
}

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

// Fill/image/stroke/shadow for one styled box, scaled. corner is device px.
void PopupWindow::DrawBox(Graphics& g, RectF box, const cm::BoxStyle& s, int corner, Bitmap* image) {
    // Shadow: single offset rounded rect (ponytail: not a real gaussian blur).
    if (s.shadowSize > 0 && s.shadow.a > 0) {
        RectF sh = box;
        sh.Offset((REAL)(s.shadowOffX * scale_), (REAL)(s.shadowOffY * scale_));
        GraphicsPath sp; AddRoundRect(sp, sh, (REAL)corner);
        SolidBrush shb(G(s.shadow)); g.FillPath(&shb, &sp);
    }
    GraphicsPath path; AddRoundRect(path, box, (REAL)corner);
    if (s.hasGradient && s.gradStops.size() >= 2) {
        LinearGradientBrush br(box, G(s.gradStops.front()), G(s.gradStops.back()), (REAL)s.gradAngle);
        int n = (int)s.gradStops.size();
        std::vector<Gdiplus::Color> cols(n); std::vector<REAL> pos(n);
        for (int i = 0; i < n; ++i) { cols[i] = G(s.gradStops[i]); pos[i] = (REAL)i / (n - 1); }
        br.SetInterpolationColors(cols.data(), pos.data(), n);
        g.FillPath(&br, &path);
    } else if (s.hasColor) {
        SolidBrush br(G(s.color)); g.FillPath(&br, &path);
    }
    if (image) {
        g.SetClip(&path);
        g.DrawImage(image, box);
        g.ResetClip();
    }
    if (s.strokeW > 0 && s.stroke.a > 0) {
        REAL sw = (REAL)(s.strokeW * scale_);
        RectF ir = box; ir.Inflate(-sw / 2, -sw / 2);
        GraphicsPath ip; AddRoundRect(ip, ir, (std::max)(0.0f, (REAL)corner - sw / 2));
        Pen pen(G(s.stroke), sw); g.DrawPath(&pen, &ip);
    }
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

int PopupWindow::HitTestScreen(POINT sp) const {
    return HitTest(POINT{ sp.x - winX_, sp.y - winY_ });
}

bool PopupWindow::ContainsScreen(POINT sp) const {
    int bx = winX_ + margin_, by = winY_ + margin_;
    return sp.x >= bx && sp.x < bx + bodyW_ && sp.y >= by && sp.y < by + bodyH_;
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
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        FontFamily family(th.fontFace.c_str());
        Font font(&family, emPx_, FontStyleRegular, UnitPixel);

        RectF bodyRect((REAL)margin, (REAL)margin, (REAL)bodyW_, (REAL)bodyH_);

        DrawBox(g, bodyRect, th.background, radius, bgImage_.get());

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
            RectF hr((REAL)r.left, (REAL)r.top,
                     (REAL)(r.right - r.left), (REAL)(r.bottom - r.top));
            int itCorner = it.box.cornerSet ? (int)(it.box.cornerRadius * scale_) : radius_ / 2;
            DrawBox(g, hr, it.box, itCorner, itemImages_[i].get());
            bool hot = ((int)i == hovered_);
            if (hot) {
                GraphicsPath hp; AddRoundRect(hp, hr, (REAL)itCorner);
                g.FillPath(&hoverBg, &hp);
            }
            if (hasIcons_ && icons_[i]) {
                int ix = margin + (iconSlot_ - iconPx_) / 2;
                int iy = r.top + ((r.bottom - r.top) - iconPx_) / 2;
                g.DrawImage(icons_[i].get(), Rect(ix, iy, iconPx_, iconPx_));
            }
            int gutter = hasIcons_ ? iconSlot_ : pad;
            RectF tr((REAL)(margin + gutter), (REAL)r.top,
                     (REAL)(bodyW_ - gutter - pad - chevronReserve_), (REAL)(r.bottom - r.top));
            SolidBrush* brush = it.disabled ? &disBrush : (hot ? &hoverText : &textBrush);
            g.DrawString(it.text.c_str(), -1, &font, tr, &sf, brush);

            if (!it.submenu.empty()) {   // right-pointing chevron
                REAL cx = (REAL)(margin + bodyW_ - pad);
                REAL cy = (REAL)((r.top + r.bottom) / 2);
                REAL s  = (REAL)(4 * scale_);
                PointF tri[3] = { { cx - s, cy - s }, { cx, cy }, { cx - s, cy + s } };
                g.FillPolygon(hot ? &hoverText : &textBrush, tri, 3);
            }
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

void PopupWindow::Open(const cm::MenuModel& model, POINT anchor, bool asSubmenu, int parentLeftX) {
    EnsureGdiplus();
    RegisterPopupClass();
    model_ = &model;

    const cm::Theme& th = model.theme;

    HMONITOR mon = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
    UINT dpiX = 96, dpiY = 96;
    GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    scale_ = dpiX / 96.0;

    const cm::BoxStyle& bg = th.background;
    pad_    = (int)(8 * scale_);   // fixed text gutter; background padding added below
    margin_ = (int)((bg.shadowSize + (std::max)(std::abs(bg.shadowOffX), std::abs(bg.shadowOffY))) * scale_);
    radius_ = (int)(bg.cornerRadius * scale_);
    emPx_   = (REAL)(th.fontSize * scale_ * 96.0 / 72.0);
    const int itemH = (int)(th.itemHeight * scale_);
    const int sepH  = (int)(9 * scale_);

    // Resolve + load per-item icons (Rainmeter vars → absolute path). A menu
    // with any loadable icon reserves a square gutter on the left of every row.
    const int iconPad = (int)(5 * scale_);
    iconPx_ = (std::max)(1, itemH - 2 * iconPad);
    icons_.clear();
    hasIcons_ = false;
    bool hasSub = false;
    for (const auto& it : model.items) {
        if (!it.submenu.empty()) hasSub = true;
        if (it.icon.empty()) { icons_.emplace_back(); continue; }
        LPCWSTR v = RmReplaceVariables(rm_, it.icon.c_str());
        LPCWSTR abs = RmPathToAbsolute(rm_, v);
        Bitmap* bmp = LoadIconBitmap(abs ? abs : (v ? v : L""), iconPx_);
        if (bmp) hasIcons_ = true;
        icons_.emplace_back(bmp);
    }
    iconSlot_ = hasIcons_ ? itemH : 0;
    chevronReserve_ = hasSub ? (int)(14 * scale_) : 0;

    // Box-style images: background + per-item (loaded full-res via LoadIconBitmap).
    bgImage_.reset();
    if (!bg.image.empty()) {
        LPCWSTR v = RmReplaceVariables(rm_, bg.image.c_str());
        LPCWSTR abs = RmPathToAbsolute(rm_, v);
        bgImage_.reset(LoadIconBitmap(abs ? abs : (v ? v : L""), 256));
    }
    itemImages_.clear();
    for (const auto& it : model.items) {
        if (it.box.image.empty()) { itemImages_.emplace_back(); continue; }
        LPCWSTR v = RmReplaceVariables(rm_, it.box.image.c_str());
        LPCWSTR abs = RmPathToAbsolute(rm_, v);
        itemImages_.emplace_back(LoadIconBitmap(abs ? abs : (v ? v : L""), 256));
    }

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kClass, L"", WS_POPUP, 0, 0, 0, 0,
        owner_, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd_) return;

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
    const int leftGutter = hasIcons_ ? iconSlot_ : pad_;
    const int bgPadX = (int)((bg.padL + bg.padR) * scale_);
    bodyW_ = leftGutter + textMax + chevronReserve_ + pad_ + bgPadX;
    int maxW = (int)(th.maxWidth * scale_);
    if (bodyW_ > maxW) bodyW_ = maxW;
    if (bodyW_ < 2 * pad_) bodyW_ = 2 * pad_;

    winW_ = bodyW_ + 2 * margin_;
    winH_ = bodyH_ + 2 * margin_;

    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    int bodyX, bodyY;
    if (asSubmenu) {
        // Prefer opening to the right of the parent row; flip left if it would
        // run off the monitor (right-align the child to the parent's left edge).
        bodyX = anchor.x;
        if (bodyX + bodyW_ > mi.rcWork.right) bodyX = parentLeftX - bodyW_;
        bodyY = anchor.y;
    } else {
        bodyX = anchor.x;
        bodyY = anchor.y;
        if (bodyX + bodyW_ > mi.rcWork.right)  bodyX = mi.rcWork.right - bodyW_;
    }
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
}

PopupWindow* PopupWindow::DeepestAt(POINT sp) {
    PopupWindow* hit = nullptr;
    for (PopupWindow* p = this; p; p = p->child_.get())
        if (p->ContainsScreen(sp)) hit = p;   // last match = deepest (chain has no overlap)
    return hit;
}

void PopupWindow::OpenChildFor(int ri) {
    CloseChild();
    const cm::MenuItem& it = model_->items[ri];
    child_ = std::make_unique<PopupWindow>(hwnd_, skin_, rm_);
    child_->owned_.theme = model_->theme;
    child_->owned_.items = it.submenu;
    const RECT& r = itemRects_[ri];
    POINT anchor{ winX_ + r.right, winY_ + r.top };
    child_->Open(child_->owned_, anchor, true, winX_ + margin_);
    childParent_ = ri;
}

void PopupWindow::CloseChild() {
    child_.reset();       // ~PopupWindow destroys the child window
    childParent_ = -1;
}

std::wstring PopupWindow::Show(const cm::MenuModel& model, POINT anchor) {
    Open(model, anchor, false, 0);
    if (!hwnd_) return L"";

    // One shared pump for the whole chain. Capture stays on the root so every
    // mouse message (over root, child, or outside) routes here in root-client
    // coords; we convert to screen and dispatch to the right popup ourselves.
    SetCapture(hwnd_);
    done_ = false;
    std::wstring chosen;
    MSG msg;
    while (!done_ && GetMessageW(&msg, nullptr, 0, 0)) {
        switch (msg.message) {
        case WM_MOUSEMOVE: {
            POINT sp{ GET_X_LPARAM(msg.lParam) + winX_, GET_Y_LPARAM(msg.lParam) + winY_ };
            PopupWindow* p = DeepestAt(sp);
            if (p) {
                int ri = p->HitTestScreen(sp);
                if (ri != p->hovered_) { p->hovered_ = ri; p->Paint(); }
                if (ri >= 0) {
                    if (!p->model_->items[ri].submenu.empty()) {
                        if (ri != p->childParent_) p->OpenChildFor(ri);
                    } else {
                        p->CloseChild();    // deeper chain collapses under a leaf row
                    }
                }
                // ri < 0 (separator/gap inside p): keep child so diagonal travel works.
            }
            // p == null (in the gutter between popups): keep the whole chain open.
            break;
        }
        case WM_LBUTTONUP: {
            POINT sp{ GET_X_LPARAM(msg.lParam) + winX_, GET_Y_LPARAM(msg.lParam) + winY_ };
            PopupWindow* p = DeepestAt(sp);
            if (!p) { done_ = true; break; }   // click-away past every popup
            int ri = p->HitTestScreen(sp);
            if (ri >= 0) {
                if (p->model_->items[ri].submenu.empty()) {
                    chosen = p->model_->items[ri].bang; done_ = true;
                } else {
                    p->hovered_ = ri;
                    p->OpenChildFor(ri);       // click a parent: just ensure open
                }
            }
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

    // Tear the whole chain down before the caller runs the bang.
    CloseChild();
    if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
    return chosen;
}
