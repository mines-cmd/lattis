import QtQuick

// Behind the header (z below its parent) and the details, which are the row's later items. It
// fills the panel up to its outline, which then serves as the card's sides: the outline is
// translucent, and a second one drawn over it shows brighter.
Item {
    id: root

    required property Item row
    required property real progress
    required property bool firstInCard
    required property bool lastInCard
    property bool lit: false

    readonly property Item header: parent

    z: -1
    visible: progress > 0

    Rectangle {
        id: openCard
        readonly property real reach: Theme.settingsWindow.cardPadding - Theme.settingsWindow.cardBorderWidth
        readonly property real cornerRadius: Math.max(0, Theme.settingsWindow.cardRadius - Theme.settingsWindow.cardBorderWidth)
        opacity: root.progress
        x: -reach
        y: root.firstInCard ? -reach : 0
        width: root.header.width + 2 * reach
        height: root.row.height + (root.firstInCard ? reach : 0) + (root.lastInCard ? reach : 0)
        topLeftRadius: root.firstInCard ? cornerRadius : 0
        topRightRadius: topLeftRadius
        bottomLeftRadius: root.lastInCard ? cornerRadius : 0
        bottomRightRadius: bottomLeftRadius
        color: Theme.settingsWindow.openRowHeaderBackground
    }
    Rectangle {
        opacity: root.progress
        x: openCard.x
        y: root.header.height
        width: openCard.width
        height: Math.max(0, openCard.y + openCard.height - root.header.height)
        bottomLeftRadius: openCard.bottomLeftRadius
        bottomRightRadius: bottomLeftRadius
        color: Qt.darker(Theme.settingsWindow.openRowBackground, Theme.settingsWindow.openRowDarkness)
    }
    Rectangle {
        visible: root.lit
        x: openCard.x
        y: openCard.y
        width: openCard.width
        height: root.header.height - y
        topLeftRadius: openCard.topLeftRadius
        topRightRadius: topLeftRadius
        color: Theme.controls.hover
    }
    Repeater {
        // Top edge, the line under the header, bottom edge; the panel's outline is the edge for
        // the first and last rows.
        model: [root.firstInCard ? -1 : 0,
                root.header.height - Theme.settingsWindow.openRowBorderWidth,
                root.lastInCard ? -1 : root.row.height - Theme.settingsWindow.openRowBorderWidth]

        Rectangle {
            required property real modelData
            visible: modelData >= 0
            opacity: root.progress
            x: openCard.x
            y: modelData
            width: openCard.width
            height: Theme.settingsWindow.openRowBorderWidth
            // The panel's outline is drawn over the window, not its own fill, so a translucent
            // line over the card would come out a different shade than the outline it meets.
            color: Qt.tint(Theme.settingsWindow.background, Theme.settingsWindow.openRowBorder)
        }
    }
}
