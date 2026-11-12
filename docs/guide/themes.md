# Themes

Almost everything you see in Lattis comes from its look: colors, fonts, sizes, corner roundness, and how the game page is laid out. Changes apply the moment you make them.

The look has two halves you can pick separately:

- **Colors** are every color setting.
- **A style** is everything else: corner roundness, borders, the font, sizes, labels and layout.

Any colors go with any style. A **theme** is just a name for a pair of the two, so you can switch to the pair in one click.

## Picking colors, a style and a theme

Pick on Settings → Appearance, in the app menu's **Theme** submenu (a theme sets both; colors and style on their own are in Settings), or in the setup guide (which picks colors and a style). Lattis comes with these colors:

- **Ember:** dark blue-grey slate, with an ember-orange accent from the logo. The colors a new library starts with.
- **Moss:** Ember's dark slate with a moss-green accent.
- **Mythos:** near-black, with one blue accent. Inspired by the Mythos theme for Playnite by bansakai.
- **Steam:** styled like the Steam client.
- **Catppuccin:** mauve accents on a deep blue-purple, from the Catppuccin Mocha palette.
- **Gruvbox:** warm browns with a yellow accent, from the Gruvbox palette.
- **Rose Pine:** deep indigo with a pink accent, from the Rosé Pine palette.
- **Nord:** arctic blue-greys with a blue accent, from the Nord palette.

and these styles, named by how round they are:

- **Sharp:** square corners, except tags, chips and game icons, which keep a little.
- **Soft:** gently rounded. The style a new library starts with.
- **Round:** rounder corners.
- **Steam:** Steam's look: nearly square, no borders, "▶  PLAY" on the button.

The built-in themes are Ember, Moss, Mythos, Catppuccin, Gruvbox, Rose Pine and Nord (each with Soft) and Steam (with the Steam style).

Settings → Appearance shows a card for each theme, with your own after the built-in ones, and a picker each for the colors and the style in use. The card of the pair in use is marked; if the pair matches no theme, none is. **Save as theme…** names the pair you've picked (asking before it replaces a theme of the same name). Hover one of your own themes and click the × (or right-click it and choose **Delete**) to move its file to the trash; the colors and style it named stay. Right-click and choose **Rename** to give it another name.

## Editing a built-in

You can change any built-in. Your first change to Ember writes `themes/colors/Ember.toml` in your [data folder](data-folder.md); it holds only what you changed and sits on top of the built-in Ember. Delete it, or use **Reset to built-in** in the picker's menu, and Ember is back as it came. A built-in theme includes your changes to its colors and style. Your changes to one built-in don't reach another built on it (Sharp and Round are built on Soft), but colors or styles of your own based on it do follow them. To keep the original beside your version, make a copy with **New colors…** or **New style…**.

## Changing a theme in Settings

Settings → Customize lists every setting the active colors or style can change, grouped by part of the window (the rail, the game list, the hero, the Launch button…). In each part the colors come first, then the style settings. Long parts are split into named groups (the Settings window's Sidebar, Settings list and Theme popup, the game editor's Fields and Art, and so on), each in its own box with the number of settings; the sidebar lists a part's groups under its link while you are in it. Button, text field, switch, slider, check box and scroll bar colors are under **Controls**, and the colors and sizes of menus and popups under **Menu**; they apply everywhere in the app, not only in Settings. The theme button in the toolbar picks the colors and the style, and has New, Open file, Reset to built-in, Rename and Delete for each. **Show** narrows the list to colors only or style only, to a **Group** (Corners, Borders, Text sizes, Spacing, Shadows or Gradients, to see every setting of that kind across the parts), to **Shared values only** (the sections other parts follow, such as Palette and Radius) or to changed settings only; they combine, and the search narrows further. While a filter is on, a bar above the list names each one with an × to remove it, shows how many settings match and has **Clear all**. **This session** lists what you changed since opening Settings, with Undo. Search finds a setting by name, by its name in a theme file (`icon_button.badge`), or by a color code (`#1a8cff`) to see everything using that color. Settings you've changed are marked "Changed".  A violet "→ Small" tag beside a setting means it follows another setting, so changing that one changes this too; a green "13 follow" chip means that many settings follow it, and clicking it lists them; an orange "own value" tag marks a setting that normally follows a shared value but has its own in your theme.

Click a color's swatch to open the picker: the square sets the shade, the strip beside it the hue and the checkered strip the opacity. Below them are your theme's palette colors (your own `[palette]` colors included); clicking one makes the setting follow that color (`@accent`), so it changes when the palette does. Hover one for its name. You can also type a color code or `@name` in the box next to the swatch.

Each setting has a **⋯** button at its right end; right-clicking the setting opens the same menu. It lists only what applies: **Remove from** your theme's file (the value goes back to what the theme extends, or the default; a message offers **Undo**), **Use default**, **Use own value** to stop following another setting, **Follow … again**, **Go to** the setting it follows (the violet tag does the same), and **Show N that follow this**. Each item says on the right what it will do.

### Undoing changes

- **This session** in the toolbar lists every setting you've changed since you opened Settings, with its value then and now. **Undo** puts one back; **Undo all** puts them all back. Click a setting's name to go to it.
- If your theme already set a value when you opened Settings, and you've since changed or reset it, **Revert** in the setting's **⋯** menu puts that value back.

Closing and reopening Settings starts a new session, and so does switching the colors or the style.

The button at the right of the toolbar shows the colors and style in use and opens a panel with a column for each. A column lists the names with a preview: a colors row shows four dots (background, accent, and the Launch button when running and when it needs installing), and a style row shows a line of text and a small scene drawn with that style's own corners, borders, rail and Launch label. The line is the style's description, or when it has none, its corner radii and what else differs from the defaults ("no borders", the Launch label, "no rail panel", "+N other changes"). Click a name to switch to it; the one in use is highlighted. The `⋯` button at the top of a column holds Reset to built-in, Rename, Edit description (styles of your own) and Delete, where they apply, and New and Open file sit at the bottom. The panel's size, spacing and colors are settings under Settings window. In detail:

- **New colors…** (Ctrl+Shift+N) or **New style…** (Ctrl+Alt+N), which make your own based on any other. The name can't be empty, contain `/` or `\`, be `reference`, or be one already taken, and a built-in's name is refused: edit the built-in instead. A new style can also have a short description (up to 48 characters); it is the `description = "…"` line at the top of the style's file.
- **Open colors file** (Ctrl+E) or **Open style file** (Ctrl+Shift+E): see below.
- **Reset to built-in**, once you've changed a built-in: it deletes your changes to it, after asking.
- **Rename…**, for colors or a style of your own. The file takes the new name, and everything that named the old one follows: the themes that use it, your colors or styles that extend it, and the picker if it was in use.
- **Delete**, for colors or a style of your own. It names the file and the themes that use it, then moves the file to the trash (asking again before deleting for good when there is no trash). Those themes stay, falling back to Ember or Soft until you pick others. If it was in use, Ember or Soft takes its place.

If your settings name colors or a style that no longer exists, Lattis uses Ember or Soft for that half and lists the missing file under the Problems button. Your settings aren't changed, so putting the file back restores it.

## Editing the files

**Open colors file** and **Open style file** open the one in use in your text editor. On a built-in you haven't changed yet, they first write the file with a short comment on what it layers on. Saved changes apply right away, so you can keep Lattis open beside the editor. Each file only needs what it changes from the one it extends:

```toml
# themes/colors/Night.toml
extends = "Mythos"

[palette]
accent = "#ff3399"
```

```toml
# themes/styles/Boxy.toml
extends = "Soft"

[launch_button]
label = "Play"

[radius]
large = 20
medium = 14
```

- **Colors files** may only hold color settings and **style files** only the rest. A value in the wrong one is reported, with a hint saying where it belongs.
- **`[palette]`** holds the base colors (`background`, `surface`, `text`, `accent`…). Everything else uses them by default, so changing the palette restyles the whole app. You can add colors of your own to the palette, and use them elsewhere as `"@my_color"`.
- **Any setting can refer to another** of the same kind, as `"@section.key"`, like `"@radius.medium"`.
- **`[font]`** has `family` (Inter by default, bundled with Lattis; empty uses your system's font, or name any installed family) and `on_accent_weight` (100–900, 600 by default), the weight of text drawn on the accent: the selected sort order, the selected Settings category, a dialog's main button and the current setup step.
- **`[radius]`** sets how round everything is, in four sizes: `large` (panels and cards), `medium` (buttons), `small` (rows, tags, inputs) and `tiny` (icons, check boxes), plus `pill` (fully round ends: the toolbar, filter chips; the built-in styles leave it alone). Thin outlines follow `layout.hairline_width` and the rings around the update, status and badge dots `layout.ring_width`; a borderless style sets `layout.border_width` to 0 and keeps both.
- **`[motion]`** sets how things move: `duration_scale` (0 turns animation off, 2 is half as fast), the three speeds `fast`, `normal` and `slow` in milliseconds, the `easing` curve (`smooth`, `snappy`, `gentle`, `linear`), and `overshoot`, `spring` and `damping` for things that bounce, `popup_scale`, `press_scale` and `chip_scale` for how far popups and chips grow from and buttons dip to. `mark_scale` and `mark_overshoot` are the size check marks and radio dots grow from and how far they bounce past it. `menu_scale` and `menu_offset` shape how the rail logo's menu, a game's right-click menu and their submenus open: the size they grow from and how far they spring out. `[settings_window] page_offset` is how far settings pages slide out and in when you switch categories (0 = fade only),, `[list] hover_spring` and `hover_damping` how closely the game list's hover highlight follows the pointer and how much it bounces, and `[stats] page_offset` how far the Play statistics page rises as it opens and sinks as it closes. `[gallery] row_stagger` is the delay in milliseconds between one row and the next fading in the first time the Gallery opens (0 = all at once), and `[gallery] placeholder_color` is the block each Gallery tile shows until its picture arrives. Settings → General → Reduce motion stops things from growing, sliding or bouncing whatever the style says, while fades and color changes stay; spinners and progress indicators keep moving, since they show something is busy.
- **`[collections]`** styles the Collections popup and the banner above the list while a collection is open. Its colors and sizes follow the filter panel's sort buttons and the list's selection bar until you set a value there; `popup_width` is the popup's width and `list_max_height` how tall its list grows before it scrolls. `edit_width` and `edit_list_height` size the Edit collection dialog; `edit_label_width` is the column of rule names, `edit_section_gap` the space between its Library, Progress and Details groups, `edit_section_heading`, `edit_section_heading_opacity` and `edit_section_rule` the group headings and the line beside them, `edit_year_width` the release year fields, `edit_footer_background` and `edit_footer_text` its footer, and `edit_tab_text`, `edit_tab_active_text`, `edit_tab_count_opacity`, `edit_tab_indicator` and `edit_tab_indicator_height` its tabs (the chips are the filter panel's toggles, so `[filters]` styles them), and `import_list_height` the lists in the Import from Steam dialog; `checklist_row_height`, `checklist_row_padding` and `checklist_padding` space the rows of the lists of checks in both dialogs and the box around them.
- **`[grid]`** styles the Grid view. The wash over the cover under the pointer is `hover_overlay` (with `hover_overlay_end` and `hover_gradient_direction` for a gradient); `play_button` (`off`, `center` or `corner`; `corner` by default) puts a round launch button on it (`hover_zoom` is how far the cover's picture grows under the pointer, 1 = off; `play_button_size`, `play_button_margin` (its space from the right and bottom edges in the corner), `play_button_background`, `play_button_background_end`, `play_button_icon_color` (also the label's), `play_button_label` (the text beside the icon, empty by default, so the icon alone) and `play_button_font_size`) and `platform_icon` (`off`, `hover` or `always`; `hover` by default, `platform_icon_size`, `platform_icon_margin`); the badges at the top right take `badge_margin`. These are in the Badges section of Customize the platform's icon at its top left, in the badge colors. In its side panel, `chip_style` sets how the play time, last played and other chips look: `strip` (the default) puts them in one card split by thin lines, the value first and larger with its label under it (`strip_value_font_size`, `strip_label_font_size`, `strip_value_color`, `strip_label_color`, their `strip_hover_*` colors, `strip_uppercase_labels`, `strip_icons`, `strip_icon_size`, `strip_padding` (the space inside each chip; smaller lets three fit in a row sooner), and `strip_divider` and `strip_divider_width` for the lines), `cards` puts each in its own card and `bare` shows them without one (`chip_background`, `chip_border` and `chip_border_width` style the strip and the cards); the chips sit in two columns, or three once `panel_width` leaves room for the widest of them (about 400 with the default font), spread from edge to edge with equal gaps when bare, `card_opacity` how opaque the panel's cards are (chips, info, Friends, Achievements), `card_background`, `card_background_end`, `card_border` and `card_border_width` their look in the panel only (chips in cards follow them until `chip_*` is set), and `panel_logo_min_height` (a height always kept for the logo, so Launch doesn't move between games), `panel_logo_height` and `panel_logo_gap` set how tall the logo gets and the space under it, `panel_logo_top_gap` adds space above it, and `panel_buttons_gap` adds space between Properties and More and the content below. The menu button is labeled Actions (`more_label`). `show_buttons = false` hides Properties and More; Properties then appears as a text button opposite **Details**, and the game's menu stays on right-click and Shift+F10. The panel's Launch button and its Properties and More buttons have their own height, corners, outline, colors, text size and labels (`launch_*`, `button_*`, `properties_label`, `more_label`), following the usual button settings until you set them. The game page is not affected. The cover size slider at the bottom is `slider_width` long, with `pill_button_size` − and + buttons that also set the height of that pill and of the selection bar. `backdrop_enabled`, `backdrop_height`, `backdrop_fade_start`, `backdrop_opacity`, `backdrop_blur` and `backdrop_saturation` give Grid view its own blurred backdrop; it reaches further down and is a little stronger than the list's, and the blur and saturation follow `[backdrop]` until set.
- **`[filters]`** styles the filter panel. `more_width` is its width while More filters is open (never below the normal width) and `column_gap` the space between the two columns. The charts over the year and rating sliders use `histogram_height`, `histogram_bar` (outside the range), `histogram_bar_selected` (inside it) `histogram_bar_radius` and `histogram_empty_opacity` (the stub of an empty year or rating); the sliders use `slider_track`, `slider_fill`, `slider_handle` and `slider_handle_size`. `disabled_opacity` is how faint a section or toggle that doesn't apply is, like Group by in the grid view.
- **`[overview]`** styles the Library overview page. Its cards and tiles follow `[stats]` (Play statistics) until you set a value there, so the two pages match by default. `platform_colors` is a list of colors, one per platform in the order platforms are listed (then around again), used in Storage and Games by platform; a platform keeps its color however the rows are sorted. `tile_hover_tint` is laid over a top-row tile under the mouse (the tile also dips when pressed, by `[motion] press_scale`). `page_offset` and `card_stagger` set how far the page rises as it opens and how much later each card starts, `compact_width` and `narrow_width` the window widths below which three columns become two and then one, and the cover, bar and Storage values size the covers and bars. Reduce motion applies to its animations too.
- **Gradients:** every surface color `X` has a second color `X_end`. Set it to something else and the surface becomes a gradient; `gradient_direction` (`down`, `up`, `right`, `left`) sets which way.

A theme is a file in `themes/` naming a pair:

```toml
# themes/Night.toml
colors = "Night"
style = "Boxy"
```

`themes/colors/reference.toml` and `themes/styles/reference.toml` list every setting of that half with its default, what it accepts and what it does. They're rewritten at each start, so copy from them rather than editing them.

### Mistakes

A value Lattis can't use falls back to its default, and an unknown setting is reported with a "did you mean" suggestion. A file with a syntax error leaves the current look as it is until the error is fixed.

## Tag colors

Tag colors come from the theme's tag palette, which you can edit on Settings → Library. Each colors file has its own, so your tags can match each set of colors. The text on a filled tag is a light version of its own color, at the same lightness for every tag; `tags.text_style` can make it `dark` instead, or `custom` to use the `tags.text` color (`tags.light_text_lightness`, `tags.dark_text_lightness`). `tags.font_weight` (`thin`, `normal`, `semibold` by default, or `bold`) sets the weight of a tag's name.
