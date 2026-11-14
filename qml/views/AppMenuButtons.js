.pragma library

// The rail buttons the app menu has a row for, in the menu's names. A button not listed
// here (a new rail button, or toggle_view, which the menu draws as its own Grid view row)
// gets no generated row instead of a wrong icon and label.
const known = ["add_game", "overview", "play_statistics", "gallery", "open_settings"]

function shown(order) {
    return order.filter(name => known.includes(name))
}
