# Changelog

All notable changes to this project are documented here.

## [Unreleased]

### Added
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
