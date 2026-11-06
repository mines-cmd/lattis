import QtQuick

// See docs/code/qml.md.
SteamFriendsFace {
    size: Theme.rail.statusAvatarSize
    buttonRadius: Theme.rail.buttonRadius
    iconSize: Theme.rail.iconSize
    iconColor: hovered ? Theme.rail.iconHover : Theme.rail.icon
    ringColor: Theme.rail.statusAvatarHover
}
