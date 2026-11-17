# Customize reference

Settings → Customize has a setting for almost every color, size and animation in Lattis, about 1,700 of them. These pages name every one, grouped by where you see the thing it changes, and say what each does, so you can go from "I want that smaller" to the setting that does it. How Customize itself works (the color picker, Undo, filters, the ⋯ menu) is on the [Themes](themes.md) page.

## The pages

- [Shared basics](customize-basics.md): the palette, corner roundness, motion, fonts, spacing, and the buttons, fields, menus, popovers and tooltips that look the same everywhere. Start here to change the whole app at once.
- [The main window](customize-main-window.md): the blurred backdrop, the rail, the toolbar, the search box and filter panel, collections, the game list, the Grid view, and the banners and messages that pop up.
- [The game page](customize-game-page.md): the banner and logo, the Launch button, the stat chips and icon buttons, tags, the info panel, Achievements, Friends, the Media row and viewer, and the download bar.
- [Play statistics, Library overview and Gallery](customize-pages.md): the full-window pages opened from the toolbar.
- [Settings, dialogs and the setup guide](customize-dialogs.md): the Settings window, Properties and the dialogs that share its look, the setup guide, play history, the date picker and About.

## Reading the names

Each setting is listed by its name in the theme file, like `badge_margin`. Customize shows the same name in words, "Badge margin", under its part's heading (the part's file name, like `[grid]`, is in each heading here). To jump to one, type either form in Customize's search box, or the full `grid.badge_margin`.

Many names follow the same patterns:

- **`_end` and `gradient_direction`**: a color with an `_end` twin becomes a gradient when you give the twin a different color. Left alone, it stays flat.
- **`hover_…`, `selected_…`, `checked_…`, `…_on`**: the same thing under the mouse, selected, or switched on.
- **`border`, `border_width`**: an outline's color and thickness. A width of 0 removes it.
- **`radius`**: corner roundness. Most follow one of the four tiers in `[radius]`.
- **`…_opacity`**: how strongly something shows, from 0 (invisible) to 1 (solid).
- **`…_size`, `…_height`, `…_width`, `padding`, `spacing`, `gap`, `margin`**: in pixels.
- **`…_duration`, `delay`**: in milliseconds.
- **`…_scale`**: a size to grow from or shrink to, where 1 means no change.

## Changing one thing or everything

Most settings follow another by default: a violet tag in Customize names the one it follows. Change the one being followed and everything following it changes too; change the follower and only that one changes. So:

- To change every panel's background, change `surface` in `[palette]`. To change only the info panel's, change `background` in `[info_panel]`.
- To make everything rounder or square, change the tiers in `[radius]`. To change only the Launch button, change `radius` in `[launch_button]`.
- To remove every outline, set `border_width` in `[layout]` to 0.
- To slow down or turn off every animation, change `duration_scale` in `[motion]`.

## Common wishes

| I want to… | Look at |
| --- | --- |
| Change the main highlight color | `accent` in [Palette](customize-basics.md#palette-palette) |
| Make the window darker or lighter | `background`, `surface` in [Palette](customize-basics.md#palette-palette) |
| Make text bigger | `size` in [Fonts](customize-basics.md#fonts-font) |
| Use another font | `family` in [Fonts](customize-basics.md#fonts-font) |
| Turn animations off | `duration_scale` in [Motion](customize-basics.md#motion-motion) |
| Make the blurred art behind the window weaker | `opacity` in [Backdrop](customize-main-window.md#blurred-art-behind-the-window-backdrop) |
| Make the game list wider, or rows shorter | `width`, `row_height` in [Game list](customize-main-window.md#game-list-list) |
| Hide the icons in the game list | `show_icons` in [Game list](customize-main-window.md#icons) |
| Show a Play button on Grid view covers | `play_button` in [Grid view](customize-main-window.md#badges) |
| Change the side panel's chips in Grid view | `chip_style` in [Grid view](customize-main-window.md#side-panel-chips) |
| Make the banner shorter | `max_height`, `aspect` in [Banner](customize-game-page.md#banner-hero) |
| Make the logo on the banner readable | `scrim`, `scrim_style` in [Banner](customize-game-page.md#scrim-and-shadow) |
| Rename the Launch button | `label` in [Launch button](customize-game-page.md#labels) |
| Change the Launch button's color while a game runs | `running_background` in [Launch button](customize-game-page.md#colors-by-state) |
| Change tag text | `text_style` in [Tags](customize-game-page.md#tags-tags) |
| Hide the window buttons | `buttons` in [Window buttons](customize-basics.md#window-buttons-and-edges-window_controls) |
| Hide the rail logo | `show_logo` in [Rail](customize-main-window.md#logo) |
| Change how long tooltips take | `delay` in [Tooltips](customize-basics.md#tooltips-tooltip) |
