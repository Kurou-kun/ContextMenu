#include "Render/PopupWindow.h"

#include <gdiplus.h>
#include <shellscalingapi.h>
#include <windowsx.h>
#include <mutex>
#include <algorithm>
#include <cstdlib>
#include <cwctype>
#include "RainmeterAPI.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "shell32.lib")

using namespace Gdiplus;

// Expand a leading %system% to the Windows System32 directory.
static std::wstring ExpandSystem(const std::wstring& p) {
    size_t at = p.find(L"%system%");
    if (at == std::wstring::npos) return p;
    wchar_t sys[MAX_PATH]; UINT n = GetSystemDirectoryW(sys, MAX_PATH);
    std::wstring out = p; out.replace(at, 8, std::wstring(sys, n));
    return out;
}

// Split "file,index" (trailing signed int after the last comma). Returns true and
// fills base/index when an index suffix is present.
static bool SplitIconIndex(const std::wstring& in, std::wstring& base, int& index) {
    size_t comma = in.find_last_of(L',');
    if (comma == std::wstring::npos) return false;
    std::wstring idx = in.substr(comma + 1);
    wchar_t* end = nullptr; long v = wcstol(idx.c_str(), &end, 10);
    if (end == idx.c_str() || *end != L'\0') return false;   // not a number => real comma path
    base = in.substr(0, comma); index = (int)v; return true;
}

// Load an icon file into a GDI+ bitmap. "file,index" extracts a DLL/EXE icon via
// ExtractIconEx; .ico goes via LoadImage (picks the closest embedded size);
// .png/.bmp/others load directly. Returns null on any failure so a bad path just
// leaves the row text-only.
static Bitmap* LoadIconBitmap(const std::wstring& path, int px) {
    std::wstring base; int index = 0;
    if (SplitIconIndex(path, base, index)) {
        HICON h = nullptr;
        if (ExtractIconExW(base.c_str(), index, &h, nullptr, 1) >= 1 && h) {
            Bitmap* bmp = Bitmap::FromHICON(h);
            DestroyIcon(h);
            if (bmp && bmp->GetLastStatus() != Ok) { delete bmp; return nullptr; }
            return bmp;
        }
        return nullptr;
    }
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

static int    RFontAlign(const cm::MenuItem& it, const cm::Theme& t) { return it.fontAlign >= 0 ? it.fontAlign : t.fontAlign; }
static int    RFontSize (const cm::MenuItem& it, const cm::Theme& t) { return it.fontSizeSet ? it.fontSize : t.fontSize; }
static const std::wstring& RFontFace(const cm::MenuItem& it, const cm::Theme& t) { return it.fontFaceSet ? it.fontFace : t.fontFace; }
static cm::Color RTextColor(const cm::MenuItem& it, const cm::Theme& t, bool hot) {
    cm::Color c = hot ? (it.fontHoverColorSet ? it.fontHoverColor : t.hoverText)
                      : (it.fontColorSet ? it.fontColor : t.text);
    if (it.disabled) c.a = 150;
    return c;
}

// Disabled rows dim their icon/chevron images to match the 150 text alpha.
static const REAL kDisabledAlpha = 150.0f / 255.0f;

// FontCase: 1=upper 2=lower 3=proper (title case); 0/else = verbatim.
static std::wstring ApplyCase(std::wstring s, int mode) {
    if (mode == 1) for (auto& ch : s) ch = towupper(ch);
    else if (mode == 2) for (auto& ch : s) ch = towlower(ch);
    else if (mode == 3) {
        bool start = true;
        for (auto& ch : s) {
            if (iswspace(ch)) { start = true; }
            else { ch = start ? towupper(ch) : towlower(ch); start = false; }
        }
    }
    return s;
}

std::wstring PopupWindow::DisplayText(const cm::MenuItem& it) const {
    std::wstring disp = it.text;
    if (it.title && disp.empty()) {
        LPCWSTR cc = RmReplaceVariables(rm_, L"#CURRENTCONFIG#");
        if (cc) disp = cc;
    }
    return ApplyCase(disp, it.fontCase);
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

// Fill `path` with a hover style: per-item box hover, else the menu bg hover.
void PopupWindow::FillHover(Graphics& g, const RectF& box, GraphicsPath& path,
                            const cm::BoxStyle& item) {
    const cm::BoxStyle& bg = model_->theme.background;
    bool ig = item.hasHoverGradient, ic = item.hasHoverColor;
    const std::vector<cm::Color>& stops = ig ? item.hoverGradStops : bg.hoverGradStops;
    double angle = ig ? item.hoverGradAngle : bg.hoverGradAngle;
    if (ig || (!ic && bg.hasHoverGradient)) {
        if (stops.size() >= 2) {
            LinearGradientBrush br(box, G(stops.front()), G(stops.back()), (REAL)angle);
            int n = (int)stops.size(); std::vector<Gdiplus::Color> cols(n); std::vector<REAL> pos(n);
            for (int i = 0; i < n; ++i) { cols[i] = G(stops[i]); pos[i] = (REAL)i / (n - 1); }
            br.SetInterpolationColors(cols.data(), pos.data(), n);
            g.FillPath(&br, &path);
        }
    } else {
        cm::Color c = ic ? item.hoverColor : (bg.hasHoverColor ? bg.hoverColor : cm::Color{60,60,90,255});
        SolidBrush br(G(c)); g.FillPath(&br, &path);
    }
}

LRESULT CALLBACK PopupWindow::WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    return DefWindowProcW(h, m, w, l);
}

PopupWindow::PopupWindow(HWND owner, void* skin, void* rm)
    : owner_(owner), skin_(skin), rm_(rm) {}

PopupWindow::~PopupWindow() {
    if (hwnd_) DestroyWindow(hwnd_);
    FreeLayer();
}

int PopupWindow::HitTest(POINT p) const {
    for (size_t i = 0; i < itemRects_.size(); ++i) {
        const RECT& r = itemRects_[i];
        if (p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom) {
            const cm::MenuItem& it = model_->items[i];
            if (it.separator || it.disabled || it.title) return -1;
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
    const int margin = margin_, radius = radius_;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = winW_;
    bmi.bmiHeader.biHeight = -winH_; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    FreeLayer();   // drop the previous frame's surface
    HDC screenDC = GetDC(nullptr);
    void* bits = nullptr;
    dib_ = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    memDC_ = CreateCompatibleDC(screenDC);
    oldBmp_ = SelectObject(memDC_, dib_);
    ReleaseDC(nullptr, screenDC);

    {
        Bitmap surface(winW_, winH_, winW_ * 4, PixelFormat32bppPARGB, (BYTE*)bits);
        Graphics g(&surface);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        // Per-section AntiAlias (default off => crisp): each box picks its own.
        auto applyAA = [&](const cm::BoxStyle& b) {
            bool on = b.aaSet && b.aa;
            g.SetSmoothingMode(on ? SmoothingModeAntiAlias : SmoothingModeNone);
            g.SetTextRenderingHint(on ? TextRenderingHintAntiAlias
                                      : TextRenderingHintSingleBitPerPixelGridFit);
        };

        applyAA(th.background);
        RectF bodyRect((REAL)margin, (REAL)margin, (REAL)bodyW_, (REAL)bodyH_);
        DrawBox(g, bodyRect, th.background, radius, bgImage_.get());

        // Per-row horizontal padding (default 0; the user owns all insets).
        auto pL = [&](const cm::MenuItem& it) { return (int)((it.box.padSet ? it.box.padL : 0) * scale_); };
        auto pR = [&](const cm::MenuItem& it) { return (int)((it.box.padSet ? it.box.padR : 0) * scale_); };
        // Blit an image; disabled rows grey it out (luminance) and dim its alpha.
        auto drawImg = [&](Bitmap* b, int x, int y, int w, int h, bool dim) {
            if (!b) return;
            if (dim) {
                ColorMatrix m = { {{0.299f,0.299f,0.299f,0,0},
                                   {0.587f,0.587f,0.587f,0,0},
                                   {0.114f,0.114f,0.114f,0,0},
                                   {0,0,0,kDisabledAlpha,0},
                                   {0,0,0,0,1}} };
                ImageAttributes ia; ia.SetColorMatrix(&m);
                g.DrawImage(b, Rect(x, y, w, h), 0, 0, (INT)b->GetWidth(), (INT)b->GetHeight(),
                            UnitPixel, &ia);
            } else {
                g.DrawImage(b, Rect(x, y, w, h));
            }
        };

        for (size_t i = 0; i < model_->items.size(); ++i) {
            const cm::MenuItem& it = model_->items[i];
            const RECT& r = itemRects_[i]; // client coords
            applyAA(it.box);

            if (it.separator) {
                const cm::BoxStyle& b = it.box;
                RectF rowRect((REAL)margin, (REAL)r.top, (REAL)bodyW_, (REAL)(r.bottom - r.top));
                if (b.hasColor || !b.image.empty() || b.hasGradient) {
                    int rc = b.cornerSet ? (int)(b.cornerRadius * scale_) : 0;
                    DrawBox(g, rowRect, b, rc, itemImages_[i].get());
                }
                REAL thick = (REAL)((b.heightSet ? b.height : 1) * scale_);
                REAL pl = (REAL)pL(it), pr = (REAL)pR(it);
                REAL cy = (REAL)((r.top + r.bottom) / 2);
                RectF bar((REAL)margin + pl, cy - thick / 2, (REAL)bodyW_ - pl - pr, thick);
                cm::Color barCol = it.fontColorSet ? it.fontColor : th.separatorFallback;
                SolidBrush sb(G(barCol));
                g.FillRectangle(&sb, bar);
                continue;
            }

            RectF hr((REAL)r.left, (REAL)r.top,
                     (REAL)(r.right - r.left), (REAL)(r.bottom - r.top));
            int itCorner = it.box.cornerSet ? (int)(it.box.cornerRadius * scale_) : radius_ / 2;
            DrawBox(g, hr, it.box, itCorner, itemImages_[i].get());
            bool hot = ((int)i == hovered_) && !it.disabled && !it.title;
            if (hot) {
                GraphicsPath hp; AddRoundRect(hp, hr, (REAL)itCorner);
                FillHover(g, hr, hp, it.box);
            }

            int padl = pL(it), padr = pR(it);
            int chevW = (!it.submenu.empty() && it.showChevron) ? chevronReserve_ : 0;
            int leftGutter = padl, rightGutter = padr + chevW;
            if (hasIcons_ && icons_[i]) {
                int islot = iconSlot_;
                int iy = r.top + ((r.bottom - r.top) - iconPx_) / 2;
                if (it.iconRight) {
                    int ix = margin + bodyW_ - rightGutter - islot + (islot - iconPx_) / 2;
                    drawImg(icons_[i].get(), ix, iy, iconPx_, iconPx_, it.disabled);
                    rightGutter += islot;
                } else {
                    int ix = margin + padl + (islot - iconPx_) / 2;
                    drawImg(icons_[i].get(), ix, iy, iconPx_, iconPx_, it.disabled);
                    leftGutter = padl + islot;
                }
            } else if (hasIcons_ && !it.iconRight) {
                leftGutter = padl + iconSlot_;  // keep the left column aligned on icon-less rows
            }

            FontFamily rfam(RFontFace(it, th).c_str());
            Font rfont(&rfam, (REAL)(RFontSize(it, th) * scale_ * 96.0 / 72.0),
                       (FontStyle)it.fontStyle, UnitPixel);
            StringFormat rsf;
            int al = RFontAlign(it, th);
            rsf.SetAlignment(al == 1 ? StringAlignmentCenter : al == 2 ? StringAlignmentFar : StringAlignmentNear);
            rsf.SetLineAlignment(StringAlignmentCenter);
            rsf.SetFormatFlags(StringFormatFlagsNoWrap);
            rsf.SetTrimming(StringTrimmingEllipsisCharacter);   // long text -> "..."
            SolidBrush rbrush(G(RTextColor(it, th, hot)));

            RectF tr((REAL)(margin + leftGutter), (REAL)r.top,
                     (REAL)(bodyW_ - leftGutter - rightGutter), (REAL)(r.bottom - r.top));
            std::wstring disp = DisplayText(it);
            g.DrawString(disp.c_str(), -1, &rfont, tr, &rsf, &rbrush);

            if (chevW) {
                if (chevrons_[i]) {   // custom ChevronIcon
                    int cs = (std::min)(chevronReserve_, (int)(r.bottom - r.top)) - (int)(4 * scale_);
                    if (cs < 1) cs = chevronReserve_;
                    int cx = margin + bodyW_ - padr - chevronReserve_ + (chevronReserve_ - cs) / 2;
                    int cy = r.top + ((r.bottom - r.top) - cs) / 2;
                    drawImg(chevrons_[i].get(), cx, cy, cs, cs, it.disabled);
                } else {              // drawn right-pointing triangle
                    REAL cx = (REAL)(margin + bodyW_ - padr - (int)(4 * scale_));
                    REAL cy = (REAL)((r.top + r.bottom) / 2);
                    REAL s  = (REAL)(4 * scale_);
                    PointF tri[3] = { { cx - s, cy - s }, { cx, cy }, { cx - s, cy + s } };
                    SolidBrush chev(G(RTextColor(it, th, hot)));
                    g.FillPolygon(&chev, tri, 3);
                }
            }
        }
    } // surface flushes to `bits`

    Blit(layerAlpha_);
}

// Re-send the (already rendered) layer surface at a whole-window constant alpha.
void PopupWindow::Blit(BYTE alpha) {
    if (!memDC_ || !hwnd_) return;
    HDC screenDC = GetDC(nullptr);
    POINT ptDst{ winX_, winY_ };
    SIZE  size{ winW_, winH_ };
    POINT ptSrc{ 0, 0 };
    BLENDFUNCTION bf{ AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd_, screenDC, &ptDst, &size, memDC_, &ptSrc, 0, &bf, ULW_ALPHA);
    ReleaseDC(nullptr, screenDC);
}

void PopupWindow::FreeLayer() {
    if (memDC_) { SelectObject(memDC_, oldBmp_); DeleteDC(memDC_); memDC_ = nullptr; }
    if (dib_)   { DeleteObject(dib_); dib_ = nullptr; }
}

// ~110ms fade up / ~90ms fade down, re-blitting the same frame each step.
void PopupWindow::FadeIn() {
    const int dur = 110, step = 12;
    for (int t = 0; t <= dur; t += step) Blit((BYTE)(255 * t / dur)), Sleep(step);
    layerAlpha_ = 255; Blit(255);
}
void PopupWindow::FadeOut() {
    const int dur = 90, step = 12;
    for (int t = dur; t >= 0; t -= step) Blit((BYTE)(255 * t / dur)), Sleep(step);
    layerAlpha_ = 0; Blit(0);
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
    margin_ = (int)((bg.shadowSize + (std::max)(std::abs(bg.shadowOffX), std::abs(bg.shadowOffY))) * scale_);
    radius_ = (int)(bg.cornerRadius * scale_);
    emPx_   = (REAL)(th.fontSize * scale_ * 96.0 / 72.0);
    const int itemH = (int)(th.itemHeight * scale_);   // icon-slot sizing basis
    // Per-row height: item uses its Height else ItemHeight; separator uses its
    // ItemHeight else bar-thickness+padding; title follows item rules.
    auto rowH = [&](const cm::MenuItem& it) -> int {
        if (it.separator) {
            if (it.rowHeightSet) return (int)(it.rowHeight * scale_);
            int thick = it.box.heightSet ? it.box.height : 1;
            int pt = it.box.padSet ? it.box.padT : 0, pb = it.box.padSet ? it.box.padB : 0;
            return (int)((thick + pt + pb) * scale_);
        }
        return (int)((it.rowHeightSet ? it.rowHeight : th.itemHeight) * scale_);
    };

    // Resolve + load per-item icons (Rainmeter vars → absolute path). A menu
    // with any loadable icon reserves a square gutter on the left of every row.
    const int iconPad = (int)(5 * scale_);
    iconPx_ = (std::max)(1, itemH - 2 * iconPad);
    icons_.clear();
    hasIcons_ = false;
    bool hasSub = false;
    for (const auto& it : model.items) {
        if (!it.submenu.empty() && it.showChevron) hasSub = true;
        if (it.icon.empty()) { icons_.emplace_back(); continue; }
        std::wstring raw = ExpandSystem(it.icon);
        LPCWSTR v = RmReplaceVariables(rm_, raw.c_str());
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

    // Custom chevron images (only for rows that actually show a chevron).
    const int chevPx = chevronReserve_ > 0 ? chevronReserve_ : (int)(16 * scale_);
    chevrons_.clear();
    for (const auto& it : model.items) {
        if (it.chevronIcon.empty() || it.submenu.empty() || !it.showChevron) {
            chevrons_.emplace_back(); continue;
        }
        std::wstring raw = ExpandSystem(it.chevronIcon);
        LPCWSTR v = RmReplaceVariables(rm_, raw.c_str());
        LPCWSTR abs = RmPathToAbsolute(rm_, v);
        chevrons_.emplace_back(LoadIconBitmap(abs ? abs : (v ? v : L""), chevPx));
    }

    // Reuse the window on reposition (right-click again) so we never destroy the
    // capture-holding window mid-pump — that would fire WM_CAPTURECHANGED and end.
    if (!hwnd_) {
        hwnd_ = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            kClass, L"", WS_POPUP, 0, 0, 0, 0,
            owner_, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!hwnd_) return;
    }

    // Measure text to size the body.
    Bitmap probe(1, 1, PixelFormat32bppPARGB);
    Graphics gm(&probe);
    gm.SetTextRenderingHint(TextRenderingHintAntiAlias);
    FontFamily family(th.fontFace.c_str());
    Font font(&family, emPx_, FontStyleRegular, UnitPixel);

    // Body width = widest row's content (padL + icon slot + text + chevron + padR).
    // Titles measure their resolved text so Width=auto accounts for the header.
    int contentMax = 0;
    bodyH_ = 0;
    for (const auto& it : model.items) {
        bodyH_ += rowH(it);
        if (it.separator) continue;
        int padl = (int)((it.box.padSet ? it.box.padL : 0) * scale_);
        int padr = (int)((it.box.padSet ? it.box.padR : 0) * scale_);
        int chevW = (!it.submenu.empty() && it.showChevron) ? chevronReserve_ : 0;
        RectF box;
        std::wstring disp = DisplayText(it);
        gm.MeasureString(disp.c_str(), -1, &font, PointF(0, 0), &box);
        int need = padl + (hasIcons_ ? iconSlot_ : 0) + (int)(box.Width + 0.5f) + chevW + padr;
        contentMax = (std::max)(contentMax, need);
    }
    const int bgPadX = (int)((bg.padL + bg.padR) * scale_);
    bodyW_ = contentMax + bgPadX;
    if (th.widthFixed) bodyW_ = (int)(th.fixedWidth * scale_);
    int maxW = (int)(th.maxWidth * scale_);
    if (bodyW_ > maxW) bodyW_ = maxW;
    int minW = (int)(8 * scale_);
    if (bodyW_ < minW) bodyW_ = minW;

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
        int h = rowH(it);
        itemRects_.push_back(RECT{ margin_, y, margin_ + bodyW_, y + h });
        y += h;
    }

    // Fade the root popup in (submenus appear instantly to keep cascades snappy).
    bool fade = !asSubmenu && th.animFade;
    layerAlpha_ = fade ? 0 : 255;
    Paint();
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    if (fade) FadeIn();
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
    bool forwardRClick = false; POINT forwardPt{};
    MSG msg;
    while (!done_ && GetMessageW(&msg, nullptr, 0, 0)) {
        switch (msg.message) {
        case WM_MOUSEMOVE: {
            POINT sp{ GET_X_LPARAM(msg.lParam) + winX_, GET_Y_LPARAM(msg.lParam) + winY_ };
            PopupWindow* p = DeepestAt(sp);
            if (p) {
                int ri = p->HitTestScreen(sp);
                if (ri != p->hovered_) { p->hovered_ = ri; p->Paint(); }
                if (ri >= 0 && !p->model_->items[ri].submenu.empty()) {
                    if (ri != p->childParent_) p->OpenChildFor(ri);
                } else if (p->childParent_ >= 0 && ri != p->childParent_) {
                    // Left the parent row (leaf, separator, disabled, gap) => collapse.
                    p->CloseChild();
                }
            } else {
                // Over no popup body. Child bodies sit flush against the parent's
                // edge (no gap), so this is never mid-travel onto a submenu — the
                // cursor has left for the desktop. Collapse the whole submenu tree
                // and drop every stale hover highlight.
                if (child_) CloseChild();
                for (PopupWindow* q = this; q; q = q->child_.get())
                    if (q->hovered_ != -1) { q->hovered_ = -1; q->Paint(); }
            }
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
        case WM_RBUTTONUP: {
            // Right-click again over the owning skin: re-open at the new location.
            // Right-click anywhere else (desktop, another skin): dismiss.
            POINT sp{ GET_X_LPARAM(msg.lParam) + winX_, GET_Y_LPARAM(msg.lParam) + winY_ };
            if (DeepestAt(sp)) break;   // right-click on the menu itself: ignore
            RECT ow;
            if (owner_ && GetWindowRect(owner_, &ow) && PtInRect(&ow, sp)) {
                CloseChild();
                hovered_ = -1;
                Open(*model_, sp, false, 0);
            } else {
                // Dismiss, then replay the right-click so the window under it
                // (another skin, Rainmeter, the desktop) gets its own menu.
                forwardRClick = true; forwardPt = sp; done_ = true;
            }
            break;
        }
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

    // Tear the whole chain down before the caller runs the bang. Collapse
    // submenus first, then fade the root out (if enabled) before it's destroyed.
    CloseChild();
    if (model_ && model_->theme.animFade) FadeOut();
    if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }

    // Replay the outside right-click now that our window/capture are gone, so it
    // lands on whatever is under the cursor.
    if (forwardRClick) {
        SetCursorPos(forwardPt.x, forwardPt.y);
        INPUT in[2] = {};
        in[0].type = INPUT_MOUSE; in[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        in[1].type = INPUT_MOUSE; in[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        SendInput(2, in, sizeof(INPUT));
    }
    return chosen;
}
