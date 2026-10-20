import QtQuick
import QtQuick.Controls.Basic

MotionPopup {
    id: root

    property int current: -1
    signal picked(int index)

    function openFor(anchorItem, colorIndex) {
        current = colorIndex
        parent = anchorItem
        y = anchorItem.height + Theme.layout.spacing / 2
        open()
    }

    padding: Theme.layout.spacing
    background: Rectangle {
        radius: Theme.settingsWindow.inputRadius
        color: Theme.settingsWindow.popupBackground
        border.color: Theme.settingsWindow.inputBorder
        border.width: Theme.settingsWindow.inputBorderWidth
    }

    contentItem: Grid {
        columns: Theme.settingsWindow.paletteColumns
        spacing: Theme.layout.spacing / 2

        Repeater {
            model: Theme.tags.palette

            Rectangle {
                id: swatch
                required property color modelData
                required property int index

                width: Theme.settingsWindow.swatchSize
                height: Theme.settingsWindow.swatchSize
                radius: Theme.settingsWindow.inputRadius
                color: modelData
                border.color: swatch.index === root.current || mouse.containsMouse ? Theme.settingsWindow.inputText
                                                                                   : Theme.settingsWindow.inputBorder
                border.width: Theme.settingsWindow.inputBorderWidth * 2

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.picked(swatch.index)
                        root.close()
                    }
                }
            }
        }
    }
}
