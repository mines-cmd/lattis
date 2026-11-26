import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window

// See docs/code/qml.md.
SettingsComboBox {
    id: root

    signal failed(string error)

    readonly property string matchTheme: "theme"
    readonly property string current: Theme.availableColors.includes(General.appIcon) ? General.appIcon : matchTheme
    readonly property var choices: [matchTheme].concat(Theme.availableColors)

    // The dogs are drawn at twice their size and scaled down: at 20 px the renderer would switch to
    // the bolder 16 px drawing, which the applications menu shows but the picker doesn't.
    function sourceSizeFor(size) {
        const side = size * 2 * Screen.devicePixelRatio
        return Qt.size(side, side)
    }
    function colorsFor(name) {
        if (name !== matchTheme)
            return Theme.dogColors(name)
        return {
            background: Theme.rail.dogBackground, backgroundEnd: Theme.rail.dogBackgroundEnd,
            coat: Theme.rail.dogCoat, coatEnd: Theme.rail.dogCoatEnd, details: Theme.rail.dogDetails
        }
    }
    function sourceFor(name) {
        const c = colorsFor(name)
        return "image://appicon/" + [c.background, c.backgroundEnd, c.coat, c.coatEnd, c.details]
            .map(color => String(color).substring(1)).join("/")
    }
    function nameFor(name) {
        return name === matchTheme ? qsTr("Match theme") : name
    }
    function pick(name) {
        picker.close()
        if (name === current)
            return
        const err = General.setAppIcon(name)
        if (err !== "")
            failed(err)
    }

    implicitWidth: Math.max(Theme.settingsWindow.appIconFieldWidth, implicitContentWidth + leftPadding + rightPadding)
    fieldBackground: popupShowing ? hoverBackground : Theme.controls.inputBackground
    model: choices
    currentIndex: choices.indexOf(current)
    displayText: nameFor(current)
    leadingSource: sourceFor(current)
    leadingSize: Theme.settingsWindow.appIconFieldIconSize
    leadingSourceSize: sourceSizeFor(leadingSize)
    onActivated: index => pick(choices[index])

    FontMetrics {
        id: nameMetrics
        font.pixelSize: Theme.settingsWindow.appIconNameFontSize
    }

    popup: MotionPopup {
        id: picker

        readonly property var s: Theme.settingsWindow
        readonly property int columns: Math.max(1, s.appIconColumns)
        readonly property int rows: Math.ceil(root.choices.length / columns)
        readonly property bool scrolls: rows > s.appIconMaxRows
        readonly property int edge: s.appIconSelectedBorderWidth
        readonly property real tileHeight: 2 * edge + s.appIconTilePaddingTop + s.appIconTileIconSize + s.appIconTileSpacing
                                           + Math.ceil(nameMetrics.height) + s.appIconTilePaddingBottom

        x: root.width - width
        y: root.height + Theme.layout.spacing / 2
        // Inside the outline, as the tiles keep their content inside theirs.
        padding: s.appIconPadding + Theme.popover.borderWidth
        background: PopoverBackground {}
        onAboutToShow: root.popupShowing = true
        onAboutToHide: root.popupShowing = false

        contentItem: Flickable {
            implicitWidth: grid.implicitWidth + (picker.scrolls ? bar.reserve : 0)
            implicitHeight: picker.scrolls
                            ? picker.s.appIconMaxRows * picker.tileHeight + (picker.s.appIconMaxRows - 1) * grid.spacing
                            : grid.implicitHeight
            contentHeight: grid.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: SettingsScrollBar { id: bar }

            Grid {
                id: grid
                columns: picker.columns
                spacing: picker.s.appIconSpacing

                Repeater {
                    model: picker.visible ? root.choices : []

                    Rectangle {
                        id: tile
                        required property string modelData
                        readonly property bool selected: modelData === root.current

                        width: picker.s.appIconTileWidth
                        height: picker.tileHeight
                        radius: picker.s.appIconTileRadius
                        color: hover.hovered ? picker.s.appIconTileHover : Qt.alpha(picker.s.appIconTileHover, 0)
                        border.color: selected ? picker.s.appIconSelectedBorder : Qt.alpha(picker.s.appIconSelectedBorder, 0)
                        border.width: picker.s.appIconSelectedBorderWidth

                        Behavior on color { enabled: Motion.fades; MotionColor {} }

                        Image {
                            y: picker.edge + picker.s.appIconTilePaddingTop
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: picker.s.appIconTileIconSize
                            height: picker.s.appIconTileIconSize
                            source: root.sourceFor(tile.modelData)
                            sourceSize: root.sourceSizeFor(picker.s.appIconTileIconSize)
                            smooth: true
                            mipmap: true
                        }
                        PlainLabel {
                            x: picker.edge + picker.s.appIconTilePaddingSide
                            y: picker.edge + picker.s.appIconTilePaddingTop + picker.s.appIconTileIconSize + picker.s.appIconTileSpacing
                            width: parent.width - 2 * x
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                            text: root.nameFor(tile.modelData)
                            color: Theme.controls.inputText
                            font.pixelSize: picker.s.appIconNameFontSize
                            font.weight: tile.selected ? Theme.font.strongWeight : Font.Normal
                        }

                        HoverHandler {
                            id: hover
                            cursorShape: Qt.PointingHandCursor
                        }
                        TapHandler { onTapped: root.pick(tile.modelData) }
                    }
                }
            }
        }
    }
}
