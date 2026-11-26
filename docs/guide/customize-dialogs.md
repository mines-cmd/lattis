# Customize: Settings, dialogs and the setup guide

Part of the [Customize reference](customize.md). The Settings window, the game's Properties dialog and the smaller dialogs that share its look, the setup guide, play history, the date picker and About. Many of these follow the Settings window until set, so changing the Settings window's cards and sidebar restyles the dialogs too.

## Settings window (`[settings_window]`)

### Window

- `background`, `background_end`: the window's background.
- `content_padding`: the space around the content area.
- `window_border`, `window_border_width`, `window_border_radius`: the outline around the window while it isn't maximized, and its roundness.
- `window_border_follow_compositor`: on Hyprland, use its window rounding for that outline.
- `remember_size`, `width`, `height`: open at the size it had when closed, or always at this width and height.
- `controls_margin`: the window buttons' distance from the edge.
- `page_description_top_margin`: the distance from the top to the page description beside the window buttons.
- `page_offset`: how far pages slide when you switch categories. 0 = fade only.
- `toolbar_height`: the height of the theme picker and search bar on Customize.

### Sidebar

The category list on the left. Follows the rail until set. The Properties dialog's tab list follows this.

- `sidebar_container`, `sidebar_container_margin`, `sidebar_container_radius`: draw it as a floating rounded panel, the space around it and its roundness.
- `sidebar_width`, `sidebar_padding`: its width and inner space.
- `sidebar_background`, `sidebar_background_end`, `sidebar_border`, `sidebar_border_width`: its fill and outline.
- `sidebar_item_height`, `sidebar_item_radius`, `sidebar_icon_size`: each category's row, highlight roundness and icon.
- `sidebar_text`, `sidebar_text_muted`: category names, and placeholder categories.
- `sidebar_selected_background`, `sidebar_selected_background_end`, `sidebar_selected_text`: the selected category.
- `sidebar_section_height`, `sidebar_section_indent`, `sidebar_section_font_size`: the section links under Customize (Rail, Game list…).
- `sidebar_subsection_indent`: how far the group links under the current section are indented.
- `sidebar_section_text`, `sidebar_section_selected_text`, `sidebar_section_selected_background`, `sidebar_section_marker`: section links, and the one you're scrolled to with its highlight and marker bar.

### Settings list

- `card_background`, `card_background_end`, `card_border`, `card_border_width`, `card_radius`, `card_padding`: the cards holding a category's settings. Dialog cards follow these.
- `section_spacing`, `section_header_color`, `section_header_font_size`: the space between categories and their headings ("Rail").
- `subsection_color`, `subsection_font_size`, `subsection_spacing`: the group headings within a category ("Sidebar").
- `row_padding`, `row_radius`, `row_label_spacing`: each setting row's inner space, roundness and the gap between its text and its control.
- `row_dividers`, `row_divider`: the lines between settings.
- `editor_width`: the width of the controls column on the right.
- `label_min_width`: the narrowest the names get before the controls shrink.
- `open_row_header_background`, `open_row_background`, `open_row_darkness`, `open_row_border`, `open_row_border_width`: an expanded emulator or platform row: its header, the darker card below it, and the outline.
- `reset_button_color`, `reset_button_hover_color`, `reset_button_spacing`: the ⋯ button at the end of each setting on Customize.
- `source_tag_custom`, `source_tag_inherited`: the tag naming where a value comes from (your file, or inherited from …).
- `source_tag_padding`, `source_tag_radius`, `source_tag_fill_opacity`, `source_tag_border_width`, `source_tag_font_size`: those tags' shape and text.
- `link_tag_color`: the violet "→ Small" tag on a setting that follows another.
- `follow_tag_color`: the green "13 follow" chip on a setting others follow.
- `own_value_tag_color`: the orange "own value" tag.
- `filter_banner_color`, `filter_banner_text_color`, `filter_banner_fill_opacity`, `filter_banner_chip_opacity`: the banner above the list while a filter is on, and its chips.
- `filter_banner_height`, `filter_banner_padding`, `filter_banner_spacing`, `filter_banner_radius`, `filter_banner_border_width`: its shape.

### Theme popup

The panel the theme button on Customize opens, with a column for colors and one for styles.

- `theme_popup_width`, `theme_popup_list_height`, `theme_popup_padding`, `theme_popup_column_spacing`: its size, inner space and the gap around the line between columns.
- `theme_popup_header_color`, `theme_popup_header_font_size`: the column headings.
- `theme_popup_row_spacing`, `theme_popup_row_radius`: the rows.
- `theme_popup_selected_background`, `theme_popup_selected_border`, `theme_popup_selected_border_width`: the row in use.
- `theme_popup_swatch_size`, `theme_popup_swatch_spacing`, `theme_popup_swatch_border_width`: the color dots in a colors row.
- `theme_popup_description_font_size`: the line under a style's name.
- `theme_popup_scene_height`, `theme_popup_scene_padding`, `theme_popup_scene_background`: a style's small preview scene.
- `theme_popup_button_height`: the New and Open file buttons.
- `theme_popup_fade_height`, `theme_popup_scrollbar_width`: the fade at the bottom of the lists and their scroll bar.

### Pages

Things on individual Settings pages.

- `small_button_size`, `small_icon_size`: small icon buttons in setting rows, shortcut rows and theme cards.
- `list_button_size`, `list_icon_size`: the move up, move down and remove buttons in text lists.
- `entry_icon_size`: each entry's icon in the Game page lists.
- `drag_opacity`, `drop_indicator`, `drop_indicator_width`: dragging rows on the Platforms or Game page, and the line where they'll land.
- `off_entry_opacity`: how faded a chip, button or info row that's turned off is on the Game page.
- `platform_columns_min_width`: the narrowest an expanded Platforms or Emulators row gets with its settings in two columns.
- `marker_color`, `marker_font_size`: the "found automatically" mark beside a setting.
- `key_chip_background`, `key_chip_border`, `key_chip_border_width`, `key_chip_text`, `key_chip_font_size`, `key_chip_height`, `key_chip_padding`, `key_chip_radius`: the keys on the Keyboard page (Ctrl+D).
- `key_chip_conflict`, `key_chip_recording`: a key another action also uses, and the outline while waiting for a new key.
- `preview_dot_radius`, `preview_dot_opacity`: the rail dots in the Appearance page's theme previews.
- `problems_button_size`, `problems_icon_size`: the buttons in the problems list.
- `install_found`, `install_missing`, `install_not_found`, `install_offer`: the tags on the Emulators page and in the setup guide (Flatpak, Not found, Install).
- `install_tile_radius`, `install_tile_padding`, `install_tile_button_background`: the boxes at the top of an emulator's card.
- `install_error`, `install_error_fill_opacity`: why an emulator couldn't be installed, and the tint behind it.
- `install_mini_progress_width`, `install_progress_height`, `install_progress_radius`, `install_progress_track`, `install_progress_fill`, `install_progress_slide_duration`: the progress bars while an emulator installs.
- `missing_description`, `missing_year`, `missing_rating`, `missing_art`: the colored tags on Metadata → Missing info.
- `picker_width`, `picker_art_size`: list dialogs like Steam's Add existing shortcuts, and the art beside each entry.

## Properties and other dialogs (`[game_editor]`)

The Properties dialog (and Add game), plus the other dialogs that share its look: questions, sign-in, backups, updates, the art browser, Detailed lookup and Look up missing info.

### Dialog

- `width`, `height`, `max_height`: the dialog's size. The height is the same on every tab; `max_height` is a share of the window.
- `install_width`: the dialog that installs a game's emulator.
- `padding`, `radius`, `column_spacing`: its inner space, roundness, and the gap between fields side by side.
- `background`, `background_end`, `plain_background`: the background behind the cards, and of dialogs without cards (a question, About).
- `border`, `border_width`: its outline.
- `scrim`: dims the window behind it.
- `heading_color`, `heading_font_size`: its title ("Properties").

### Tabs

The tab list on the left. Follows the Settings sidebar until set.

- `rail_width`, `rail_gap`, `rail_spacing`, `rail_padding`: its width, the gap to the content, between tabs, and inside its card.
- `rail_background`, `rail_background_end`, `rail_border`, `rail_border_width`, `rail_radius`: its card.
- `rail_item_height`, `rail_item_radius`, `rail_icon_size`, `rail_text`: each tab.
- `rail_selected_background`, `rail_selected_background_end`, `rail_selected_text`: the chosen tab.
- `rail_focus_ring`, `rail_focus_ring_width`: the tab reached with the keyboard.
- `changed_dot`, `changed_dot_selected`, `changed_dot_size`: the dot on a tab with unsaved changes.
- `details_column_spacing`: the gap between the Details card's columns (year, rating, favorite).

### Cards

- `card_background`, `card_background_end`, `gradient_direction`, `card_border`, `card_border_width`, `card_radius`, `card_padding`: the cards grouping related fields. Follow the Settings cards.
- `card_spacing`: the space between a card and the next card's title.
- `card_title_color`, `card_title_font_size`, `card_title_spacing`: the title above each card ("Images").
- `card_action_color`, `card_action_hover_color`, `card_action_font_size`: clickable text beside a title (Look up, New tag, Browse SteamGridDB).

### Fields

- `label_color`, `label_spacing`: field names and their gap to the field.
- `hint_color`, `hint_font_size`: notes under a field.
- `danger_color`: the mark and note on what an uninstall deletes for good.
- `divider`: the line above a dialog's closing note.
- `row_spacing`: the space between fields.
- `option_row_bleed`: how far the highlight of a row with its control at the right reaches past it.
- `description_height`: the description box.
- `media_popup_width`: the list of media folders that opens under its Edit button.
- `path_font_size`: the ROM path.
- `platform_field_width`, `platform_font_size`, `short_field_width`: the platform field, the platform name when it can't be changed, and short fields like the year.
- `number_field_padding`, `date_spacing`, `date_segment_focus`, `date_separator_color`: the play time and date boxes.
- `error_color`, `warning_color`: why a change wasn't saved, and warnings about a change.
- `marker_color`, `marker_font_size`: the "from IGDB" or "from Steam" mark.
- `help_color`, `help_hover_color`, `help_width`: the info icon beside a field's name and its explanation.
- `link_color`: links in dialogs.
- `read_only_opacity`: fields shown but not editable.
- `pending_color`: the line under a Steam game's Launch card while changes wait for Steam to close.
- `placeholder_icon_opacity`: the icon in an art thumbnail that's loading or failed.
- `check_size`, `check_on`, `check_mark`: check boxes in Look up missing info.
- `changed_color`: the "changed" mark on art you picked.

### Rating

- `star_size`, `star_spacing`, `star`, `star_empty`: the rating stars.
- `star_preview_opacity`: the stars a click would fill, while hovering.

### Tags

- `suggestion_fill_opacity`, `suggestion_hover_fill_opacity`: tags you can add, and under the mouse.
- `tag_hover_lift`: how far a tag rises under the mouse.
- `suggested_tag_dot`, `suggested_tag_dot_size`: the dot on a tag a lookup added.
- `add_tag_hover`, `tag_popup_width`, `tag_popup_list_height`: the Add tag chip and the list it opens.

### Art

- `art_spacing`: the gap between the hero, cover, logo and icon previews.
- `art_preview_background`: behind a preview, and where there's no art.
- `art_hover_overlay`, `art_hover_icon`: the darkening and folder icon on a preview under the mouse.
- `art_remove_hover_background`, `art_remove_icon`: the button that removes a kind of art.
- `checker_light`, `checker_dark`, `checker_size`: the checkerboard behind logos and icons.
- `selected_color`, `selected_width`: the outline of the picked image.

### Small dialogs

- `confirm_width`: questions before removing a game or restarting Steam.
- `sign_in_width`, `qr_size`, `qr_quiet_modules`, `qr_dark`, `qr_light`, `qr_radius`: the Steam sign-in window and its QR code. Keep dark on light for phones to read it.
- `uninstall_width`, `backup_width`: dialogs that show folder paths.
- `update_width`: the dialog that updates Lattis.
- `whats_new_width`, `whats_new_height_percent`: the What's new window.
- `update_notes_background`, `update_notes_text`, `update_notes_padding`, `update_notes_radius`: the release notes box.
- `progress_height`, `progress_radius`, `progress_track`, `progress_fill`: progress bars (a backup being made).
- `preset_spacing`: the gap between a Steam game's preset check boxes.

### Lookup

- `lookup_background`, `lookup_text`, `lookup_padding`, `lookup_radius`: the row under the title showing a lookup's progress.
- `pick_cover_height`, `pick_hover`: the list of games to pick from.

### Art browser

- `browser_width`, `browser_max_height`: its size.
- `browser_cover_height`, `browser_hero_height`, `browser_logo_height`, `browser_icon_size`: each kind of image.
- `browser_spacing`: the gap between images.

### Detailed lookup

- `detailed_width`, `detailed_max_height`, `detailed_details_width`: its size and the chosen game's details column.
- `detailed_cover_height`, `detailed_large_cover_height`: covers in the results and the chosen one.
- `detailed_selected`: the selected result.
- `detailed_busy_opacity`: the previous results while it searches again.

### Look up missing info

- `bulk_window_border`, `bulk_window_border_width`, `bulk_window_border_radius`: the window's outline.
- `bulk_sidebar_width`, `bulk_sidebar_selected`: the filters beside the games.
- `bulk_tag_popup_width`, `bulk_tag_list_height`, `bulk_popup_shadow_color`, `bulk_popup_shadow_size`: the tags list.
- `bulk_scrollbar_width`: the scroll bars.
- `bulk_row_padding`, `bulk_icon_size`: each game's row.
- `bulk_progress_track`: behind a lookup's progress bar.
- `bulk_progress_margin`: space above the progress text and bar.
- `bulk_state_matched`, `bulk_state_pick`, `bulk_state_not_found`, `bulk_state_error`, `bulk_state_not_on_igdb`, `bulk_badge_opacity`: the dot and badge for each result.
- `bulk_card_border_width`: the result cards' outline.
- `bulk_tray_background`, `bulk_tray_background_end`, `bulk_tray_darkness`: the darker tray under an open match.
- `bulk_match_cover_width`, `bulk_thumbnail_height`: the matched cover and result thumbnails.
- `bulk_art_height`, `bulk_art_icon_size`, `bulk_art_unchecked_opacity`, `bulk_art_unchecked_saturation`: the art preview and how unchecked pictures fade.
- `bulk_banner_background`, `bulk_banner_border`, `bulk_banner_icon`, `bulk_banner_text`: the note on an art-only result.
- `bulk_radio_border_width`: the radio buttons' ring.

## Setup guide (`[onboarding]`)

The card over a wall of covers shown on first start, and from the app menu.

### Window and card

- `background`, `background_end`: the base color behind the card.
- `card_width`, `card_height`, `card_padding`, `margin`: the card's size, inner space and distance from the window's edges.
- `large_screen_scale`: how much bigger the card is on 1440p and taller screens.
- `card_background`, `card_background_end`, `card_border`, `card_border_width`, `card_radius`: the card.
- `card_shadow_color`, `card_shadow_size`: its shadow.
- `panel_background`, `panel_background_end`, `panel_border`: panels inside the card (settings cards, choices, theme cards).
- `step_offset`: how far pages slide when the step changes.

### Backdrop

- `backdrop_motion`: scroll the cover wall behind the card.
- `backdrop_tint`, `backdrop_dim`, `backdrop_vignette`: the color over the covers, how strongly, and how much darker toward the edges.

### Steps

- `step_size`, `step_font_size`, `step_spacing`: the numbered circles in the step bar.
- `step_current`, `step_current_text`: the current step.
- `step_done`, `step_done_opacity`: a finished step.
- `step_border`, `step_border_width`: steps still ahead.
- `step_line`: the line joining them.
- `step_label_color`, `step_current_label_color`, `step_label_font_size`: the step names.

### Text and buttons

- `title_top_margin`, `title_color`, `title_size`: a step's title.
- `description_color`: the line under it.
- `spacing`, `control_spacing`: the gap between title, settings and cards, and between things side by side.
- `footer_divider`, `button_height`: the line above Skip setup, Back and Next, and their height.
- `skip_color`, `skip_hover_color`: Skip setup.
- `logo_height`, `logo_shadow_color`, `logo_shadow_size`: the wordmark on Welcome and its shadow.

### Welcome

- `welcome_tagline_size`, `welcome_tagline_color`, `welcome_tagline_spacing`, `welcome_tagline_width`: the line under the logo.
- `welcome_art_radius`, `welcome_art_tint`, `welcome_art_dim`, `welcome_art_vignette`: the panel of covers behind the logo.
- `welcome_logo_top`: the space above the logo.
- `welcome_shot_width`, `welcome_shot_radius`, `welcome_shot_fade`: the app screenshot.
- `welcome_heading_size`: "How do you want to start?".

### Choices

- `choice_padding`, `choice_title_size`, `choice_icon_size`: Welcome's two choices.
- `choice_selected_border`, `choice_icon`, `choice_icon_opacity`: the chosen one.
- `found_color`, `found_icon_size`: the check beside "Steam found".

### Themes

- `theme_card_padding`, `theme_rows`, `theme_preview_height`, `theme_preview_hero_tint`, `theme_name_size`: the theme cards on the Look step.

### Tiles

- `tile_background`, `tile_background_end`, `gradient_direction`, `tile_count_size`: the Done step's tiles of games found per platform.
- `scan_slide_duration`: the piece sliding along Done's bar while scanning.

## Play history (`[play_history]`)

The popup opened from a game's play time chip, listing its sessions.

- `width`, `max_height`: its size.
- `icon_size`: the game's icon beside its name. 0 hides it.
- `header_size`, `header_gap`: the column headers (Date, Time, Length).
- `summary_value_size`, `summary_value_color`, `summary_primary_color`: the numbers in the summary, and the total.
- `date_column_width`, `platform_column_width`, `duration_column_width`: the columns.
- `row_height`, `row_radius`, `row_hover`, `row_selected`: a session's row.
- `separator`: the line between sessions.
- `duration_color`, `note_color`, `manual_mark_color`: a session's length, a Steam date or the empty state, and the mark on a session added by hand.
- `delete_size`, `delete_icon_size`, `delete_color`, `delete_hover_color`: the × on the session under the mouse.

## Date picker (`[calendar]`)

The calendar that opens from a date field (Add session). Follows the popovers until set.

- `background`, `border`, `border_width`, `radius`, `padding`: its panel.
- `header_color`, `arrow_disabled_color`: the month, year and arrows, and an arrow that can't be used.
- `weekday_color`: the weekday initials.
- `cell_size`, `cell_radius`: each day.
- `day_color`, `outside_color`: days of this month and of the months around it.
- `disabled_color`, `disabled_opacity`: days that can't be picked.
- `hover_background`: the day under the mouse or keyboard.
- `today_ring`, `today_ring_width`: the ring around today.
- `picked_background`, `picked_text`: the date the field holds.

## About dialog (`[about]`)

- `width`, `side_padding`: the dialog's width, and the space between its sides and its content.
- `logo_height`, `logo_text`: the logo, centered at the top, and whether its "Lattis" is light or dark (`auto` picks from the background).
- `page_logo_height`: the logo on Settings → General.
- `logo_spacing`, `description_width`, `text_spacing`: the space below the logo, how wide the centered description gets, and the space around the line below it.
- `credits_indent`: how far the credits sit in from the lines above and below them.
- `label_color`, `label_width`, `label_spacing`, `row_spacing`: the credits: one list, its links lined up after a label column `label_width` wide.
- `wide_width`, `intro_width`, `column_spacing`, `intro_indent`, `wide_credits_indent`: where there is room (Settings → General), the logo and description sit in a column `intro_width` wide left of the credits, with a line between them; `wide_width` is the content width from which that happens. `intro_indent` moves the logo and description in from the left, and `wide_credits_indent` moves the credits further right of the line.
- `link_color`, `link_hover_color`, `link_underline`, `link_hover_underline`: a credit's link and the line under it, at rest and under the mouse.
- `separator_color`, `separator_spacing`: the dots between a credit's links.
- `divider`: the lines above the credits and above the footer.
- `footer_spacing`, `footer_padding`, `footer_font_size`, `footer_color`: the footer under the credits, with the version at the left and the note about covers at the right.
- `version_color`, `version_hover_color`: "Version 0.1" in the footer. Clicking it opens that version's release page.
