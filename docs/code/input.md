# Input (`src/input/`)

The `src/input/` folder handles keyboard shortcuts: every action the app can perform is declared here with a name, a default key binding, and a scope. Users can rebind any action in `[shortcuts]` in `settings.toml`; windows and QML code trigger actions by name so the same dispatch path serves keyboard shortcuts, menus, and anything else that may come later (a gamepad, for instance).

## Contents

- [The action schema](#the-action-schema)
- [Loading and dispatch](#loading-and-dispatch)
- [Key bindings](#key-bindings)
- [Conflict detection](#conflict-detection)
- [Files](#files)
- [Reference](#reference)

## The action schema

Every action is one row in `kActions` in `actionschema.cpp`. Each row names a group, an action (`"launch"`, `"toggle_favorite"`, …), default keys in Qt's portable form (`"Ctrl+D"`, space-separated when there are several), and a display title. Groups carry a scope: `Main` actions are handled in the main window, `Settings` actions in the settings window, and `App` actions work in every window. Adding an action means adding a row here and handling its name in the `run()` method of the window that owns it.

Groups serve two purposes: they determine scope, and they organise the Keyboard settings page. The page shows groups as sections with a header, and `sections` on `ActionListModel` gives the section nav.

## Loading and dispatch

`Actions` is a QML singleton that loads `[shortcuts]` from `settings.toml` on startup. The file is watched and reloaded like the theme. Each window creates one `QShortcut` per action in its scope (`names(scope)` gives the list); the shortcut calls `trigger(name)`. `triggered(name, window)` fires with the window that has keyboard focus — because Qt would also match a window's shortcuts while its child window (Settings) has focus, `Actions` tracks `focusWindow` and each shortcut only activates while its window is the focused one.

QML menus and toolbar buttons call `trigger(name)` directly; a gamepad driver would do the same. The action name is the only coupling: windows never share key sequences directly.

## Key bindings

`[shortcuts]` in settings.toml binds action names to keys:

```toml
[shortcuts]
toggle_favorite = "Ctrl+D"
launch = ["Return", "Enter"]   # a list for several keys
toggle_favorite = []            # empty list = unbound
```

`Actions.setKeys(name, keys)` writes the binding into the file (or removes it for an empty list); `resetKeys(name)` removes the entry so the default applies again. `Actions.keys` is a `QQmlPropertyMap` (`ActionKeys`) that exposes each name as a property: `Actions.keys.launch` is a `QStringList` of portable sequences. QML uses this to build tooltips (`Actions.tip(text, Actions.keys.launch)`) and menu labels (`Actions.label(Actions.keys.launch)`).

`sequenceFor(key, modifiers)` converts a raw Qt key event into a portable sequence string — the Keyboard settings page calls it while recording a new binding. Backtab is stored as `Shift+Tab`, since that's what users write.

## Conflict detection

Two actions can conflict when both are in scope at the same time (both `Main`, both `Settings`, one `Main` and one `App`, or one `Settings` and one `App`). `findConflicts()` runs after every reload. Conflicting actions are flagged in `ActionListModel` with the `ConflictRole`. Problems (invalid sequences, conflicts) are reported in `Actions.problems`.

## Files

| File | Purpose |
|---|---|
| `actionschema.h/.cpp` | Every action: group, name, default keys, title, scope |
| `actions.h/.cpp` | Singleton: loading `[shortcuts]`, dispatch, key map, conflict detection; `ActionListModel` and `ActionKeys` |

## Reference

### ActionSchema

```
groups()              → all groups (name, title, scope)
actions()             → all actions (group, name, keys, title)
groupOf(action)       → the group that owns an action
```

`Scope` values: `Main`, `Settings`, `App`.

### Actions (QML singleton)

| Property / method | What it does |
|---|---|
| `keys` | `ActionKeys` — `Actions.keys.<name>` → `QStringList` of portable sequences |
| `model` | `ActionListModel` for the Keyboard settings page |
| `problems` | Validation warnings: unknown action names, duplicate keys, conflicts |
| `recording` | True while the Keyboard page listens for a new binding; shortcuts are off |
| `focusWindow` | The window with keyboard focus |
| `names(scope)` | Action names handled by that scope (`"main"`, `"settings"`) plus app-wide ones |
| `trigger(name)` | Dispatch an action |
| `tip(text, keys)` | `"Add to favorites (Ctrl+D)"` — text with the first key appended |
| `label(keys)` | First key as shown to the user (`"Ctrl+D"`), or `""` when unbound |
| `sequenceFor(key, modifiers)` | Portable sequence from a Qt key event, for recording |
| `setKeys(name, keys)` | Write to `[shortcuts]`; empty list unbinds |
| `resetKeys(name)` | Remove from `[shortcuts]`; default applies again |
| `settingsFile()` | Path to `settings.toml` |

Signal: `triggered(name, window)` — only the focused window's handler runs.

### ActionListModel (via Actions.model)

| Property / method | What it returns |
|---|---|
| `filterText` | Current search text |
| `sections` | `[{name, title}]` for groups with visible rows |
| `firstRow(group)` | Index of the first visible row in the group, or -1 |

Row roles: `NameRole`, `GroupRole`, `GroupTitleRole`, `TitleRole`, `KeysRole`, `KeyLabelsRole`, `DefaultLabelsRole`, `IsCustomRole`, `ConflictRole`, `FirstInGroupRole`, `LastInGroupRole`.
