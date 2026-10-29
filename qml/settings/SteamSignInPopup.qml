import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes

// See docs/code/qml.md.
MotionPopup {
    id: root

    property bool scrim: false
    readonly property string auth: SteamLibrary.authState
    readonly property int modules: SteamLibrary.qrSize
    readonly property int quiet: Theme.gameEditor.qrQuietModules
    // Whole pixels per square, so the edges stay sharp.
    readonly property int step: Math.max(1, Math.floor(Theme.gameEditor.qrSize / Math.max(1, modules + 2 * quiet)))

    function ask() {
        SteamLibrary.signIn()
        open()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.gameEditor.signInWidth, parent.width - 2 * Theme.gameEditor.padding)
    modal: true
    focus: true
    padding: Theme.gameEditor.padding
    Overlay.modal: Scrim { color: root.scrim ? Theme.gameEditor.scrim : "transparent" }
    background: Surface {
        radius: Theme.gameEditor.radius
        fill: Theme.gameEditor.background
        fillEnd: Theme.gameEditor.backgroundEnd
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.border
        border.width: Theme.gameEditor.borderWidth
    }

    onOpened: cancelButton.forceActiveFocus()
    onClosed: if (SteamLibrary.authState === "waiting" || SteamLibrary.authState === "expired") SteamLibrary.cancelSignIn()

    Connections {
        target: SteamLibrary
        function onSignedIn() { root.close() }
    }

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        Label {
            Layout.fillWidth: true
            text: qsTr("Sign in with Steam")
            color: Theme.gameEditor.headingColor
            font.weight: Theme.font.strongWeight
            font.pixelSize: Theme.gameEditor.headingFontSize
            wrapMode: Text.WordWrap
        }

        Item {
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: side
            implicitHeight: side
            readonly property int side: (root.modules + 2 * root.quiet) * root.step

            Rectangle {
                anchors.fill: parent
                radius: Theme.gameEditor.qrRadius
                color: Theme.gameEditor.qrLight
                opacity: root.auth === "waiting" ? 1 : 0.35
                Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
            }
            Shape {
                x: root.quiet * root.step
                y: root.quiet * root.step
                width: root.modules * root.step
                height: root.modules * root.step
                visible: root.modules > 0
                opacity: root.auth === "waiting" ? 1 : 0.1
                Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                transform: Scale { xScale: root.step; yScale: root.step }

                ShapePath {
                    strokeColor: "transparent"
                    fillColor: Theme.gameEditor.qrDark
                    fillRule: ShapePath.WindingFill
                    PathSvg { path: SteamLibrary.qrPath }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: root.auth === "expired" ? Theme.controls.problemsText : Theme.gameEditor.labelColor
            text: root.auth === "expired" ? qsTr("The code ran out.")
                  : qsTr("Scan with the Steam app on your phone, then approve the sign-in. Lattis never sees your password.")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Item { Layout.fillWidth: true }
            SettingsButton {
                visible: root.auth === "expired"
                text: qsTr("New code")
                primary: true
                onClicked: SteamLibrary.signIn()
            }
            SettingsButton {
                id: cancelButton
                text: qsTr("Cancel")
                onClicked: root.close()
            }
        }
    }
}
