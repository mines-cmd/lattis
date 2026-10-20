import QtQuick

Rectangle {
    id: root

    property var preview: ({})

    readonly property real pad: Theme.settingsWindow.themePopupScenePadding
    readonly property real innerHeight: Theme.settingsWindow.themePopupSceneHeight
    readonly property bool container: preview.railContainer ?? true
    readonly property real railInset: Math.max(4, Math.round(innerHeight * 0.22))
    readonly property real railButtonSize: innerHeight - 2 * railInset

    implicitHeight: innerHeight + 2 * pad
    radius: Theme.settingsWindow.themePopupRowRadius
    color: Theme.settingsWindow.themePopupSceneBackground
    clip: true

    // The preview is smaller than the real thing, so a radius is scaled by the same ratio to keep its look.
    function capped(radius, size, realSize) {
        return Math.min((radius ?? 0) * size / (realSize || size), size / 2)
    }

    Rectangle {
        id: rail
        x: root.container ? root.pad : 0
        y: root.container ? root.pad : 0
        width: buttons.implicitWidth + 2 * root.railInset + (root.container ? 0 : root.pad)
        height: root.container ? root.innerHeight : root.height
        radius: root.container ? root.capped(root.preview.railContainerRadius, height, root.preview.railWidth) : 0
        color: root.preview.rail ?? "transparent"
        border.color: root.preview.border ?? "transparent"
        border.width: root.container ? (root.preview.railBorderWidth ?? 0) : 0

        Row {
            id: buttons
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: root.railInset
            spacing: root.railInset

            Repeater {
                model: 2

                Rectangle {
                    required property int index
                    width: root.railButtonSize
                    height: width
                    radius: root.capped(root.preview.railButtonRadius, width, root.preview.railButtonSize)
                    color: index === 0 ? (root.preview.railSelected ?? "transparent") : (root.preview.railHover ?? "transparent")
                }
            }
        }
    }

    Rectangle {
        id: launch
        anchors.right: parent.right
        anchors.rightMargin: root.pad
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(label.implicitWidth + 20, root.width / 3)
        height: root.innerHeight
        radius: root.capped(root.preview.launchRadius, height, root.preview.launchHeight)
        color: root.preview.launch ?? "transparent"
        border.color: root.preview.border ?? "transparent"
        border.width: root.preview.launchBorderWidth ?? 0

        Text {
            id: label
            anchors.centerIn: parent
            width: Math.min(implicitWidth, parent.width - 8)
            text: root.preview.launchLabel ?? ""
            color: root.preview.onAccent ?? "white"
            font.pixelSize: Theme.settingsWindow.themePopupDescriptionFontSize
            font.bold: true
            elide: Text.ElideRight
        }
    }

    Rectangle {
        anchors.left: rail.right
        anchors.leftMargin: root.pad
        anchors.right: launch.left
        anchors.rightMargin: root.pad
        anchors.verticalCenter: parent.verticalCenter
        height: root.innerHeight
        radius: root.capped(root.preview.searchRadius, height, root.preview.searchHeight)
        color: root.preview.surface ?? "transparent"
        border.color: root.preview.border ?? "transparent"
        border.width: root.preview.searchBorderWidth ?? 0

        Text {
            anchors.left: parent.left
            anchors.leftMargin: root.pad
            anchors.right: parent.right
            anchors.rightMargin: root.pad
            anchors.verticalCenter: parent.verticalCenter
            text: root.preview.searchPlaceholder ?? ""
            color: root.preview.placeholderColor ?? root.preview.textMuted ?? "transparent"
            font.pixelSize: Theme.settingsWindow.themePopupDescriptionFontSize
            elide: Text.ElideRight
        }
    }
}
