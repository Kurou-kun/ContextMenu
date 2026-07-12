# ContextMenu

A Rainmeter plugin that replaces a skin's default right-click menu with a
custom, themed popup defined entirely by one menu file. Rounded corners, soft
shadow, hover highlight, per-item icons (`.png`/`.ico`/`.bmp`), and fly-out
sub-menus. DPI-aware and multi-monitor aware.

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

A menu file is a Rainmeter INI. The `[Menu]` section is the theme; every other
top-level section is an item, shown in file order.

```ini
[Menu]
BgColor=30,30,30,255
HoverBgColor=60,60,90,255
FontFace=Segoe UI
FontSize=10
ItemHeight=28
CornerRadius=8

[Refresh]
Text=Refresh skin
Bang=!Refresh
Icon=#@#refresh.png     ; optional, .png/.ico/.bmp

[Sep1]
Separator=1

[Tools]
Text=Tools
Submenu=Tools           ; opens a fly-out built from [Tools\*]
[Tools\Log]
Text=Show log
Bang=!About
```

**Item keys:** `Text`, `Bang` (any Rainmeter bang, variables resolved on click),
`Icon`, `Separator=1`, `Disabled=1`, `Submenu=<group>`. A child of group `X`
is a section named `X\<name>`; a child may itself declare `Submenu=X\<name>`.

**Theme keys (`[Menu]`):** `BgColor`, `TextColor`, `HoverBgColor`,
`HoverTextColor`, `DisabledTextColor`, `SeparatorColor`, `BorderColor`,
`ShadowColor`, `BorderWidth`, `FontFace`, `FontSize`, `ItemHeight`, `Padding`,
`CornerRadius`, `ShadowBlur`, `MaxWidth`. Colors are `R,G,B` or `R,G,B,A`
(0–255). Unset keys keep their dark-theme defaults.

## Layout

| Path | What |
|------|------|
| `source/Plugin/` | Rainmeter plugin entry points (`Initialize`/`Reload`/`Update`/`Finalize`) |
| `sdk/rainmeter/` | Official Rainmeter plugin SDK (API headers, import libs, samples) |
| `.github/workflows/` | CI build on `windows-latest` |

## License

GPL-2.0. See [LICENSE](LICENSE).
