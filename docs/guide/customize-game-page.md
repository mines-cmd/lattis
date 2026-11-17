# Customize: the game page

Part of the [Customize reference](customize.md). A game's page right of the list in List view (and opened from the Grid view): the banner at the top, the Launch row under it, the description and tags, the info panel beside them, the Achievements and Friends panels, the Media row, and the download bar. Listed from top to bottom.

## Page layout (`[details]`)

The frame everything on the page sits in: margins, gaps, when it switches to a narrow layout, and the text of the description and headings.

### Layout

- `margin_left`: the gap between the game list and the page.
- `margin_right`, `margin_bottom`: the space at the right, and below the content when scrolled to the end.
- `margin_top`: the space between the toolbar and the banner. 0 puts the banner level with the bottom of the toolbar.
- `padding`: extra space on both sides of the content, inside the margins.
- `spacing`: the vertical gap between the banner, the Launch row and the body.
- `action_spacing`: the gap between items in the Launch row (the button, the stat chips, the icon buttons).
- `button_spacing`: the gap between the icon buttons in that row.
- `column_spacing`: the gap between the description and the info panel.
- `compact_width`, `compact_margin_right`: when the page is narrower than this, it goes compact: the stats move under the Launch button, the info panel below the description, and the right margin becomes `compact_margin_right`. 0 = never.
- `min_width`: the narrowest the page may get. Together with the rail and the list, it sets how narrow the window can be.
- `message_width`: the widest a message gets when there's no game to show (the empty library hint).
- `page_offset`: how far the page rises as it opens from the Grid view and sinks as it closes. 0 = fade only.

### Behavior

- `animate_description`: fade and rise the description when the game changes. Off swaps it at once.
- `animate_tags`: glide and morph the tags when the game changes.
- `rapid_switch`: picking another game within this many milliseconds of the last pick (scrolling with the arrow keys) skips the switch animation. 0 = always animate.
- `fit_description`, `description_min_lines`: cut a long description to the lines that fit the window, with Show more, so the page doesn't scroll; and the fewest lines it's cut to.

### Text

- `heading_color`, `heading_font_size`: the section headings (Description, Tags, Media).
- `count_color`, `count_font_size`, `count_font_weight`: the number beside a heading (the Media row's count).
- `collapse_chevron_size`, `collapse_chevron_gap`, `collapse_chevron_color`: the chevron after a heading, shown on hover and while its section is folded. Clicking a heading folds the section; 0 size hides the chevron.
- `description_color`, `description_line_height`: the description's text and line spacing.
- `link_color`: Show more and Show less under a cut description.
- `tag_hover_lift`: how far a tag on the page rises under the mouse.

## Banner (`[hero]`)

The large banner image at the top of the page, with the game's logo or title on it.

### Banner

- `aspect`: its width-to-height shape (Steam's banners are 3.1).
- `max_height`: on wide windows the banner is cropped instead of growing taller than this.
- `min_height`, `logo_only_height`: below `min_height` (a narrow page) the art is hidden and only the logo shows, at most `logo_only_height` tall. 0 = never hide.
- `radius`: its roundness.
- `load_delay`: how long after the selection changes before the banner loads, which keeps fast scrolling smooth.
- `fade_duration`: how long the new art fades in when you switch games. 0 = at once.

### Logo

- `logo_position`: where the logo (or title) sits on the banner. You can also drag it per game by right-clicking the logo.
- `logo_max_width`, `logo_max_height`: the largest the logo gets, as a share of the banner's width and height.
- `logo_margin`: its distance from the edges.
- `title_color`, `title_weight`, `title_max_font_size`: the game's name, drawn instead when there's no logo image.

### Moving the logo

What you see while adjusting a game's logo (right-click the logo).

- `resize_frame`, `resize_frame_width`: the outline around the logo.
- `resize_handle_size`: the corner handle that resizes it.
- `snap_distance`: how close a dragged logo gets to the margins and center lines before it snaps to them. 0 = no snapping.
- `guide_color`, `guide_width`: the line shown where it snaps.

### Scrim and shadow

What keeps the logo readable on bright art.

- `scrim`: darken the art behind the logo at all.
- `scrim_style`: a gradient over the art, or a soft shadow that follows the logo's outline.
- `scrim_color`, `scrim_side`: the gradient's darkest color and where it sits: `auto` (the logo's side), a chosen side, or `full` (an even dim over the whole banner).
- `shadow_color`, `shadow_size`, `shadow_offset`: the shadow style's color, how far its blur reaches, and how far down it's moved.

Each game can override the scrim from its logo's right-click menu.

### Switching games

- `animate_art`, `switch_zoom`: fade the new art in over the old as it settles from a slight zoom, and how much larger it starts. Off swaps at once.
- `animate_logo`: fade and rise the new logo or title.
- `switch_rise`: how far the new logo, description and tags rise as they fade in.
- `switch_timeout`: the longest the previous game's banner stays while the next one's art loads. 0 = switch at once.

### Placeholder

The gradient shown for a game without banner art (and behind it in the backdrop). Its hue comes from the game itself.

- `placeholder_saturation`, `placeholder_lightness`: how vivid and light the gradient starts.
- `placeholder_end_saturation`, `placeholder_end_lightness`: how it ends.
- `placeholder_hue_shift`: how far the hue turns from start to end.

## Launch button (`[launch_button]`)

The big button under the banner. It changes label and color with the game's state.

### Labels

- `label`: when it launches the game ("Launch").
- `uninstalled_label`: when the game isn't installed.
- `missing_rom_label`: when a ROM game's file isn't in its ROM folders.
- `missing_game_label`: when a Proton game's program isn't where it should be.
- `install_label`: when a Steam game isn't installed; clicking installs it through Steam.
- `running_label`, `stop_label`: while the game runs, and while the mouse is over it then (empty = same). Clicking asks to close the game.
- `update_label`: when Steam has an update waiting.
- `updating_label`, `installing_label`, `paused_label`: while Steam downloads an update, a first install, or has it paused.

### Shape

- `width`, `height`: its size. The stat chips follow its height.
- `radius`: its roundness.
- `border`, `border_width`: its outline (none by default).
- `hover_brightness`: how much brighter it gets under the mouse. 1 = no change.
- `gradient_direction`: the direction of its gradients.

### Colors by state

Each has an `_end` for a gradient.

- `background`, `background_end`: ready to launch (accent).
- `uninstalled_background`, `uninstalled_background_end`: not installed.
- `install_background`, `install_background_end`: when it installs a Steam game.
- `running_background`, `running_background_end`: while the game runs (green).
- `stop_background`, `stop_background_end`: while it runs and the mouse is over it.
- `update_background`, `update_background_end`: when it updates a game.
- `updating_background`, `updating_background_end`: while Steam updates or installs, or has it paused. The download bar's progress follows it.

### Text

- `text`, `uninstalled_text`, `install_text`, `running_text`, `update_text`, `updating_text`: the label's color in each state.
- `font_size`, `bold`: the label's size and weight.
- `hover_text_shadow`, `hover_text_shadow_blur`, `hover_text_shadow_offset`: a shadow under the label while the mouse is over the button.

## Stat chips (`[stat_chip]`)

The small boxes beside the Launch button: play time, last played, rating, ProtonDB, Steam changes waiting.

- `show_icons`, `icon_size`, `icon_color`: their icons.
- `height`, `padding`, `radius`: their height (follows the Launch button), side padding and roundness.
- `background`, `background_end`, `gradient_direction`, `border`, `border_width`: their box. Invisible by default.
- `label_color`, `label_font_size`, `uppercase_labels`: the label (PLAY TIME).
- `value_color`: the value (40.9 hours).
- `hover_value_color`, `hover_label_color`, `hover_icon_color`: all three under the mouse, on a chip that opens something (play time opens play history).
- `button_height`: the Apply button inside the Steam changes chip.
- `star_size`: the stars in the rating chip's tooltip.
- `protondb_native`, `protondb_platinum`, `protondb_gold`, `protondb_silver`, `protondb_bronze`, `protondb_borked`, `protondb_pending`: the ProtonDB chip's icon color for each rating.

## Icon buttons (`[icon_button]`)

The square buttons in the Launch row: Favorite, Open folder, Properties, the ⋯ menu, Open emulator.

- `size`, `icon_size`, `radius`: their size, icon size and roundness.
- `background`, `background_end`, `hover_background`, `hover_background_end`, `gradient_direction`: their fill, at rest and under the mouse.
- `border`, `border_width`: their outline.
- `icon`: the icons' color.
- `icon_inactive`, `icon_active`: toggles such as Favorite when off and on. Filled rating stars follow `icon_active`.
- `badge`, `badge_size`, `badge_margin`: the dot on Properties when Steam changes are queued (and on the filter button while a filter is on).
- `badge_border`, `badge_border_width`: the ring around that dot that makes it look cut out of the button.
- `spin_duration`, `busy_timeout`: the spinner on Open emulator while the emulator starts: the time per turn, and the longest it spins.

## Tags (`[tags]`)

The colored labels on a game's page, in the game editor and in filter chips.

- `palette`: the colors offered when you create a tag, in order. Also editable on Settings → Library.
- `height`, `padding`, `radius`, `spacing`: each tag's height, side padding, roundness and the gap between them.
- `border`, `border_width`: their outline (none by default).
- `text_style`: the text on a filled tag: `light` or `dark` (a light or dark version of the tag's own color, the same lightness for every tag) or `custom` (the `text` color).
- `light_text_lightness`, `dark_text_lightness`: how light that text is in each style.
- `text`: the text color when `text_style` is `custom`.
- `font_size`, `font_weight`: the tag name's size and weight.
- `hover_lighten`: how much a clickable tag brightens under the mouse on a game's page.
- `outline_width`: the outline of tags you can still add, in the game editor.
- `remove_hover`: the circle behind a tag's × under the mouse, in the game editor.

## Info panel (`[info_panel]`)

The box beside the description (or below it when compact) with release date, platform, status, size, rating. Achievements, Friends and the Grid view's cards follow its look.

### Layout

- `width`: its width.
- `padding`, `vertical_padding`: its inner space left and right, and above and below the rows.
- `row_height`, `row_padding`, `label_spacing`: each row's height, the space either side of its text, and the least gap between label and value.
- `radius`: its roundness.
- `icon_size`: the icon beside a value (the platform's).
- `show_dividers`, `divider`, `divider_width`: the lines between rows.

### Colors

- `background`, `background_end`, `gradient_direction`: its fill.
- `border`, `border_width`: its outline.
- `label_color`, `value_color`: each row's label and value.
- `value_hover_color`: the platform value under the mouse (clicking it shows only that platform).

### Rating

- `star_size`, `star_spacing`: the rating stars' size and gap.
- `star`, `star_empty`: filled and empty stars.

## Achievements (`[achievements]`)

The panel under the info panel on a Steam game's page, and the detailed view that opens from "Show all".

### Panel

- `gap`: the space between the info panel and this panel.
- `padding`, `side_padding`: its inner space above/below and left/right.
- `radius`, `background`, `background_end`, `gradient_direction`, `border`, `border_width`: its box. Follows the info panel.

### Header

- `title_color`, `title_weight`: the word "Achievements".
- `count_color`, `percent_color`: how many are unlocked (3 / 15) and the percentage after it.
- `complete_color`, `complete_percent_opacity`: the count and percentage once every achievement is unlocked.
- `trophy_size`: the medal before the bar once all are unlocked.
- `bar_height`, `bar_color`, `bar_fill`: the progress bar's thickness, empty part and filled part.

### Icons

- `icon_size`, `icon_gap`, `icon_radius`: each achievement's icon, the gap between them and their roundness.
- `icon_placeholder`: behind an icon that's loading or missing.
- `locked_opacity`: how visible the grey icons of locked achievements are.
- `max_rows`: rows of icons before "Show all".
- `link_color`, `link_hover_color`: "Show all", and under the mouse.

### Icon tooltip

What appears when you hover an icon.

- `tip_width`, `tip_padding`, `tip_gap`: its width, inner space and the gap between icon and text.
- `tip_icon_size`: the icon's size in it.
- `tip_header_background`: behind the icon, name and description.
- `tip_name_color`, `tip_name_weight`: the achievement's name.
- `tip_description_color`, `tip_date_color`: its description, and when it was unlocked (or Locked).
- `tip_rarity_color`, `tip_rare_color`: the share of players who unlocked it, and the same for a rare one.

### Detailed view

The large view "Show all" opens over the window.

- `viewer_scrim`: dims the window behind it.
- `viewer_width`, `viewer_margin`, `viewer_padding`: its width, its space from the window's edges and its inner space.
- `viewer_background`, `viewer_radius`, `viewer_border`, `viewer_border_width`: its box. Follows the info panel.
- `viewer_backdrop_opacity`, `viewer_backdrop_saturation`: the game's blurred art behind the view: its strength and color.
- `viewer_cover_height`, `viewer_cover_radius`: the game's cover at the top. 0 hides it.
- `viewer_title_color`, `viewer_title_size`, `viewer_title_weight`: the game's name at the top.
- `viewer_year_color`, `viewer_year_size`, `viewer_year_weight`: the release year beside it.
- `viewer_play_time_color`: the play time under the name.
- `viewer_text_shadow`, `viewer_text_shadow_blur`, `viewer_text_shadow_offset`: a shadow under the name, year and play time, so they read on a light backdrop.
- `viewer_summary_background`, `viewer_summary_radius`, `viewer_summary_padding`: the card behind the progress bar.
- `viewer_progress_color`, `viewer_percent_color`: "4 / 29 achievements unlocked" and the percentage.
- `viewer_bar_height`, `viewer_bar_color`, `viewer_bar_fill`: the progress bar.
- `viewer_complete_color`, `viewer_complete_bar_fill`, `viewer_trophy_size`: the medal and bar once every achievement is unlocked.
- `viewer_filter_height`, `viewer_filter_width`: the search box and the All / Unlocked / Locked filter.
- `viewer_filter_background`, `viewer_filter_hover_background`: behind them, and the filter under the mouse.
- `viewer_filter_text`, `viewer_filter_placeholder`, `viewer_filter_icon`: their text, hint and icons.
- `viewer_close_gap`: the close button's distance from the top right corner.
- `section_color`, `viewer_section_weight`: the Unlocked, Locked and Hidden headings.
- `viewer_icon_size`, `viewer_icon_radius`, `viewer_icon_placeholder`: each achievement's icon in the list.
- `viewer_locked_opacity`: how visible locked achievements' grey icons are.
- `viewer_hidden_mark_color`: the ? on hidden achievements.
- `viewer_name_color`, `viewer_name_weight`, `viewer_description_color`: each achievement's name and description.
- `viewer_date_color`: when it was unlocked.
- `viewer_rarity_color`, `viewer_rare_color`: the share of players who unlocked it, normal and rare.
- `viewer_divider`: the line between achievements.
- `viewer_row_hover`, `viewer_row_radius`: the highlight behind an achievement under the mouse.
- `rare_percent`: an achievement fewer than this percent of players unlocked counts as rare.

### Rare glow

- `rare_glow_enabled`: a glow with rays behind the icon of an unlocked rare achievement.
- `rare_glow_color`, `rare_glow_opacity`: its color and strength.
- `rare_glow_size`, `rare_glow_softness`: how far it reaches past the icon and how blurred it is.

### Complete wall

Behind the progress bar of a game with every achievement unlocked, a slowly scrolling wall of its icons.

- `wall`: show the wall.
- `wall_motion`, `wall_speed`: scroll its rows, and how fast.
- `wall_icon_size`, `wall_icon_gap`, `wall_icon_count`: the icons' size, gap, and how many different ones it uses (newest unlocked first).
- `wall_tilt`: the angle the rows are turned by.
- `wall_opacity`: how visible the wall is.
- `wall_tint`, `wall_dim`, `wall_vignette`: the color laid over the wall, how strongly it covers all of it, and how strongly toward the edges.
- `wall_gradient_top`, `wall_gradient_bottom`, `wall_gradient_left`, `wall_gradient_right`: the colored gradients over each side of the wall. The bottom one hides it toward the bar.

## Friends (`[friends]`)

The panel under the info panel on a Steam game's page with the friends who play it.

### Panel

- `padding`, `side_padding`: its inner space.
- `radius`, `background`, `background_end`, `gradient_direction`, `border`, `border_width`: its box. Follows the info panel.

### Header and groups

- `title_color`, `title_weight`: the top group's heading.
- `link_color`, `link_hover_color`: "View all".
- `group_gap`, `group_color`, `group_weight`: the space between groups and the headings of the groups below the top one ("Played recently 3").
- `recent_max`, `recent_columns`: how many friends show under Played recently, and how many side by side.

### Friends

- `avatar_size`, `avatar_radius`, `avatar_placeholder`: a friend's picture.
- `row_gap`, `row_padding`: the space between friends and around each inside its highlight.
- `row_hover`, `row_radius`: the highlight behind a friend under the mouse.
- `name_gap`: the space between picture and name.
- `name_color`, `playing_color`, `online_color`: a friend's name when offline, playing now, and online.
- `name_font_size`, `name_weight`: the name's size and weight.
- `hours_color`, `hours_opacity`, `hours_font_size`: the line under the name (time played, or the game being played).

### Played previously

- `previous_size`, `previous_gap`, `previous_max_rows`: the row of small pictures of friends who played before: their size, gap and how many rows before "+N".
- `more_color`: that "+N".

## Media row and viewer (`[media]`)

The row of Steam clips, screenshots and your own media on a game's page, and the viewer that plays them full window. The Gallery's viewer is this same viewer.

### Row

- `card_background`, `card_border`, `card_border_width`, `card_radius`, `card_padding`: the card the thumbnails scroll inside. Transparent = no card.
- `thumb_height`: a thumbnail's height (width follows at 16:9).
- `thumb_gap`, `thumb_radius`: the gap between thumbnails and their roundness.
- `thumb_border_color`: their outline (invisible by default).
- `thumb_shadow_color`, `thumb_shadow_size`: the drop shadow under each.
- `thumb_hover_scale`: how much one grows under the mouse.
- `thumb_fit_below`: pictures narrower than this shape (like a 3DS screenshot of both screens) are shown whole instead of cropped. 0 = always crop.
- `thumb_scrim`, `thumb_scrim_style`: the darkening on clip thumbnails that keeps their length readable, and where it sits.
- `badge_background`, `badge_text`, `badge_icon_size`: the pill with a clip's length and its play icon.
- `favorite_badge_background`, `favorite_badge_icon`, `favorite_badge_size`: the star badge on a favorite's thumbnail.
- `more_background`, `more_blur`, `more_scrim`, `more_scrim_opacity`, `more_text`, `more_font_size`: the last tile ("+93 more", opens the Gallery), which blurs the next picture behind its text.
- `more_hover_scrim`, `more_hover_scrim_opacity`: that tile under the mouse.
- `edge_fade`: the fade at an edge the row can still scroll past.
- `scrollbar_gap`, `scrollbar_height`, `scrollbar_track`, `scrollbar_handle`, `scrollbar_handle_hover`: the scroll bar under the thumbnails.
- `link_color`, `link_hover_color`, `link_inset`: "Show all" after the Media heading, which opens the Gallery.

### Viewer

- `viewer_scrim`: dims the window behind the viewer.
- `viewer_margin`, `viewer_top_margin`, `viewer_bottom_margin`: the space between the picture and the window's edges.
- `viewer_control_color`: the viewer's buttons; most viewer colors follow it.
- `viewer_close_color`: the close button.
- `viewer_time_color`, `viewer_duration_color`: the time (0:01) and the clip's length after the slash.
- `viewer_bar_scrim`, `viewer_bar_scrim_reach`, `viewer_bar_padding`: the shade behind the top and bottom bars, how far it reaches, and the bars' distance from the picture's edges.
- `viewer_hide_delay`: how long the bars stay after the mouse stops. 0 = always shown.
- `viewer_button_size`, `viewer_icon_size`, `viewer_play_icon_size`, `viewer_button_hover`: the round buttons, their icons, the play icon, and a button under the mouse.
- `viewer_arrow_inset`: the ‹ › buttons' distance from the window's sides.
- `viewer_controls_gap`: the gap between groups of controls in the bottom bar.
- `viewer_pill_background`, `viewer_pill_radius`, `viewer_pill_padding`: the dark pill behind each group of controls and each top bar item.
- `seek_height`, `seek_hover_height`: the seek and volume bars' thickness, and under the mouse.
- `seek_track`, `seek_fill`, `seek_hover_fill`, `volume_fill`: the empty part, the played part, the part up to the mouse, and the set volume.
- `seek_knob`, `seek_knob_size`: the knob shown on a bar under the mouse.
- `seek_tip_background`, `seek_tip_text`, `seek_tip_radius`: the time shown above the seek bar under the mouse.
- `viewer_volume_width`: the volume bar that slides out beside the speaker.
- `viewer_flash_size`, `viewer_flash_icon_size`, `viewer_flash_background`: the sign that flashes in the middle on play, pause, seeking and volume changes. 0 = none.

## Download bar (`[download_bar]`)

The bar along the bottom of a game's page while Steam downloads a game, which folds into a pill.

### Bar

- `show`: show it at all.
- `height`, `padding`, `radius`: its height, inner space and roundness.
- `overhang`: how far it reaches past the page content on the left.
- `background`, `background_end`, `gradient_direction`, `border`, `border_width`: its box.
- `shadow_color`, `shadow_size`: the shadow under it.
- `icon_size`: the game's icon. 0 hides it.
- `title`, `text`: the game's name, and the label and percentage.

### Progress

- `track_height`, `track_radius`, `track`: the progress bar's thickness, roundness and empty part.
- `fill`, `paused_fill`: the done part, and while paused. Follows the Launch button's Updating color.
- `fill_duration`: how long the bar takes to move to a new value.

### Collapsing

- `chevron_size`, `chevron_hover`: the hide and show chevrons, and under the mouse. 0 hides them, so the bar can't be folded.
- `collapse_duration`: how long folding into the pill takes.
- `pill_height`, `pill_radius`, `pill_icon_size`: the pill left when the bar is folded.
