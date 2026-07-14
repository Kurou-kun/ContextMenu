# ContextMenu

A Rainmeter plugin that replaces a skin's default right-click menu with a
custom, themed popup defined entirely by one menu file. The background and every
item share a Shape/Image-meter-style **box model** — gradient/image fills,
stroke, corner radius, shadow, and padding — plus per-item icons, styleable
separators, and fly-out sub-menus nested to any depth. DPI-aware and
multi-monitor aware.

## Building

Requires Visual Studio 2022+ with the C++ desktop workload (v145 toolset, C++17).

```
msbuild ContextMenu.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

The output `ContextMenu.dll` is written to `x64\Release\` and, if Rainmeter is
installed, auto-copied to `%APPDATA%\Rainmeter\Plugins\` by the post-build step.

## Usage

Add one measure to your skin. Its only option is `Menu=` — a path to a menu
file. While the skin is loaded, right-clicking it shows your menu instead of
Rainmeter's. The file is re-read on every right-click, so edits show up live.

```ini
[MeasureMenu]
Measure=Plugin
Plugin=ContextMenu
Menu=#@#menu.inc
```

See [`example/ContextMenuTest`](example/ContextMenuTest) for a complete skin.

### Menu file

A menu file is a Rainmeter INI. The `[Menu]` section styles the background box
and holds the global text/layout defaults; every other top-level section is an
item, shown in file order. A section named `[Title]` is a non-interactive
header; a section whose name starts with `Separator` is a separator bar.

```ini
[Menu]
Gradient=Linear 90 ; 34,34,40,255 ; 20,20,26,255
StrokeWidth=1
StrokeColor=80,80,90,255
CornerRadius=8
Shadow=10 ; 0,0,0,110
ShadowOffset=0,3
Padding=4
FontFace=Segoe UI
FontSize=10
FontColor=230,230,230,255
BgHoverColor=70,70,110,255
ItemHeight=28
MaxWidth=220

[Title]
FontAlign=center        ; Text omitted => shows #CURRENTCONFIG#

[Refresh]
Text=Refresh skin
Action=!Refresh
Icon=%system%\imageres.dll,228   ; %system% + DLL,index, or a .png/.ico/.bmp path

[Accent]
Text=Accent item
Action=!About
BgColor=60,60,90,255    ; per-item box fill
FontCase=Upper
CornerRadius=6

[Separator_1]
Color=190,150,255,255   ; bar color
Height=3                ; bar thickness

[Tools]
Text=Tools
Submenu=Tools           ; opens a fly-out built from [Tools\*]
[Tools\Log]
Text=Show log
Action=!About
```

**Item keys:** `Text` (one outer `"…"` pair is stripped), `Action` (any Rainmeter
bang, variables resolved on click), `Icon`, `IconPos=Left|Right`, `Height`
(overrides `ItemHeight`), `Disabled=0|1`, `Submenu=<group>`, `SubmenuIco=0|1`
(right chevron, default on). A child of group `X` is a section named `X\<name>`;
a child may itself declare `Submenu=X\<name>` — sub-menus nest to any depth.

**Box + text keys** — valid in `[Menu]` (defaults) *and* per item/title:

| Key | Meaning |
|-----|---------|
| `BgColor=r,g,b[,a]` | solid fill |
| `BgHoverColor=r,g,b[,a]` | hover fill |
| `Gradient=Linear <angle> ; c1 ; c2 [; …]` | gradient fill; overrides `BgColor` |
| `GradientHover=Linear <angle> ; c1 ; c2 [; …]` | hover gradient; overrides `BgHoverColor` |
| `BgImage=<path>` | image drawn over the fill |
| `FontColor` / `FontHoverColor` | text color, and on hover |
| `FontSize`, `FontFace`, `FontAlign=Left\|Center\|Right` | text |
| `FontCase=Upper\|Lower\|Proper\|None` | text case transform (item/title) |
| `StrokeWidth`, `StrokeColor` | border |
| `CornerRadius=<px>` | corner rounding |
| `Shadow=<size> ; r,g,b[,a]`, `ShadowOffset=<x>,<y>` | drop shadow |
| `Padding=<all>` \| `<x>,<y>` \| `<l>,<t>,<r>,<b>` | inner padding |

**`[Menu]`-only:** `ItemHeight`, `MaxWidth`, `Width=auto|<px>` (fixed width,
clamped to `MaxWidth`).

**`[Separator_X]`:** `Color` (bar), `Height` (bar thickness), `ItemHeight` (row
height), plus box keys for the row behind the bar.

**`[Title]`:** a single header row. `Text` defaults to the skin config path
(`#CURRENTCONFIG#`) when omitted; supports the item icon/height and box/text
keys but is never hoverable or clickable.

Disabled rows render `FontColor` at alpha 200. Colors are `R,G,B` or `R,G,B,A`
(0–255). Unset keys keep their dark-theme defaults.

**Right-click behaviour:** right-clicking the skin again repositions the open
menu; right-clicking another skin or the desktop dismisses it and passes the
click through to that target; left-click-away or `Esc` dismisses. If the measure
has no `Menu=`, the skin keeps Rainmeter's native right-click menu.

## Layout

| Path | What |
|------|------|
| `source/Plugin/` | Rainmeter plugin entry points (`Initialize`/`Reload`/`Update`/`Finalize`) |
| `sdk/rainmeter/` | Official Rainmeter plugin SDK (API headers, import libs, samples) |
| `.github/workflows/` | CI build on `windows-latest` |

## License

GPL-2.0. See [LICENSE](LICENSE).
