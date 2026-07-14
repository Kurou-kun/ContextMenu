# Changelog

All notable changes to this project are documented here.

## [Unreleased]

### Added
- Shared **BoxStyle** for the menu background and every item/title/separator:
  gradient/image fills, stroke, corner radius, shadow (+offset), and padding —
  `BgColor`/`Gradient`/`BgImage`/`StrokeWidth`+`StrokeColor`/`CornerRadius`/
  `Shadow`/`ShadowOffset`/`Padding` work in `[Menu]` and any item section.
- Per-item text + hover styling: `FontColor`/`FontHoverColor`/`FontSize`/
  `FontFace`/`FontAlign`/`FontCase`, and `BgHoverColor`/`GradientHover`.
- `[Separator_X]` sections (name-detected) with `Color`/`Height` bar plus box keys.
- `[Title]` non-interactive header row; `Text` defaults to the skin config path.
- `IconPos=Left|Right`, `SubmenuIco` chevron toggle, per-item `Height`,
  `Width=auto|<px>`, and `%system%` + `file.dll,index` icon extraction.
- Sub-menus now nest to unlimited depth.
- Right-clicking the skin again repositions the open menu; right-clicking
  another skin or the desktop dismisses and replays the click to that target.
- Auto-subclass the skin window and take over its right-click, suppressing
  Rainmeter's default menu.
- Menu-file (Rainmeter INI) driven menu: `[Menu]` theme section plus one section
  per item, in file order. UTF-8 / UTF-16 aware; re-read on every right-click.
- GDI+ layered popup rendering: rounded corners, soft shadow, border, hover
  highlight; DPI- and multi-monitor-aware positioning.
- Per-item icons (`.png` / `.ico` / `.bmp`) with a left icon column.
- Fly-out sub-menus via `Submenu=` groups, with a chevron affordance and
  edge-aware flipping.
- Click resolves the item's bang (variables expanded) and runs it; `Esc` or
  click-away dismisses.
- Sample skin under `example/ContextMenuTest`.
- Repo scaffolding based on the NativeHardwareMonitor plugin template:
  Rainmeter SDK, MSBuild project (v145 / C++17, x64), CI build workflow.

### Changed
- Menu schema v2 — keys renamed: `Color`→`BgColor`, `TextColor`→`FontColor`,
  `Fill`→`Gradient`, `Image`→`BgImage`, `Stroke`→`StrokeWidth`+`StrokeColor`,
  `Bang`→`Action`, `HoverBgColor`→`BgHoverColor`, `HoverTextColor`→`FontHoverColor`.
- `Shadow` now uses `size ; r,g,b,a`. Separators are sections named `Separator*`
  (the `Separator=1` key is gone). `Text=` strips one outer pair of quotes.

### Fixed
- Fly-out submenu now collapses when leaving the parent row without entering it.
- No `Menu=` on the measure falls back to Rainmeter's native context menu
  (with a log warning) instead of swallowing the right-click.

### Removed
- `DisabledTextColor` (disabled rows now dim `FontColor` to alpha 200) and
  `SeparatorColor` (separators carry their own `Color`).
