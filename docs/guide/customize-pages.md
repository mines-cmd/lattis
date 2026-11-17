# Customize: Play statistics, Library overview and Gallery

Part of the [Customize reference](customize.md). The three full-window pages opened from the toolbar. Library overview's cards and tiles follow Play statistics until you set them, so changing Play statistics restyles both.

## Play statistics (`[stats]`)

### Page

- `title_size`: the page title, the largest text on the page.
- `month_size`: the picked month or year between its arrows.
- `max_width`: the widest the content gets; it's centered in wider windows.
- `compact_width`: below this window width the columns stack. 0 = never.
- `page_offset`: how far the content rises as the page opens and sinks as it closes. 0 = fade only. The game page from Grid view and the other pages follow it.
- `section_spacing`: the space between cards.
- `label_size`: the uppercase labels on the tiles and highlight cards.
- `row_height`: the height of a platform's or a most played game's row.
- `text_muted`: labels, chart axes and hints.

### Cards

- `card_background`, `card_background_end`: every card's fill, tiles included.
- `card_border`, `card_border_width`, `card_radius`: their outline and roundness.
- `card_padding`: the space inside a card.
- `card_title_size`: the titles of chart and ranking cards (Hours by day, Most played).

### Tiles

The summary tiles across the top (Hours played, Games played, Sessions…).

- `tile_value_size`, `tile_primary_value`: the big number, and Hours played's number color.
- `tile_padding_x`: the space either side of a tile's text.
- `delta_up`, `delta_down`: the change from the previous month when it's up and when it's down.
- `tile_icon`, `tile_icon_size`, `tile_icon_opacity`, `tile_icon_overhang`: the large faded icon in each tile's corner: its color, size (0 hides it), strength and how much runs off the tile.
- `light_brightness`: how much lighter than the accent today's bar and Hours played are drawn. The two-way toggles follow it.

### Charts

- `bar`, `bar_end`, `bar_opacity`: chart bars and how strongly they show. Today's and the hovered column are full strength.
- `bar_active`, `bar_active_end`: the hovered column, and the picked month in the year strip.
- `merged_bar`, `merged_opacity`: play time from Steam while Lattis was closed, drawn fainter.
- `track`: behind the platform and most played bars and the year strip. Many placeholders follow it.
- `bar_radius`, `bar_gap`: the bars' roundness and the gap between day columns.
- `chart_height`: the hours-by-day chart's height.
- `week_label_opacity`: the week names under the daily chart.
- `strip_height`, `strip_year_width`: the year strip's bar height, and a bar's width in its years view.

### Month and year toggle

Follows the [two-way toggles](customize-basics.md#two-way-and-three-way-toggles-segmented) until set.

- `toggle_checked_background`, `toggle_checked_opacity`, `toggle_checked_text`: the picked month or year in the title picker, and the checked Month | Year choice.
- `toggle_text`: the other choices and cells.

## Library overview (`[overview]`)

### Page

- `title_size`, `max_width`, `section_spacing`, `text_muted`, `page_offset`: as on Play statistics.
- `compact_width`, `narrow_width`: the window widths below which three columns become two, and two become one.
- `card_stagger`: how much later each card starts to rise as the page opens. 0 = all together.
- `column_stretch`: when one column ends this much or less above the other, its last card grows to end level.
- `platform_colors`: the colors Storage and Games by platform give platforms without their own color, in order. A platform's own color can be set as `color = "#rrggbb"` in `library/platforms.toml`.

### Cards and tiles

- `card_background`, `card_background_end`, `card_border`, `card_radius`, `card_padding`, `card_title_size`: the cards (Continue playing, Storage, Games by platform…).
- `tile_value_size`, `tile_primary_value`, `tile_padding_x`: the tiles across the top.
- `tile_hover_tint`: the wash over a tile under the mouse.

### Continue playing

- `cover_width`, `cover_max_width`, `cover_ratio`: covers grow to share the card's width between these sizes and scroll sideways when they don't fit; the ratio is height to width.
- `cover_radius`, `cover_spacing`, `cover_placeholder`: their roundness, gap, and what shows while one loads.
- `cover_hover_overlay`, `cover_hover_zoom`: the darkening and zoom on a cover under the mouse.
- `cover_play_size`, `cover_play_inset`, `cover_play_icon_size`: the round Play button on a hovered cover.
- `cover_play_background`, `cover_play_hover_background`, `cover_play_icon`: its colors.

### Storage

- `storage_bar_height`, `storage_bar_radius`: each drive's bar.
- `storage_segment_gap`, `storage_segment_min_width`: the gaps between platform parts of the bar, and the narrowest a part is drawn.
- `storage_fold_percent`: platforms taking less of a drive than this are counted in Other.
- `storage_free`, `storage_other`: the free space, and what's on the drive besides games.
- `storage_thumb_width`, `storage_cover_outline`: the covers in Largest games and their thin outline.
- `storage_size_bar_height`: the bar under each of those games showing its size against the largest. 0 = none.
- `storage_row_radius`: the highlight behind a game there.

### From your backlog

- `backlog_cover_width`: the suggested game's cover.

### Needs attention

- `attention_icon_tile`, `attention_icon_size`, `attention_icon_background`, `attention_icon`: the square and icon beside each row. 0 tile = no icons.
- `attention_count_width`, `attention_count_size`: the numbers' column width and size.

### Breakdowns

Games by platform, Status and Release years.

- `bar_height`, `bar_radius`, `bar_track`: the bars and what's behind them.
- `bar_fill`: Status bars and release year columns. Platform bars use the platform colors.
- `bar_active`, `bar_opacity`: the year column under the mouse, and how strong the others are.
- `row_padding`: the space above and below a row, inside its hover highlight.
- `label_width`, `count_width`: the names' column and the counts' column, so bars line up across cards.
- `years_height`, `year_gap`: the release year columns' height and gap.

### Compatibility

- `compat_none`: games without ProtonDB reports on the bars.
- `compat_tile_background`, `compat_tile_radius`: behind the Native and Proton counts.
- `compat_divider`: the line above the most used Proton version.

### Save backups

- `saves_tile_background`, `saves_tile_radius`: the three tiles (last backup, number of backups, space used).
- `saves_icon_background`, `saves_icon`: the shield shown when nothing is backed up.

### Top tags

Follows the filter panel's tags until set.

- `tag_rows`: how many rows of tags show.
- `tag_spacing`, `tag_border_width`: the gap and outline.
- `tag_text_lightness`, `tag_off_opacity`, `tag_hover_opacity`, `tag_count_opacity`: their text, fill, fill under the mouse, and count.

### Edit mode

What you see while customizing the overview (hiding and moving cards).

- `edit_button_size`, `edit_button_inset`, `edit_button_radius`, `edit_button_background`, `edit_button_border`: the squares behind a card's grip and eye.
- `edit_icon`, `edit_icon_hover`, `edit_icon_size`: the grip and eye icons.
- `edit_hidden_opacity`: how strongly a hidden card still shows.
- `edit_drag_opacity`: a card being dragged.
- `edit_placeholder`: "Nothing to show now" on an empty card.
- `edit_drop_indicator`, `edit_drop_indicator_width`: the line where a dragged row will land.

## Gallery (`[gallery]`)

Every clip and screenshot in the library as a grid. The viewer it opens is the [Media viewer](customize-game-page.md#viewer), plus the top bar below.

### Page

- `title_size`, `count_color`: the title and the number of items beside it.
- `compact_width`: below this window width the filters hide behind a button.
- `page_offset`: how far the content rises as the page opens.
- `button_height`, `button_radius`, `button_spacing`: the header's buttons (Group, sort, captions).
- `slider_width`, `slider_track_height`, `slider_handle_size`, `slider_fill`: the tile size slider.
- `slider_step`: how much the − and + buttons change a tile's width.

### Sidebar

The filter sidebar. Follows the filter panel until set.

- `sidebar_width`, `sidebar_padding`, `sidebar_spacing`: its width, inner space and gap between sections.
- `sidebar_background`, `sidebar_background_end`, `sidebar_gradient_direction`, `sidebar_border`, `sidebar_border_width`, `sidebar_radius`: its box.
- `sidebar_scrollbar_width`, `sidebar_title_size`: its scroll bars and title.
- `section_color`, `section_size`, `section_uppercase`: section headings.
- `entry_color`, `entry_size`, `entry_height`, `entry_count_color`: each filter entry and its count.
- `entry_hover`, `entry_radius`: an entry under the mouse.
- `entry_empty_opacity`: an entry the other filters leave empty.
- `month_indent`: how far months sit in from their year.
- `list_height`: the tallest the date tree grows before it scrolls.
- `search_height`: the Search games field.
- `clear_color`: the Clear and Clear all links.
- `filter_dot_color`, `filter_dot_size`: the dot on the Filters button (narrow windows) while a filter is on.

### Grid

- `tile_width`, `tile_min_width`, `tile_max_width`: a tile's starting width and the size slider's range.
- `tile_gap`, `scrollbar_width`: the gap between tiles, and the scroll bar.
- `tile_radius`, `tile_shadow_color`, `tile_shadow_size`, `tile_hover_scale`, `tile_border_color`: the tiles. Follow the Media row's thumbnails.
- `placeholder_color`: what a tile shows until its picture arrives.
- `row_stagger`: the delay between rows fading in when the page opens. 0 = all at once.
- `game_name_size`, `game_name_color`, `date_color`: the captions under tiles.

### Groups

- `group_card_background`, `group_card_border`, `group_card_border_width`, `group_radius`, `group_padding`, `group_spacing`: the card behind each group.
- `group_height`, `group_size`, `group_color`, `group_count_color`, `group_icon_size`: a group's heading, its name, count and platform icon.
- `group_background`: around the corners of a heading while it sticks to the top.

### Viewer

- `viewer_line_size`, `viewer_line_color`, `viewer_line_muted`: the game's name in the viewer's top bar, and the date and position under it.
- `viewer_game_hover`: the Go to game button under the mouse.
