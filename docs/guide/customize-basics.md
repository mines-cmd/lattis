# Customize: shared basics

Part of the [Customize reference](customize.md). These settings aren't tied to one part of the window: the main colors, roundness, motion, fonts, spacing, and the controls, menus and popups that look the same everywhere. Most other settings follow these, so start here when you want to change the whole app at once.

## Palette (`[palette]`)

The handful of colors almost every other color follows. Changing one restyles everything that hasn't been given its own color.

- `background`, `background_end`, `gradient_direction`: the color behind everything in the main window, the darkest layer. Set `background_end` to another color to make it a gradient running in `gradient_direction`.
- `surface`, `surface_end`: raised panels that sit on the background: the info panel, stat chips, the search box, toasts, popovers. Change it when panels blend into the background or stand out too much.
- `surface_raised`, `surface_raised_end`: buttons and text fields, one step above panels.
- `surface_hover`, `surface_hover_end`: buttons under the mouse. Keep it a little lighter (or darker) than `surface_raised` so hover is visible.
- `border`: thin outlines and dividers everywhere. Usually a faint, semi-transparent white or black.
- `hover_overlay`: the faint wash laid over a game list row (and many other rows) under the mouse.
- `selected_overlay`: the wash over the selected game list row and other selected items.
- `text`: the main text color.
- `text_muted`: secondary text: labels, hints, descriptions, inactive icons.
- `accent`, `accent_end`: the theme's highlight color: the Launch button, the selected rail icon, focus rings, switches, check boxes, the main button of a dialog. The single most noticeable color in a theme.
- `on_accent`: text and icons drawn on top of the accent (the Launch label, the check mark). Pick something readable against `accent`.

You can add colors of your own here and use them anywhere as `@name`.

## Corners (`[radius]`)

Four tiers every corner in the app follows, plus a pill. Change a tier to re-round everything in it.

- `large`: big panels and cards: the info panel, popovers, the Settings cards, dialogs.
- `medium`: buttons and controls: the Launch button, text buttons, the toolbar buttons.
- `small`: things inside panels: list rows, tags, text fields, thumbnails.
- `tiny`: small details: icons, check boxes, progress bars, menu highlights.
- `pill`: fully round ends, for the toolbar, filter chips and the media viewer's controls. Any value above half the height looks the same.

## Motion (`[motion]`)

How fast and how bouncy things move. Every animation uses one of these.

- `duration_scale`: one knob for every animation's speed. 0 turns animation off, 2 makes everything half as fast, 0.5 twice as fast.
- `fast`: quick feedback (hover, press, color changes, chevrons, switch knobs), in milliseconds.
- `normal`: popups, toasts, list changes, the rail's selection moving, switching category in Settings.
- `slow`: big changes: the banner switching games, page changes, the backdrop fading.
- `easing`: the curve for moves and fades. `smooth` starts fast and settles slowly, `snappy` settles sooner, `gentle` eases in and out, `linear` is even all the way.
- `overshoot`: how far things that pop in go past their size before settling. 0 = no bounce.
- `spring`, `damping`: things that follow the pointer (the hover highlight in the list, dragged items). Higher `spring` follows more tightly; `damping` 1 stops without bouncing, lower values wobble.
- `popup_scale`: the size popups and menus grow from as they open. 1 = they just fade.
- `press_scale`: how far buttons dip while pressed. 1 = no dip.
- `chip_scale`: the size chips, tags and rail buttons grow from when they appear and shrink to when they go.
- `reflow_scale`: the size a Grid view cover grows from when it moves to another row (opening the side panel, zooming).
- `mark_scale`, `mark_overshoot`: check marks and radio dots: the size they grow from and how far they bounce past it.
- `menu_scale`, `menu_offset`: the app menu, the game menu and their submenus: the size they grow from and how far they spring out as they open.
- `view_switch_wait`: when switching between List and Grid view (or closing a page on a game picked there), how long the old view stays while the new one's art loads, so it appears whole. 0 shows it at once.
- `view_switch_rise`, `view_switch_blur`: how far the new view rises into place, and how much the old one blurs as it fades out.

Settings → General → Reduce motion overrides the growing, sliding and bouncing whatever these say.

## Fonts (`[font]`)

- `family`: the typeface for all text. Inter by default; empty uses your system font; any installed family works.
- `mono_family`: the fixed-width font used for file paths.
- `size`: the base text size most text uses. Raising it enlarges almost every label in the app.
- `small_size`: small labels: stat chip labels, list group headers, setting descriptions.
- `heading_size`: headings on a game's page (Description, Tags) and similar.
- `title_size`: dialog and window titles.
- `page_title_size`: the big titles of Library overview, Play statistics and the Gallery.
- `emphasis_weight`: semibold text: headings in dialogs, selected items, names in lists (400 normal, 600 semibold, 700 bold).
- `strong_weight`: the boldest text: page and dialog titles, the game's title.
- `on_accent_weight`: text drawn on the accent: the selected sort order, the selected Settings category, a dialog's main button, the current setup step.
- `label_weight`, `label_letter_spacing`, `label_uppercase`: the small headings above groups of controls (SHOW, COLORS, the stats card headings): their weight, letter spacing, and whether they're in capitals.

## Spacing and outlines (`[layout]`)

- `margin`: the space from the top of the window to the logo and search box.
- `spacing`: the gap under the logo and search row, where the rail, list and banner start; also the default gap inside many panels.
- `spacing_small`: tight gaps between things that belong together: an icon and its label, rows of a list, tags.
- `toolbar_height`: the height of the logo and search box row.
- `border_width`: the thickness of every outline that follows it. Set to 0 for a borderless look.
- `hairline_width`: very thin outlines that stay even in a borderless theme: color swatches, small tags, a selected thumbnail.
- `ring_width`: the ring in the background color around small dots (update, status, badge) that separates them from what's under them.
- `icon_stroke`: the line thickness of the outline icons across the app.
- `gradient_direction`: the direction every gradient uses unless its own `gradient_direction` says otherwise.

## Controls (`[controls]`)

Buttons, fields, switches, sliders, check boxes and scroll bars everywhere: Settings, the game editor, the setup guide and dialogs.

### Buttons

- `button_height`: the height of text buttons (Save, Cancel). Follows `input_height` so buttons line up with fields.
- `footer_button_height`: the smaller buttons in a card's footer row (Close Steam and apply).
- `button_background`, `button_background_end`, `button_hover_background`, `button_hover_background_end`, `gradient_direction`: an ordinary button's fill, at rest and under the mouse, with optional gradients.
- `button_text`: an ordinary button's label.
- `button_primary_background`, `button_primary_background_end`, `button_primary_text`: the main button of a dialog (Create, Save): accent colored by default.
- `button_danger_background`, `button_danger_text`: buttons that remove something (Remove from library), red by default.

### Text fields

- `input_height`: the height of text fields, dropdowns, folder path boxes and the small icon buttons beside them.
- `input_radius`: their corner roundness.
- `icon_size`, `icon_color`: icons inside fields and buttons (the magnifier, the dropdown arrow).
- `input_background`, `input_text`, `placeholder_color`: the field's fill, the text you type, and the grey hint shown while it's empty.
- `input_border`, `input_border_width`: the outline while the field isn't selected (0 = none by default).
- `input_border_focus`, `input_border_focus_width`: the outline while you're typing in it.
- `input_border_invalid`: the outline when what you typed can't be used (a date that can't be read).

### Switches and check boxes

- `switch_width`, `switch_height`: the size of on/off switches.
- `switch_track`, `switch_track_on`: the switch's background when off and on.
- `switch_handle`, `switch_handle_on`: the knob when off and on.
- `checkbox_size`, `checkbox_radius`: the size and roundness of check boxes.
- `checkbox_on`, `checkbox_mark`: a checked box's fill and its tick.

### Sliders

- `slider_track_height`, `slider_handle_size`: the thickness of the track and the size of the round handle.
- `slider_track`, `slider_fill`, `slider_handle`: the empty track, the part left of the handle, and the handle.
- `slider_damping`: how much the handle bounces when it springs to a new value. 1 = no bounce.

### Scroll bars

- `scrollbar_width`: how thick scroll bars are.
- `scrollbar_track`, `scrollbar_handle`, `scrollbar_handle_hover`: the groove, the handle, and the handle under the mouse or while dragging.

### Text and spacing

- `control_spacing`: the gap between controls side by side (a field and its buttons, a dialog's buttons) and between a button's icon and its text.
- `hover`: the background of a settings row under the mouse.
- `label_color`, `label_font_size`: the name of a setting in Settings and dialogs.
- `description_color`, `description_font_size`: the grey explanation under a setting.
- `path_font_size`: the size of folder paths (like Steam's), shown in the fixed-width font.
- `problems_background`, `problems_text`: the Problems panel that lists mistakes in your theme or settings files.

## Menus (`[menu]`)

The app menu, the game menu, right-click menus, dropdown lists and small dialogs (New theme, Add platform).

- `popup_background`: the background of menus, dropdown lists and small dialogs.
- `padding`: the space between a menu's edge and its first and last entry.
- `min_width`: the narrowest a menu gets.
- `item_height`: the height of each entry.
- `item_radius`: the roundness of the highlight on the entry under the mouse.
- `item_padding`: the space on either side of an entry's content.
- `item_spacing`: the gap between an entry's parts (icon, text, tag dot).
- `icons`: whether the app and game menus show icons beside their entries.
- `icon_size`, `icon_color`, `icon_spacing`: those icons' size, color and gap to the text.
- `keys_spacing`, `keys_color`: the shortcut shown at an entry's right (Ctrl+D) and the submenu arrow: the gap before it and its color.
- `danger_text`: entries that remove something (Uninstall), red by default. Many other "danger" colors follow it.
- `row_menu_text`: entries in a setting's ⋯ menu on the Customize page.
- `button_gap`: the gap between a button and the menu dropping from it (the game page's ⋯ button).
- `dialog_padding`: the space inside small dialogs.
- `popup_scrim`: the darkening of the page behind the Customize page's popups. Transparent = no dimming.
- `popup_shadow_color`, `popup_shadow_size`: the shadow under those popups; popovers follow it too.

## Popovers (`[popover]`)

Panels that open from a button: the filter panel, Collections, the stats period picker, tag lists, the calendar, the color picker. Each of those has its own settings that start from these, so change them here to restyle all popovers at once.

- `background`: the popover's fill.
- `radius`: its corner roundness.
- `border`, `border_width`: its outline.
- `shadow_color`, `shadow_size`: the shadow around it; 0 size = none.

## Color picker (`[color_picker]`)

The picker that opens from a color swatch in Customize, on the tag palette and on Settings → Library.

- `background`, `border`, `border_width`, `padding`: the picker's panel. Follows `[popover]`.
- `size`: the shade square's size; `strip_width`: the width of the hue and opacity strips beside it.
- `marker`, `marker_outline`: the ring and bars marking the picked color, and the thin outline that keeps them visible on light colors.
- `theme_columns`, `theme_rows`: how many of your theme's palette colors show under the picker, per row and in rows (0 rows hides them).
- `swatch_radius`, `swatch_border`, `swatch_border_width`: those palette colors' corners and outline.
- `swatch_size`: the size of color swatches in general (the ones beside each color setting, the tag palette).
- `editor_swatch_radius`: the roundness of the swatch beside each color setting in Customize.
- `palette_columns`: colors per row in color lists such as the tag palette.
- `checker_light`, `checker_dark`, `checker_size`: the checkerboard behind see-through colors, so you can tell how transparent they are.

## Tooltips (`[tooltip]`)

The hints that appear when you hover a button.

- `background`: the tooltip's fill.
- `border`, `border_width`: its outline.
- `text`: its text color.
- `delay`: how many milliseconds you hover before it appears.

## Window buttons and edges (`[window_controls]`)

The minimize, maximize and close buttons at the top right of every window, and the edges you drag to resize.

- `buttons`: which buttons show and in what order; empty hides them. Minimize is left out on desktops without it (Hyprland, sway, niri, river).
- `size`, `spacing`, `radius`: each button's size, the gap between them, and their roundness.
- `icon`, `icon_size`, `icon_stroke`: the icons' color, size and line thickness.
- `hover_background`: a button's background under the mouse.
- `close_hover_background`, `close_hover_icon`: the close button under the mouse (red by default).
- `margin`: how far the buttons sit from the main window's top and right edges.
- `background`, `padding`, `background_radius`: an optional panel behind the buttons in the main window, so they stay readable over a bright banner. Transparent by default.
- `resize_border`: how wide the invisible strip along the window's edges is that you drag to resize. 0 turns it off.

## Two-way and three-way toggles (`[segmented]`)

Toggles that pick one of a few choices side by side, like Month | Year on Play statistics or Missing info | All games in Look up missing info.

- `checked_background`, `checked_opacity`: the tint behind the chosen choice and how strong it is.
- `checked_text`, `light_brightness`: the chosen choice's text, and how much lighter it's drawn so it reads on the tint.
- `text`: the other choices' text.
