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
        radius: Theme.controls.inputRadius
        color: Theme.menu.popupBackground
        border.color: Theme.controls.inputBorder
        border.width: Theme.controls.inputBorderWidth
    }

    contentItem: Grid {
        columns: Theme.colorPicker.paletteColumns
        spacing: Theme.layout.spacing / 2

        Repeater {
            model: Theme.tags.palette

            Rectangle {
                id: swatch
                required property color modelData
                required property int index

                width: Theme.colorPicker.swatchSize
                height: Theme.colorPicker.swatchSize
                radius: Theme.controls.inputRadius
                color: modelData
                border.color: swatch.index === root.current || mouse.containsMouse ? Theme.controls.inputText
                                                                                   : Theme.controls.inputBorder
                border.width: Theme.controls.inputBorderWidth * 2

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
