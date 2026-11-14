import QtQuick
import QtTest
import "../../qml/views/AppMenuButtons.js" as AppMenuButtons

TestCase {
    name: "AppMenuButtons"

    function test_leaves_out_what_the_menu_draws_itself() {
        const order = ["steam_friends", "add_game", "overview", "play_statistics", "gallery", "toggle_view", "open_settings"]
        compare(AppMenuButtons.shown(order), ["add_game", "overview", "play_statistics", "gallery", "open_settings"])
    }

    function test_keeps_the_users_order() {
        compare(AppMenuButtons.shown(["open_settings", "gallery", "toggle_view", "add_game"]),
                ["open_settings", "gallery", "add_game"])
    }

    function test_a_button_it_does_not_know_gets_no_row() {
        compare(AppMenuButtons.shown(["add_game", "some_new_button"]), ["add_game"])
    }

    function test_one_row_per_known_button() {
        const rows = AppMenuButtons.shown(AppMenuButtons.known.concat(["toggle_view"]))
        compare(rows.length, AppMenuButtons.known.length)
        compare(new Set(rows).size, rows.length)
    }
}
