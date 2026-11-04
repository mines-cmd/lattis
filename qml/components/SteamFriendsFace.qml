import QtQuick
import QtQuick.Effects

// See docs/code/qml.md.
Item {
    id: root

    property bool hovered: false

    readonly property string status: SteamStatus.status
    readonly property string text: status === "online" ? qsTr("Steam friends: Online")
                                  : status === "away" ? qsTr("Steam friends: Away")
                                  : status === "invisible" ? qsTr("Steam friends: Invisible")
                                  : qsTr("Steam friends")

    readonly property real dot: Theme.toolbar.statusSize
    readonly property real ring: Math.min(Theme.toolbar.statusBorderWidth, dot / 2)
    readonly property real corner: Math.min(Theme.toolbar.radius, width / 2)
    // On the round picture's edge: at the box's corner the dot would float clear of a large picture.
    readonly property real dotCenter: width / 2 * (1 + Math.SQRT1_2)

    implicitWidth: Theme.toolbar.statusAvatarSize
    implicitHeight: Theme.toolbar.statusAvatarSize

    Rectangle {
        id: mask
        anchors.fill: parent
        visible: false
        layer.enabled: true
        color: "transparent"

        Rectangle {
            anchors.fill: parent
            radius: root.corner
        }
    }

    Image {
        id: picture
        anchors.fill: parent
        source: SteamStatus.avatar
        sourceSize: Qt.size(root.width * 2, root.height * 2)
        fillMode: Image.PreserveAspectCrop
        smooth: true
        visible: status === Image.Ready
        // Masked only once loaded: a layer made before the picture arrived could stay blank until redrawn.
        layer.enabled: status === Image.Ready
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: mask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: picture.visible
        radius: root.corner
        color: "transparent"
        border.color: Theme.toolbar.statusAvatarHover
        border.width: Theme.toolbar.statusBorderWidth
        opacity: root.hovered ? 1 : 0

        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
    }

    Icon {
        anchors.centerIn: parent
        visible: picture.status !== Image.Ready
        size: Theme.toolbar.iconSize
        path: Icons.users
        color: Theme.toolbar.icon
    }

    Rectangle {
        id: badge
        visible: root.status !== ""
        x: root.dotCenter - width / 2
        y: root.dotCenter - height / 2
        width: root.dot
        height: root.dot
        radius: width / 2
        color: Theme.toolbar.statusBorder

        Rectangle {
            anchors.fill: parent
            anchors.margins: root.ring
            radius: width / 2
            color: root.status === "online" ? Theme.toolbar.statusOnline
                   : root.status === "away" ? Theme.toolbar.statusAway
                   : Theme.toolbar.statusBorder
            border.color: Theme.toolbar.statusInvisible
            border.width: root.status === "invisible" ? Math.max(1, Math.round(width / 3)) : 0
        }
    }
}
