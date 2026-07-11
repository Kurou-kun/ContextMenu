# ContextMenu

A Rainmeter plugin for building customizable right-click context menus in your skins.

> Status: **scaffolding**. The repo is set up and the plugin builds as an empty
> measure; the menu build/display logic lands in a following release.

## Building

Requires Visual Studio 2022+ with the C++ desktop workload (v145 toolset, C++17).

```
msbuild ContextMenu.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

The output `ContextMenu.dll` is written to `x64\Release\` and, if Rainmeter is
installed, auto-copied to `%APPDATA%\Rainmeter\Plugins\` by the post-build step.

## Usage

```ini
[MeasureMenu]
Measure=Plugin
Plugin=ContextMenu
Menu=...
```

Menu syntax and bangs are documented as the feature is implemented.

## Layout

| Path | What |
|------|------|
| `source/Plugin/` | Rainmeter plugin entry points (`Initialize`/`Reload`/`Update`/`Finalize`) |
| `sdk/rainmeter/` | Official Rainmeter plugin SDK (API headers, import libs, samples) |
| `.github/workflows/` | CI build on `windows-latest` |

## License

GPL-2.0. See [LICENSE](LICENSE).
