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
and holds the text theme; every other top-level section is an item, shown in
file order.

```ini
[Menu]
Fill=LinearGradient 90 ; 34,34,40,255 ; 20,20,26,255
Stroke=1,80,80,90,255
CornerRadius=8
Shadow=10,0,0,0,110
ShadowOffset=0,3
Padding=4
FontFace=Segoe UI
FontSize=10
HoverBgColor=70,70,110,255
ItemHeight=28

[Refresh]
Text=Refresh skin
Bang=!Refresh
Icon=#@#refresh.png     ; optional left glyph, .png/.ico/.bmp

[Accent]
Text=Accent item
Bang=!About
Color=60,60,90,255      ; per-item box fill
CornerRadius=6

[Sep1]
Separator=1
Height=3                ; styleable separator bar
Color=190,150,255,255

[Tools]
Text=Tools
Submenu=Tools           ; opens a fly-out built from [Tools\*]
[Tools\Log]
Text=Show log
Bang=!About
```

**Item content keys:** `Text`, `Bang` (any Rainmeter bang, variables resolved on
click), `Icon` (small left glyph), `Separator=1`, `Disabled=1`,
`Submenu=<group>`. A child of group `X` is a section named `X\<name>`; a child
may itself declare `Submenu=X\<name>` — sub-menus nest to any depth.

**Box-style keys** — valid in `[Menu]` (the background box) *and* in any item or
separator section:

| Key | Meaning |
|-----|---------|
| `Color=r,g,b[,a]` | solid fill (alpha < 255 = translucent) |
| `Fill=LinearGradient <angle> ; c1 ; c2 [; …]` | linear gradient fill, evenly-spaced stops; overrides `Color` |
| `Image=<path>` | image drawn over the fill, clipped to the rounded rect |
| `Stroke=<w>,r,g,b[,a]` | border |
| `CornerRadius=<px>` | corner rounding |
| `Shadow=<size>,r,g,b[,a]` | soft drop shadow |
| `ShadowOffset=<x>,<y>` | shadow direction |
| `Padding=<all>` \| `<x>,<y>` \| `<l>,<t>,<r>,<b>` | inner padding |
| `Height=<px>` | separator bar thickness (separators only) |

**Text-theme keys (`[Menu]` only):** `FontFace`, `FontSize`, `TextColor`,
`HoverBgColor`, `HoverTextColor`, `DisabledTextColor`, `SeparatorColor`,
`ItemHeight`, `MaxWidth`. Colors are `R,G,B` or `R,G,B,A` (0–255). Unset keys
keep their dark-theme defaults.

**Right-click behaviour:** right-clicking the skin again repositions the open
menu; right-clicking another skin or the desktop dismisses it and passes the
click through to that target; left-click-away or `Esc` dismisses.

## Layout

| Path | What |
|------|------|
| `source/Plugin/` | Rainmeter plugin entry points (`Initialize`/`Reload`/`Update`/`Finalize`) |
| `sdk/rainmeter/` | Official Rainmeter plugin SDK (API headers, import libs, samples) |
| `.github/workflows/` | CI build on `windows-latest` |

## License

GPL-2.0. See [LICENSE](LICENSE).
