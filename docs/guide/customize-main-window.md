# Customize: the main window

Part of the [Customize reference](customize.md). Everything around a game's page: the blurred art behind the window, the rail on the left, the search box and filters, the game list, the Grid view, the toolbar at the top, and the banners and messages that pop up. Listed from back to front and left to right, the way you see them.

## Blurred art behind the window (`[backdrop]`)

A blurred, dimmed copy of the selected game's banner behind the window, fading into the background toward the bottom. The Grid view has its own copy of the strength and reach settings, under [Grid view](#backdrop-in-grid-view).

- `enabled`: turns the backdrop on or off.
- `placeholder`: for games without banner art, show a soft color gradient instead of nothing.
- `area`: cover the whole window, or only the area right of the game list.
- `height`: how far down it reaches, as a fraction of the window's height.
- `fade_start`, `fade_curve`: where the fade into the background begins (as a fraction of the backdrop's height) and its shape; `smooth` has no visible edge.
- `dither`: fine noise in the fade that hides banding near black. 0 turns it off.
- `opacity`: how strongly the art shows. Lower is darker and subtler; this is the setting to change when the backdrop is too bright.
- `blur`, `blur_max`: how blurred it is (0–1), and the blur radius `blur = 1` means.
- `saturation`: how colorful it is; −1 is greyscale.
- `decode_size`: the width the art is loaded at. Small is faster and already soft; raise it only with little blur.
- `fade_duration`: the crossfade between games, in milliseconds. 0 switches at once.

## The rail (`[rail]`)

The narrow bar on the far left with the logo and the navigation icons.

### Bar

- `container`: draw the rail as a floating rounded panel, or as a plain full-height bar.
- `width`: the rail's width.
- `container_margin`, `container_radius`: the space around the floating panel and its roundness.
- `background`, `background_end`, `gradient_direction`: the rail's fill.
- `border`, `border_width`: the floating panel's outline.

### Logo

- `show_logo`: show the app logo at the top of the rail.
- `logo_style`: what the logo shows (here and on the setup guide's Welcome step): the app icon, the app's mark drawn in `logo_mark` on `logo_background`, or `logo_text`.
- `logo_mark`, `logo_mark_size`: the mark's color and its size as a percentage of the logo (mark style).
- `logo_text`, `logo_text_color`: the letters shown, and their color (text style).
- `logo_size`, `logo_radius`: the logo's size and roundness. The rail buttons follow the size.
- `logo_background`, `logo_border`, `logo_border_width`: the square behind the mark or text, and its outline. The app icon has its own shape and ignores these.
- `logo_hover`: the wash over the logo under the mouse or while its menu is open.
- `update_dot`, `update_dot_size`: the dot on the logo when a newer version of Lattis (or one of your emulators) is waiting, and its size.
- `update_dot_corner`, `update_dot_margin`: which corner it sits in, and how far in from the edges (negative pushes it out past the edge).
- `update_dot_border`, `update_dot_border_width`: the ring around that dot, so it reads on any logo.

### Buttons

- `button_size`, `button_radius`: the square behind each icon (seen on hover and when selected) and its roundness.
- `icon_size`: the icons' size.
- `icon`, `icon_hover`: the icons' color, and under the mouse.
- `hover_background`: the square under the mouse.
- `selected_background`, `selected_background_end`, `selected_icon`: the selected platform or page's square and its icon. Accent colored by default.
- `status_avatar_size`, `status_avatar_hover`: the Steam friends button's profile picture when it's on the rail, and the ring around it under the mouse.

### Dragging

- `drag_hold`: how long you hold a platform button still before you can drag it to a new place. 0 drags at once.
- `drag_opacity`, `drag_scale`: how faint and how big a held or dragged button is.

## The toolbar (`[toolbar]`)

The row of round buttons at the top, centered on the window (List/Grid, Overview, Gallery, Play statistics, friends…). Which buttons show is in Settings → General.

- `margin_top`: the distance from the top of the window.
- `size`, `icon_size`, `spacing`, `radius`: the buttons' size, their icons, the gap between them and their roundness.
- `icon`: the icons' color.
- `hover_background`: a button under the mouse.
- `checked_background`, `checked_background_end`, `checked_icon`: the button of the view that's open.
- `background`, `border`, `padding`, `background_radius`: the pill behind the buttons. Make `background` transparent for floating buttons.
- `drag_opacity`, `drag_scale`: a button being dragged to a new place.
- `status_avatar_size`, `status_avatar_hover`: the Steam friends button's profile picture, and the ring around it under the mouse.
- `status_size`, `status_border`, `status_border_width`: the status dot on that picture, and the ring that cuts it out of the picture.
- `status_online`, `status_away`, `status_invisible`: the dot's color when online, away, and the hollow ring when invisible.

## Search box (`[search]`)

The search box above the game list, and how its neighbours sit.

- `show_filter_button`: show the filter button beside the search box.
- `show_filter_chips`: show the filters that are on as chips under the search box. The filter button gets a dot either way.
- `joined_buttons`: draw the filter and collections buttons inside the search box's background instead of as separate buttons.
- `button_spacing`: the gap between the search box and those buttons, and between the buttons.
- `placeholder`, `placeholder_color`: the hint shown while it's empty ("Search").
- `height`, `radius`: its height and roundness.
- `icon`, `icon_size`: the magnifier.
- `background`, `background_end`, `gradient_direction`: its fill.
- `hover_tint`: brightens the box under the mouse. Transparent = no hover effect.
- `border`, `border_width`: its outline at rest.
- `border_focus`, `border_width_focus`: its outline while you type.
- `text`: what you type.

## Filter button (`[filter_button]`)

The funnel button beside the search box. It copies the search box until you change it here.

- `size`, `radius`: its size and roundness.
- `icon`, `icon_size`: the funnel.
- `background`, `background_end`, `gradient_direction`, `hover_tint`: its fill and the brightening under the mouse.
- `border`, `border_width`: its outline.
- `badge`: the dot on it while a filter is on.

## Filter panel and filter chips (`[filters]`)

The panel the filter button opens (sort, group by, show, tags, More filters) and the chips under the search box naming the filters that are on.

### Panel

- `panel_width`: the panel's width.
- `panel_width_rail_hidden`: its width while the rail is hidden, when it has an extra Platforms section.
- `padding`, `spacing`: the space inside it and between its sections.
- `background`, `background_end`, `radius`, `border`, `border_width`: the panel itself. Follows `[popover]`.
- `focus_ring`, `focus_ring_width`: the outline around the control you reach with the keyboard.

### Headings and text

- `heading_color`, `heading_font_size`, `heading_uppercase`: the section headings (SORT, GROUP BY, SHOW, TAGS).
- `platforms_alphabetical`: list the Platforms section alphabetically instead of in your library's order.
- `text`: the panel's ordinary text.
- `hint_color`: hints, like the line shown when there are no tags.
- `disabled_opacity`: how faint a section that doesn't apply is (Group by in Grid view).

### Tags

- `tag_off_opacity`: how strongly tags that aren't picked are filled with their color. Picked tags are fully filled.
- `tag_text_lightness`: how light the text of unpicked tags is, the same for every tag whatever its color.
- `tag_hover_opacity`, `tag_hover_lift`: an unpicked tag under the mouse: how much it fills and how far it rises.
- `tag_count_opacity`: how strongly the number of games shows beside a tag's name.
- `tag_spacing`: the gap between tags.
- `tag_border_width`: the outline of unpicked tags.
- `tag_search_height`: the height of the search field above the tags.

### Sort buttons

The Sort and Group by choices. The Collections popup's rows follow these.

- `sort_height`, `sort_radius`: their height and roundness.
- `sort_background`, `sort_hover`: a choice at rest and under the mouse.
- `sort_selected`, `sort_selected_text`: the chosen one and its text.

### Active filter chips

The chips under the search box, one per filter that's on.

- `chip_height`, `chip_padding`, `chip_radius`, `chip_spacing`: their height, inner space either side, roundness and the gap between them.
- `chip_font_size`, `chip_text`: their text. Tag chips use the tag's own text color.
- `chip_background`, `chip_background_end`, `gradient_direction`: their fill. Tag chips use the tag's color.
- `chip_hover_brightness`: how much brighter a chip gets under the mouse. 1 = no change.

### More filters

The wider panel with the year and rating charts.

- `more_width`, `column_gap`: the panel's width while More filters is open, and the gap between its two columns.
- `histogram_height`: the height of the games-per-year and rating charts.
- `histogram_bar`, `histogram_bar_selected`: bars outside and inside the range you picked.
- `histogram_bar_radius`: the bars' roundness.
- `histogram_empty_opacity`: how strongly the little stub for a year or rating with no games shows.
- `slider_track`, `slider_fill`, `slider_handle`, `slider_handle_size`: the range sliders under the charts.

## Collections (`[collections]`)

The popup the collections button opens, the banner above the list while a collection is open, and the Edit collection and Import from Steam dialogs.

### Popup

- `popup_width`: the popup's width.
- `list_max_height`: how tall the list of collections grows before it scrolls.
- `row_height`, `row_radius`, `row_spacing`: each collection's height, roundness and the gap between them.
- `row_background`, `row_hover`: a collection at rest and under the mouse.
- `row_selected`, `row_selected_text`: the collection that's open.
- `row_text`, `row_count_color`: collection names and the number of games beside them.

### Edit collection dialog

- `edit_width`: the dialog's width.
- `edit_list_height`: how tall the lists of added and left-out games grow before they scroll.
- `edit_label_width`: the width of the column of rule names (Platform, Status, Rating).
- `edit_year_width`: the width of the release year fields.
- `edit_section_gap`: the space between the Library, Progress and Details groups of rules.
- `edit_section_heading`, `edit_section_heading_opacity`, `edit_section_rule`: those groups' headings and the line beside them.
- `edit_tab_text`, `edit_tab_active_text`: the tab names (Rules, Added by hand, Left out), and the open tab's.
- `edit_tab_count_opacity`: how strongly the number after a tab name shows.
- `edit_tab_indicator`, `edit_tab_indicator_height`: the line under the open tab.
- `edit_footer_background`, `edit_footer_text`: the footer with the match count and buttons.

The rule chips are the filter panel's tags and toggles, so `[filters]` styles them.

### Import from Steam dialog

- `import_list_height`: how tall the lists of Steam's collections and of games not in your library grow before they scroll.

### Lists of checks

Lists of rows with a check box each: Steam's collections, a collection's games, a mod manager's games.

- `checklist_row_height`, `checklist_row_padding`: each row's height, and the space before its check box and after its note.
- `checklist_padding`: the space around the rows inside the list's box.
- `checklist_count_color`: the count line above the list ("12 games · 3 selected").

### Banner above the list

- `banner_height`, `banner_radius`, `banner_background`: the banner naming the open collection. Follows the list's selection bar.
- `banner_text`, `banner_count_color`: the collection's name and its number of games.
- `banner_spacing`: the gap between the banner and the list.

## Game list (`[list]`)

The list of games on the left in List view.

### Layout

- `width`: the list's width. Change this to give the game page more or less room.
- `padding`: the space left and right of the rows.
- `rail_margin`, `rail_top_margin`: the space left of the list and above it while the rail shows.
- `rail_hidden_margin`, `rail_hidden_top_margin`: the same while the rail is hidden.
- `edge_fade`: the fade where rows leave at the top and bottom as you scroll. 0 cuts them off sharply.
- `background`: the list's background. Transparent lets the backdrop show through.

### Rows

- `row_height`, `row_radius`: each row's height and roundness. Lower `row_height` fits more games.
- `row_hover`, `row_selected`: the wash over a row under the mouse, and over the selected row.
- `hover_spring`, `hover_damping`: how closely the hover highlight follows the pointer from row to row, and how much it bounces.
- `text`: game titles.
- `selected_bold`: the selected game's title in bold.
- `title_elide`: where a title too long for the list is cut: at the end, in the middle or at the start. Hover a cut title to see it whole.
- `uninstalled_opacity`: how faint games that aren't installed are.
- `show_update_icon`, `update_icon_color`: the icon on games with a Steam update waiting.
- `running_label`, `running_color`, `running_title`: the "Running" label on a game that's running, its color, and whether the title takes that color too. Empty label = none.
- `link_color`: the "Show" link on the line that says how many games are left out.

### Icons

- `show_icons`, `icon_size`, `icon_radius`: the small game icons left of each title.
- `icon_placeholder_saturation`, `icon_placeholder_lightness`, `icon_placeholder_text`: the colored square with a letter shown for a game without icon art. Its hue is the game's own; these set how vivid and light it is, and the letter's color.

### Groups

The platform (or other) groups when the list is grouped.

- `header_text`, `header_hover_text`, `header_font_size`, `header_uppercase`: the group headers.
- `show_counts`: show the number of games beside each header.
- `group_gap`: extra space above each header after the first.
- `fold_duration`: how long a group as tall as the list takes to fold or unfold (smaller groups are quicker). 0 = at once.
- `change_slide`: how far a row slides in when a game is added and out when one is removed. 0 = it only fades.

### Sort and counts

- `show_sort_value`, `sort_value_color`, `sort_value_font_size`: when sorted by last played or play time, show that value at the right of each row.
- `show_filtered_count`, `filtered_count_position`, `filtered_count_color`: the line saying how many games Installed only leaves out, with a button to show them: whether it shows, whether it sits right after the last game or at the bottom, and its color.

### Scroll and selection bars

- `scrollbar_handle`, `scrollbar_handle_hover`: the list's scroll bar handle.
- `selection_bar_background`, `selection_bar_text`: the bar under the list that says how many games are selected. The collection banner follows it.

## Grid view (`[grid]`)

The wall of covers, with the selected game's logo, Launch button and facts in a side panel.

### Covers

- `gap`, `margin`: the space between covers, and between the covers and the edges of the area.
- `radius`: the covers' roundness.
- `min_cover_width`, `max_cover_width`: the smallest and largest the size slider makes a cover.
- `edge_fade`, `edge_fade_top`: the fade where covers leave at the bottom while scrolling, and whether the top fades too.
- `title_size`: the title under each cover, when Settings shows titles there.

### Selection and hover

- `ring_color`, `ring_width`, `ring_gap`: the ring around the current game's cover, and the gap between ring and cover.
- `selected_opacity`: how strongly that ring shows on the other selected covers.
- `hover_overlay`, `hover_overlay_end`, `hover_gradient_direction`: the wash over the cover under the mouse.
- `hover_zoom`: how far the cover's picture grows under the mouse. 1 = no zoom.

### Badges

What's drawn on top of a cover.

- `badge_size`, `badge_background`, `badge_icon_color`, `badge_margin`: the round badges at the top right (favorite, update, running): size, background, icon and distance from the edges.
- `play_button`: a round Play button on the cover under the mouse that launches at once: `off`, `center` or `corner`.
- `play_button_size`, `play_button_margin`: its size, and its distance from the right and bottom edges in the corner.
- `play_button_background`, `play_button_background_end`, `play_button_icon_color`: its colors. Follows the Launch button.
- `play_button_label`, `play_button_font_size`: text beside its icon, which turns it into a pill. Empty = icon only.
- `platform_icon`, `platform_icon_size`, `platform_icon_margin`: the platform's icon at the top left: `off`, on `hover`, or `always`, its size and its distance from the edges.

### Side panel

- `panel_width`: the side panel's width. Wider panels fit the chips in three columns.
- `panel_margin`, `panel_padding`, `panel_spacing`: the space around it, inside it, and between its parts.
- `panel_background`, `panel_border`, `panel_border_width`, `panel_radius`: the panel itself. Transparent background lets the backdrop through.
- `panel_edge_fade`: the fade where its content leaves while scrolling.
- `panel_logo_height`: the tallest the game's logo gets.
- `panel_logo_min_height`: a height always kept for the logo so the Launch button doesn't jump between games. 0 = follow the logo.
- `panel_logo_top_gap`, `panel_logo_gap`: extra space above the logo, and the space between it and the Launch button.
- `panel_buttons_gap`: extra space under the Properties and More buttons.
- `card_opacity`: how solid the panel's cards are (chips, info, Friends, Achievements). 0 lets the backdrop through.
- `card_background`, `card_background_end`, `card_border`, `card_border_width`: those cards' look, in the side panel only. Follows the game page's info panel.

### Backdrop in Grid view

Grid view's own blurred backdrop; it reaches further and is a little stronger than List view's.

- `backdrop_enabled`: show it.
- `backdrop_height`, `backdrop_fade_start`: how far down it reaches and where it starts fading.
- `backdrop_opacity`, `backdrop_blur`, `backdrop_saturation`: its strength, blur and color. Blur and saturation follow `[backdrop]` until set.

### Side panel Launch button

Follows the game page's Launch button until you set these.

- `launch_label`: its label while it launches the game. Empty = the Launch button's label.
- `launch_height`, `launch_radius`, `launch_border`, `launch_border_width`: its size, roundness and outline.
- `launch_background`, `launch_background_end`, `launch_text`: its colors while it launches. Install, update and running keep the Launch button's colors.
- `launch_font_size`, `launch_bold`: its label's size and weight.

### Side panel Properties and More buttons

- `show_buttons`: show Properties and More under Launch. Off, Properties becomes a text button opposite Open game page, and the game menu stays on right-click.
- `properties_label`, `more_label`: their labels (More is "Actions" by default).
- `button_height`, `button_radius`, `button_border`, `button_border_width`: their size, roundness and outline.
- `button_background`, `button_text`, `button_font_size`: their fill and label.

### Side panel chips

The play time, last played and other facts.

- `chip_style`: `strip` puts them in one card split by thin lines, value first; `cards` gives each its own card; `bare` shows them without a card.
- `chip_background`, `chip_border`, `chip_border_width`: the card behind them in strip or cards style.
- `strip_value_font_size`, `strip_value_color`: the value (491.1 hours).
- `strip_label_font_size`, `strip_label_color`, `strip_uppercase_labels`: the label under it (Play time).
- `strip_hover_value_color`, `strip_hover_label_color`: both under the mouse, on a chip that opens something.
- `strip_icons`, `strip_icon_size`: the chips' icons.
- `strip_padding`: the space inside each chip. Smaller lets three fit in a row sooner.
- `strip_divider`, `strip_divider_width`: the lines between chips.

### Size slider

- `slider_width`: the length of the cover size slider at the bottom.
- `pill_button_size`: the − and + buttons beside it; also sets the height of that pill and of the selection bar.

## Banners right of the toolbar (`[notice]`)

Lasting notices beside the toolbar: a read-only library, Steam changes waiting, what an Import moved aside. Green ones tell you something happened; yellow ones mean something is blocked until you act.

### Look

- `background`, `background_end`, `gradient_direction`: the color under the tint.
- `tint_opacity`: how strongly the tint colors that background.
- `tint`, `border`, `text`, `icon`, `action_text`: a green (information) notice: its tint, outline, message, icon and text buttons (Show).
- `warning_tint`, `warning_border`, `warning_text`, `warning_icon`, `warning_action_text`: the same for a yellow (warning) notice (Show, Close Steam, Close Heroic).
- `danger_text`: a button that removes something (Delete), in either kind.
- `border_width`, `radius`, `padding`: the outline's thickness, roundness and inner space.
- `icon_size`, `close_icon_size`: the notice's icon, and the × that hides it.
- `button_height`: the height of its buttons.
- `chip`, `chip_opacity`, `chip_text`, `chip_padding`: the pill behind a file name in a message.

### Stacking

With more than one, they stack, the most urgent in front.

- `peek_count`: how many edges of the notices behind show (0–4).
- `peek_offset`, `peek_scale`, `peek_opacity`: how much lower, smaller and fainter each one behind is.
- `count`, `warning_count`, `count_opacity`: the pill with the number of notices on the front one.
- `count_text`, `count_size`: the number itself.
- `fan_close_delay`, `fan_stagger`: hovering fans the stack out. How long it waits after the pointer leaves before folding back, and how long each waits for the one in front.
- `enter_offset`: how far above its place a new notice starts.
- `leave_offset`, `dismiss_duration`: how far a dismissed notice rises as it fades, and how long it takes.

## Short messages at the bottom (`[toast]`)

The messages that pop up briefly at the bottom of the window ("Copied", "Install Azahar to play…").

- `background`, `background_end`, `gradient_direction`: the toast's fill.
- `border`, `border_width`, `radius`: its outline and roundness.
- `text`: its message.
- `link`, `link_hover`: a link after the message ("Emulators"), and under the mouse.
- `duration`: how many milliseconds it stays.
