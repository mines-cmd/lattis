# Theme (`src/theme/`)

The `src/theme/` folder implements the theme system: every visual setting is declared here, loaded from the user's theme files, exposed to QML as `Theme.<section>.<key>`, and editable live from the Settings window. The user-facing description of what themes can do and how to write one is in the user guide's [Themes](../guide/themes.md) page; this doc covers the code side.

## Contents

- [The schema](#the-schema)
- [Loading and resolving](#loading-and-resolving)
- [QML access](#qml-access)
- [Live editing from the Settings window](#live-editing-from-the-settings-window)
- [Migrations](#migrations)
- [ThemeSettingsModel](#themesettingsmodel)
- [Files](#files)
- [Reference](#reference)

## The schema

Every theme setting is one row in `kThemeSettings` in `themeschema.cpp`. Each row carries the section (`palette`, `rail`, `launch_button`, …), the key, the type, the default value, a description, an optional range (integers and reals), allowed values (enums and string lists), and a fallback color (for color references that can't be resolved). There is no other authoritative list: the loader validates files against it, the Colors and Style pages are generated from it, and `reference.toml` is regenerated from it on each start.

Types are `Color`, `Int`, `Real`, `Bool`, `String`, `Enum`, `StringList`, and `ColorList`. Defaults are written as they appear in a theme file (`"#rrggbb"`, `"16"`, `"true"`) but without quotes; list defaults are space-separated. A default may be a reference to another setting (`"@accent"`, `"@radius.medium"`).

## Loading and resolving

`Theme::reload()` runs whenever a watched file changes. A theme is a pair, so it follows two `extends` chains, one for the active colors (`themes/colors/`) and one for the active style (`themes/styles/`), each parents first with cycle detection, into one flat map `m_merged` of `"section.key"` → `Entry` (raw value, origin file and line, file name, which half). `ThemeSchema::isColorSetting` decides which half a setting belongs to (every `Color` and `ColorList` setting, custom palette colors included); a value of the wrong half in a file is reported like an unknown key, with a hint naming the other half. References never cross types, so neither half depends on the other. The chains end on the schema defaults; there is no "default" colors or style.

Built-ins live in `resources/themes/colors/` and `resources/themes/styles/` (the four pair files are in `resources/themes/`); user files shadow them by name in the data dir's `themes/colors/`, `themes/styles/` and `themes/`. A user file with no `extends`, or one naming itself, whose name is a built-in's layers on that built-in. `settings.toml`'s `[appearance] colors` and `style` name the active ones; a missing file falls back to Ember or Soft with a problem and leaves `settings.toml` alone. The active theme isn't stored: `activeTheme` is the theme whose pair names exactly the active colors and style, or empty.

After merging, `valueOf()` resolves each setting: it calls `convert()`, which turns the raw string into the typed QML value (QColor for colors, int, double, bool, QString, QVariantList). Colors may arrive as `#rrggbbaa` (not Qt's `#aarrggbb`), so `parseHex()` handles them explicitly. If `convert()` fails, it tries `<key>_fallback` in the same section, then the schema default.

References (`@accent`, `@radius.medium`) are resolved recursively. `m_resolving` detects cycles and breaks them by falling back.

A syntax error in any file keeps the current theme in place and reports a problem. Unknown sections and keys are reported with a suggestion (from `src/core/suggestion.*`) for likely typos, including mistyped palette color names.

## QML access

`Theme` is a `QQmlPropertyMap` singleton. Each section is a `ThemeSection`, also a `QQmlPropertyMap`, stored as a property of Theme under the camelCase section name. QML reads `Theme.launchButton.fontSize` — `camel()` converts `launch_button` → `launchButton` and `font_size` → `fontSize`. Custom palette colors added by the user are also exposed this way.

`Theme.colorsSchema` and `Theme.styleSchema` are two `ThemeSettingsModel`s, one per half, and `Theme.customizeSchema` a third over both — the Customize page reads its list of settings from the last rather than being written by hand per setting.

`Theme.availableColors`, `availableStyles` and `availableThemes` list the built-ins first, then the user's files; `builtInColors`, `builtInStyles` and `builtInThemes` are the constant lists of built-in names. `setColors`, `setStyle` and `setTheme` write `[appearance]` in `settings.toml` and reload; a theme applies both halves.

`Theme.preview(colors, style)` reads a pair's `[palette]` colors, the rail and launch button backgrounds (also running and install: `launchRunning`, `launchInstall`) and the four radius tiers (`radius`), and the style's look for a mini scene (`railBorderWidth`, `searchRadius`, `searchBorderWidth`, `launchRadius`, `launchBorderWidth`, `launchLabel`, `railContainer`, `railContainerRadius`, `railButtonRadius`), without changing the active theme. A name that isn't found falls back like the loader does.

A style file may have a top-level `description = "…"` above its sections. It is not a setting: the loader skips it in a style file (a colors file reports it like any key outside a section), and `description` is not inherited through `extends`. The Appearance page's and the setup guide's theme cards use this to show a swatch without switching; `Theme.themePair(name)` gives a theme's `{colors, style}` for it.

`Theme.createColors(name, basedOn)` and `Theme.createStyle(name, basedOn, description)` write a file that extends `basedOn` (the one in use when empty) and make it the active one; a style may carry a `description`. `Theme.saveTheme(name, replace)` writes `themes/<name>.toml` naming the active colors and style, and replaces an existing theme only when `replace` is set, so the page asks first (`Theme.themeExists`). `Theme.nameError(kind, name)` (`kind` is `"colors"`, `"style"` or `"theme"`) is empty when the name can be used and says why not otherwise: empty, a slash, `reference`, a built-in, or (for colors and styles) taken.

## Live editing from the Settings window

`Theme.setValue(section, key, value)` applies a change immediately and schedules a save after 400 ms. The debounce keeps the save timer from churning the disk while a slider is dragged. Until the save is written, the override is kept in `m_overrides` and layered on top of any hot-reload. This way, if the file watcher fires because of the launcher's own save — or a concurrent hand edit — the override isn't lost.

`Theme.resetValue(section, key)` removes the key from the active theme's file (and from `m_overrides`), so the inherited or default value takes over again.

An edit goes to the file of the setting's half: colors for every `Color` and `ColorList` setting (and custom palette colors), the style for the rest. Each half's file is the active colors' or style's own file in `themes/colors/` or `themes/styles/`. When the active one is a built-in, the first edit creates a file of the same name, which layers on the built-in (see below); the other half's file isn't touched. `Theme.activeFile()` and `sourceLabel()` name the file of the right half, for the source label and the reset tooltips. The edit session (`beginEditSession`, `revertValue`, `revertAll`) is keyed by setting, so one session covers both halves; switching colors, the style or the theme clears it.

`Theme.colorsFilePath()` and `Theme.styleFilePath()` return the file edits to that half go to, for "Open colors file" and "Open style file". When it doesn't exist yet and the active one is a built-in, they first write it with a commented header (what it layers on, where `reference.toml` is); otherwise they create it empty.

`Theme.canResetToBuiltIn(half)` (`"colors"` or `"style"`) is true while a user file sits on the built-in of the same name, and `Theme.resetToBuiltIn(half)` deletes it, drops that half's unsaved edits and session history, and reloads, so the built-in's values come back. The page asks for confirmation first.

`Theme.deleteColors(name)`, `deleteStyle(name)` and `deleteTheme(name)` move the user's own file to the trash and return whether they did; each refuses a built-in name, `reference`, a name with a slash and a file that isn't there. The trash is the second constructor argument of `Theme` (`Uninstall::systemTrash` by default, so tests pass their own); without one, `needsPermanent()` is true after a failed call, and the same call with `permanent = true` deletes the file for good (the dialog asks before that). Deleting the active colors or style switches to Ember or Soft after the file is gone, dropping that half's unsaved edits, so a failed trash leaves `settings.toml` alone. Themes and files that named what was deleted stay as they are and fall back with a problem, like any missing file; deleting a theme never touches its colors or style. `Theme.renameColors(name, newName)`, `renameStyle(…)` and `renameTheme(…)` rename the user's own file, refusing what delete refuses plus a name `nameError` rejects or a file that already exists. Renaming colors or a style rewrites every name that pointed at it: the `extends` of the user's files of that kind, the `colors` or `style` of theme files, and `[appearance]` in `settings.toml`; pending edits are flushed to the file first, then the theme reloads. `Theme.usedBy(kind, name)` (`kind` is `"colors"` or `"style"`) returns `{themes, files}`: the themes whose pair names it and the user's files of that kind whose `extends` names it, which the delete dialog lists.

`Theme.rawValue()`, `source()`, `isCustomized()`, and `resolvedValue()` let the Appearance page show where a value comes from and whether a reset button applies.

## Migrations

Two small migrations of the old single-file themes still run on start, before reloading: `dropMaterialTheme()` (themes that extended the removed `material` built-in now extend `"default"`) and `migrateRadiusTiers()` (`layout.radius` / `layout.radius_small` became `radius.medium` / `radius.small`). They only touch `themes/*.toml` files written before the split, which the new layout no longer reads as themes; old files aren't converted into colors and styles.

## ThemeSettingsModel

`ThemeSettingsModel` exposes the `ThemeSchema::Setting`s of one half as a filterable list model for the Customize page and the setup guide: the colors model lists the color settings and the style model the rest, so a section with both kinds (`hero`, `tags`, `palette`) is in both lists with its own rows. `changedCount` counts the half's own settings. Each row carries the setting's type, description, range, allowed values, default text, resolved value, raw value as written, source label (`"default"`, `"Mythos.toml"`, `"inherited from custom"`), and whether it's set in its half's active file (the `IsCustom` role, which controls the reset button). The `changedOnly` filter hides everything not set in the active theme — used by the "Changed settings" view.

`Theme.customizeSchema` is the same model in its third mode, both halves: each section is one run with its colors first, then its style settings, and `half` (`"colors"` or `"style"`) is a role for it. Its `show` property (`"all"`, `"colors"`, `"style"`) trims the rows, and `sections` follows it, so a section with no row of the shown half is gone from the links too. The half models ignore `show`. `changedCount` counts both halves there.

The model refreshes on every theme change and on every `setValue()` call. When `changedOnly` is on, a reset or theme switch can add or remove rows. To avoid rebuilding every delegate (which would close an open color picker mid-drag), the model diffs the visible rows and only resets when they actually change.

`sections` returns the sections with at least one visible row, in schema order, each with a `shared` flag (the `Section::shared` flag in `themeschema.cpp`: Palette, Radius, Motion, Font, Layout, Controls and Menu, listed first). The Customize page uses this for its section navigation and the search highlights matching sections' headers.

Links: a setting whose value (the theme's, else the schema default) is `@section.key` or `@name` (a palette color) has that target, normalised to `section.key`, in the `LinkTargetRole` (empty when it isn't a link). `FollowersRole` counts the settings that link to a setting directly; a chain counts once per step. `OwnValueRole` is true when the schema default is a link and the theme sets a plain value. All three are computed on first use after each theme change, not at start.

Groups: `ThemeSchema::groups()` is a table (name, title, rule on the key name) in `themeschema.cpp`, with a short exceptions list (`kGroupExceptions`) for keys a rule gets wrong; `ThemeSchema::inGroup()` applies both. The model's `group` filters by one (`"everything"` by default), and `following` (a `section.key`, empty by default) lists the settings that link directly to that target. A group and `following` are one slot: setting one clears the other. Both combine with `show`, `changedOnly`, `sharedOnly` and the search. `groups` lists `{name, title, count}`, Everything first; the counts apply every other filter (including `sharedOnly`), so they show what picking that group would list.

The tag color palette (`tags.palette`) has its own editor in Settings → Library and is excluded from this model.

## Files

| File | Purpose |
|---|---|
| `themeschema.h/.cpp` | Every setting: section, key, type, default, range, description |
| `theme.h/.cpp` | Theme loading, `extends`, validation, references, hot reload, live editing |
| `schemamodel.h/.cpp` | Theme settings as a filterable list model: one half, or both for the Customize page |

## Reference

### ThemeSchema

```
sections()              → all sections (name, doc, shared)
groups()                → the Customize groups (name, title, rule)
inGroup(group, setting) → the rule and the exceptions list
headings()              → sub-headings (section, title, index of the first setting in settings())
settings()              → all settings (section, key, type, def, doc, min, max, choices, fallback)
typeName(type)          → "Color", "Int", …
```

### Theme (QML singleton)

| Property / method | What it returns |
|---|---|
| `activeColors`, `activeStyle` | Names in `[appearance]` (the file loaded may be the fallback) |
| `activeTheme` | The theme naming exactly that pair; empty if none |
| `availableColors`, `availableStyles`, `availableThemes` | Built-ins, then the user's files |
| `builtInColors`, `builtInStyles`, `builtInThemes` | Built-in names (constant) |
| `problems` | Validation warnings from the last load |
| `colorsSchema`, `styleSchema` | `ThemeSettingsModel` of each half's settings |
| `customizeSchema` | `ThemeSettingsModel` of both halves, for the Customize page |
| `preview(colors, style)` | `{palette colors…, rail, launch, launchRunning, launchInstall, radius, railBorderWidth, searchRadius, launchLabel, railContainer…, extends}` — no switch |
| `themePair(name)` | `{colors, style}` of a theme; empty if there is none |
| `setValue(section, key, value)` | Apply immediately; save after 400 ms debounce |
| `resetValue(section, key)` | Remove from active file; revert to inherited/default |
| `rowMenu(section, key)` | What a setting row's menu needs, read when it opens: `revert` (the session's value, if changed), `file` and `fallback` (the active file and what removing the value falls back to: the parent theme's value, else the default; only when the active file sets it), `fromParent`, `default` (`target, value` for a link), `defaultTarget` and `defaultValue`, `current` |
| `useDefault(section, key)` | Write the schema default (a plain value or its `@` link) to the active file |
| `useOwnValue(section, key)` | Write the value now in effect to the active file, which unlinks a setting that follows another |
| `colorsFilePath()`, `styleFilePath()` | The file edits to that half go to; created (with a header on a built-in) when missing |
| `canResetToBuiltIn(half)` | A user file layers on the active built-in colors (`"colors"`) or style (`"style"`) |
| `resetToBuiltIn(half)` | Delete that file; the built-in's values return |
| `usedBy(kind, name)` | `{themes, files}` that name or extend that colors or style |
| `renameColors(name, newName)`, `renameStyle(…)`, `renameTheme(…)` | Rename the user's own file and repoint what named it; false when refused |
| `deleteColors(name, permanent)`, `deleteStyle(…)`, `deleteTheme(…)` | Trash the user's own file; false when refused or the trash failed (`needsPermanent()`) |
| `rawValue(section, key)` | As written in the file chain; invalid QVariant if none |
| `sourceLabel(section, key)` | `"default"`, `"Ember.toml (colors)"` or `"inherited from Mythos"` |
| `activeFile(section, key)` | `"Ember.toml"`: the file that holds that setting's edits |
| `isCustomized(section, key)` | Set in the active theme's own file |
| `resolvedValue(section, key)` | Same as `Theme.<section>.<key>` but by name |
| `setColors(name)`, `setStyle(name)`, `setTheme(name)` | Switch; a theme applies its pair |
| `createColors(name, basedOn)`, `createStyle(name, basedOn, description)` | Create the file extending `basedOn`; switch to it |
| `styleDescription(style)` | The style's `description` (its own file's, or the built-in's it layers on), "" if none; capped at 48 characters, longer ones elided with "…" |
| `styleSummary(style)` | The line for a style without a description: `Radius 12 · 8 · 6 · 4`, then `no borders` (`layout.border_width` 0), the launch label (if changed), `no rail panel`, then `+N other changes` (keys set along the `extends` chain that differ from the schema default) |
| `setStyleDescription(name, text)` | Sets, or with empty text removes, a user style's description; false for a built-in or a missing file |
| `nameError(kind, name)` | Empty if the name can be used for a new `"colors"`, `"style"` or `"theme"`, else why not |
| `themeExists(name)`, `saveTheme(name, replace)` | Name the active pair as a theme; replacing needs `replace` |

Signals: `activeColorsChanged`, `activeStyleChanged`, `activeThemeChanged`, `availableChanged`, `problemsChanged`, `valueChanged(section, key)`, `reloaded`.

### ThemeSettingsModel (via Theme.colorsSchema and Theme.styleSchema)

| Property / method | What it returns |
|---|---|
| `filterText` | Current search text: matched against the section and key (also as `section.key`), the description and, for a color, its value |
| `changedOnly` | When true, only settings the active theme sets |
| `show` | `"all"`, `"colors"` or `"style"`; only `customizeSchema` narrows by it |
| `sharedOnly` | When true, only the settings of shared sections |
| `group` | A group name from `groups`, `"everything"` for none; replaces `following` |
| `following` | `section.key`: only the settings that link directly to it; replaces `group` |
| `followingTitle` | `following` as "Section → Setting" for the banner's chip, empty when not following |
| `groups` | `[{name, title, count}]`, Everything first |
| `changedCount` | Count of settings in the active theme's own file |
| `sections` | `[{name, title, shared, subsections: [{title, count}]}]` for sections with visible rows; a sub-heading with no visible row is left out |
| `firstRow(section)` | Index of the first visible row in the section, or -1 |
| `firstRow(section, subsection)` | Index of the first visible row under that sub-heading, or -1 |

Row roles: `SectionRole`, `SectionDocRole`, `SectionTitleRole`, `KeyRole`, `TitleRole`, `CamelKeyRole`, `TypeRole`, `DocRole`, `MinRole`, `MaxRole`, `ChoicesRole`, `DefaultRole`, `ValueRole`, `RawRole`, `SourceLabelRole`, `IsCustomRole`, `FirstInSectionRole`, `LastInSectionRole`, `SubsectionRole` (the row's sub-heading, empty in a section without them), `SubsectionCountRole` (visible rows in its card), `FirstInCardRole`, `LastInCardRole`, `LinkTargetRole` (`linkTarget`), `FollowersRole` (`followers`), `OwnValueRole` (`ownValue`). In the both-halves model a sub-heading's colors come before its style settings.
