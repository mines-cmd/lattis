import QtQuick

Row {
    id: root

    property real value: -1
    property real preview: -1
    property real previewOpacity: 1
    property int starSize: 16
    property color color
    property color emptyColor

    Repeater {
        model: 5

        Item {
            id: star
            required property int index

            width: root.starSize
            height: root.starSize

            Icon {
                path: Icons.star
                size: parent.width
                color: root.emptyColor
            }
            Item {
                width: parent.width * Math.max(0, Math.min(1, root.preview - star.index))
                height: parent.height
                clip: true
                opacity: root.previewOpacity

                Icon {
                    path: Icons.star
                    size: star.width
                    color: root.color
                }
            }
            Item {
                width: parent.width * Math.max(0, Math.min(1, root.value - star.index))
                height: parent.height
                clip: true

                Icon {
                    path: Icons.star
                    size: star.width
                    color: root.color
                }
            }
        }
    }
}
