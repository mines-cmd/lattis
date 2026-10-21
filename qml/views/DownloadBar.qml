import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Surface {
    id: root

    property var download: ({})

    readonly property bool paused: download.state === "paused"
    readonly property int percent: download.percent ?? -1

    readonly property real rowCenter: Theme.downloadBar.padding
        + (Theme.downloadBar.height - 2 * Theme.downloadBar.padding - column.implicitHeight) / 2
        + topRow.implicitHeight / 2

    property bool morphing: false
    property real rowFromBottom: Theme.downloadBar.height - rowCenter

    readonly property bool chevronHovered: chevronArea.containsMouse
    property bool carriedHover: false
    readonly property bool hot: chevronHovered || carriedHover

    property bool tooltips: true

    signal message(string text)
    signal collapse()

    radius: Theme.downloadBar.radius
    // The pill sits right under the bar during the morph and draws the percent and chevron itself;
    // drawing both would stack their translucent colors and brighten them.
    fill: morphing ? "transparent" : Theme.downloadBar.background
    fillEnd: morphing ? "transparent" : Theme.downloadBar.backgroundEnd
    direction: Theme.downloadBar.gradientDirection
    border.color: Theme.downloadBar.border
    border.width: morphing ? 0 : Theme.downloadBar.borderWidth

    // Laid out at the bar's full height whatever the size of the card, placed so the percent and chevron
    // row sits `rowFromBottom` above the card's bottom edge.
    RowLayout {
        z: 1
        x: Theme.downloadBar.padding
        y: root.height - root.rowFromBottom - root.rowCenter + Theme.downloadBar.padding
        width: root.width - 2 * Theme.downloadBar.padding
        height: Theme.downloadBar.height - 2 * Theme.downloadBar.padding
        spacing: Theme.layout.spacing

        Item {
            visible: Theme.downloadBar.iconSize > 0
            Layout.preferredWidth: Theme.downloadBar.iconSize
            Layout.preferredHeight: Theme.downloadBar.iconSize
            Layout.alignment: Qt.AlignVCenter

            Image {
                id: gameIcon
                anchors.fill: parent
                visible: status === Image.Ready
                source: root.download.icon ?? ""
                sourceSize.width: width
                sourceSize.height: height
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }
            Icon {
                anchors.centerIn: parent
                visible: !gameIcon.visible
                path: Icons.steam
                size: Theme.downloadBar.iconSize
                color: Theme.downloadBar.text
            }
        }

        ColumnLayout {
            id: column
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.layout.spacing

            RowLayout {
                id: topRow
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Label {
                    text: root.paused ? qsTr("Paused") : root.download.installing ? qsTr("Installing") : qsTr("Updating")
                    color: Theme.downloadBar.text
                }
                Label {
                    Layout.fillWidth: true
                    text: root.download.name ?? ""
                    elide: Text.ElideRight
                    color: Theme.downloadBar.title
                    font.weight: Theme.font.emphasisWeight
                }
                Label {
                    id: percentLabel
                    opacity: root.morphing ? 0 : 1
                    visible: root.percent >= 0
                    text: root.percent + "%"
                    color: Theme.downloadBar.text
                }

                Item {
                    id: chevronItem
                    visible: Theme.downloadBar.chevronSize > 0
                    Layout.preferredWidth: Theme.downloadBar.chevronSize
                    Layout.preferredHeight: Theme.downloadBar.chevronSize

                    Icon {
                        anchors.centerIn: parent
                        path: Icons.chevron
                        size: Theme.downloadBar.chevronSize
                        opacity: root.morphing ? 0 : 1
                        rotation: 90
                        color: hot ? Theme.downloadBar.chevronHover : Theme.downloadBar.text
                    }
                }
            }

            Rectangle {
                id: track
                Layout.fillWidth: true
                implicitHeight: Theme.downloadBar.trackHeight
                radius: Theme.downloadBar.trackRadius
                color: Theme.downloadBar.track

                Rectangle {
                    width: root.percent < 0 ? 0 : track.width * root.percent / 100
                    height: parent.height
                    radius: parent.radius
                    color: root.paused ? Theme.downloadBar.pausedFill : Theme.downloadBar.fill

                    Behavior on width {
                        enabled: Motion.on
                        MotionNumber { duration: Motion.duration(Theme.downloadBar.fillDuration) }
                    }
                }
            }
        }
    }

    MouseArea {
        id: chevronArea
        z: 2
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: (chevronItem.visible ? chevronItem.width : 0)
            + (percentLabel.visible ? percentLabel.width + Theme.layout.spacing : 0)
            + Theme.downloadBar.padding + Theme.layout.spacing
        cursorShape: Qt.PointingHandCursor
        hoverEnabled: true
        onClicked: root.collapse()

        ToolTip.visible: containsMouse && root.tooltips
        ToolTip.text: qsTr("Hide the download bar")
        ToolTip.delay: 500
    }

    MouseArea {
        id: area
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        hoverEnabled: true
        onClicked: {
            const error = GameRunner.update(root.download.gameId)
            if (error !== "")
                root.message(error)
        }
    }
    ToolTip.visible: area.containsMouse && !chevronArea.containsMouse && root.tooltips
    ToolTip.text: qsTr("Open Steam's downloads")
    ToolTip.delay: 500
}
