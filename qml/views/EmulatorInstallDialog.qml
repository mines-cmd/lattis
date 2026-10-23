import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Install an emulator from a game's page: the routes, the job's progress, and the program field for one
// installed some other way. Closes by itself once the library finds the emulator.
Item {
    id: root

    property string emulatorId

    readonly property alias dialog: dialog
    readonly property var emulator: Library.emulators.find(e => e.id === emulatorId) ?? null

    // What EmulatorInstallTop reads from its row.
    readonly property var installInfo: Installer.emulators[emulatorId] ?? null
    readonly property string jobState: installInfo?.state ?? "idle"
    readonly property bool hasRoutes: (installInfo?.routes.length ?? 0) > 0
    readonly property bool installing: jobState !== "idle" && jobState !== "failed"
    readonly property bool installFailed: jobState === "failed"
    readonly property bool canInstall: !installFailed && hasRoutes
    readonly property bool byApp: false
    readonly property bool hasUpdate: false
    readonly property bool awaitingDetection: jobState === "idle" && (installInfo?.installedByApp ?? false)

    signal error(string text)

    function openFor(id) {
        emulatorId = id
        dialog.open()
    }

    onEmulatorChanged: if (emulator?.found && dialog.opened) dialog.close()

    MotionPopup {
        id: dialog

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(Theme.gameEditor.installWidth, parent.width - 2 * Theme.layout.margin)
        modal: true
        focus: true
        padding: Theme.gameEditor.padding
        closePolicy: Popup.CloseOnEscape

        Overlay.modal: Scrim {}

        background: Surface {
            radius: Theme.gameEditor.radius
            fill: Theme.gameEditor.background
            fillEnd: Theme.gameEditor.backgroundEnd
            direction: Theme.gameEditor.gradientDirection
            border.color: Theme.gameEditor.border
            border.width: Theme.gameEditor.borderWidth
        }

        contentItem: ColumnLayout {
            spacing: Theme.gameEditor.rowSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Install %1").arg(root.emulator?.name ?? "")
                    color: Theme.gameEditor.headingColor
                    font.weight: Theme.font.strongWeight
                    font.pixelSize: Theme.gameEditor.headingFontSize
                    elide: Text.ElideRight
                }
                WindowControls {
                    Layout.alignment: Qt.AlignTop
                    dialog: dialog
                }
            }

            EmulatorInstallTop {
                visible: !root.awaitingDetection && (root.installing || root.installFailed || root.hasRoutes)
                Layout.fillWidth: true
                row: root
            }

            Label {
                visible: root.awaitingDetection
                Layout.fillWidth: true
                text: qsTr("Looking for %1…").arg(root.emulator?.name ?? "")
                color: Theme.controls.descriptionColor
            }

            EmulatorProgramField {
                visible: !root.installing && root.emulator !== null
                Layout.fillWidth: true
                emulator: root.emulator
                title: root.hasRoutes ? qsTr("Already have it somewhere else?") : qsTr("Program")
                onSaved: error => { if (error !== "") root.error(error) }
            }
        }
    }
}
