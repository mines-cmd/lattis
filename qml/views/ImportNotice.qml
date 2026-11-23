import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root

    signal message(string text)

    readonly property var current: Backups.notice
    readonly property bool showing: current.text !== undefined
    readonly property bool menuOpen: confirm.visible
    property int count: 1
    property bool peeking: false
    property var notice: ({})
    onCurrentChanged: if (current.text !== undefined) notice = current
    Component.onCompleted: if (current.text !== undefined) notice = current

    NoticeBar {
        id: bar
        anchors.fill: parent
        enabled: root.showing && !root.peeking
        count: root.count
        peeking: root.peeking
        text: root.notice.text ?? ""
        parts: root.notice.parts ?? []

        SettingsButton {
            visible: root.notice.folder?.toString() !== ""
            Layout.preferredHeight: Theme.notice.buttonHeight
            text: qsTr("Show")
            flat: true
            leftPadding: Theme.layout.spacing
            rightPadding: Theme.layout.spacing
            textColor: Theme.notice.actionText
            font.weight: Theme.font.emphasisWeight
            onClicked: GameRunner.openUrl(root.notice.folder)
        }
        SettingsButton {
            visible: root.notice.canDelete === true
            Layout.preferredHeight: Theme.notice.buttonHeight
            text: qsTr("Delete")
            flat: true
            leftPadding: Theme.layout.spacing
            rightPadding: Theme.layout.spacing
            textColor: Theme.notice.dangerText
            font.weight: Theme.font.emphasisWeight
            onClicked: confirm.open()
        }
        IconButton {
            Layout.preferredWidth: Theme.notice.buttonHeight
            Layout.preferredHeight: Theme.notice.buttonHeight
            plain: true
            iconSize: Theme.notice.closeIconSize
            icon: Icons.close
            tip: qsTr("Hide this")
            onClicked: Backups.dismissNotice()
        }
    }

    MotionPopup {
        id: confirm

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(Theme.gameEditor.confirmWidth, parent.width - 2 * Theme.gameEditor.padding)
        modal: true
        focus: true
        padding: Theme.gameEditor.padding
        Overlay.modal: Scrim {}
        background: Surface {
            radius: Theme.gameEditor.radius
            fill: Theme.gameEditor.background
            fillEnd: Theme.gameEditor.backgroundEnd
            direction: Theme.gameEditor.gradientDirection
            border.color: Theme.gameEditor.border
            border.width: Theme.gameEditor.borderWidth
        }

        onOpened: cancelButton.forceActiveFocus()

        contentItem: ColumnLayout {
            spacing: Theme.gameEditor.rowSpacing

            Label {
                Layout.fillWidth: true
                text: qsTr("Move %1 to the trash?").arg(root.notice.places ?? "")
                color: Theme.gameEditor.headingColor
                font.weight: Theme.font.strongWeight
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.gameEditor.labelColor
                text: qsTr("What you had before the import. What was restored stays.")
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Item { Layout.fillWidth: true }
                SettingsButton {
                    id: cancelButton
                    text: qsTr("Cancel")
                    onClicked: confirm.close()
                }
                SettingsButton {
                    text: qsTr("Move to trash")
                    danger: true
                    onClicked: {
                        confirm.close()
                        const e = Backups.deleteNoticed()
                        if (e !== "")
                            root.message(e)
                    }
                }
            }
        }
    }
}
