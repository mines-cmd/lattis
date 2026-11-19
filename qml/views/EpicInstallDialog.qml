import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

AppDialog {
    id: root

    property var game: ({})
    property var plan: ({})
    property string folder: ""
    property double free: -1
    property string error: ""
    property var wineInfo: ({})

    readonly property string sizeState: plan.sizeState ?? "unknown"
    readonly property double downloadSize: plan.downloadSize ?? -1
    readonly property double diskSize: plan.diskSize ?? -1
    readonly property double missing: free >= 0 && diskSize >= 0 && free < diskSize ? diskSize - free : 0
    readonly property bool checking: sizeState === "checking"
    readonly property bool hasWine: (plan.platform ?? "Windows") === "Windows" && (wineInfo.wines ?? []).length > 0
    readonly property string gameFolder: plan.gameFolder || (game.title ?? "").replace(/[^\p{L}\p{N}]+/gu, "")

    function ask(g) {
        game = g
        plan = GameRunner.epicInstallPlan(g.id)
        folder = plan.folder ?? ""
        error = ""
        wineInfo = GameRunner.epicInstallWine(g.id)
        sharedBox.checked = false
        wineBox.currentIndex = Math.max(0, (wineInfo.wines ?? []).findIndex(w => w.wine.bin === (wineInfo.wine?.bin ?? "")))
        refreshFree()
        open()
    }

    function refreshFree() {
        free = folder !== "" ? GameRunner.freeSpace(folder) : -1
    }

    function install() {
        const e = hasWine ? GameRunner.installEpic(game.id, folder, wineInfo.wines[wineBox.currentIndex].wine, !sharedBox.checked)
                          : GameRunner.installEpic(game.id, folder)
        if (e !== "")
            error = e
        else
            close()
    }

    carded: false
    size: "medium"
    title: qsTr("Install %1").arg(game.title ?? "")

    onOpened: installButton.forceActiveFocus()

    Connections {
        target: GameRunner
        function onEpicInstallInfo(appName, p) {
            if (appName === root.plan.appName)
                root.plan = p
        }
    }

    Loader {
        id: folderDialog
        active: false
        sourceComponent: RomFolderDialog {
            onPicked: path => {
                root.folder = path
                root.refreshFree()
            }
            onClosed: folderDialog.active = false
        }
    }

    component Row2: RowLayout {
        property string label
        property string value
        property color valueColor: Theme.gameEditor.labelColor

        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        Label {
            Layout.fillWidth: true
            elide: Text.ElideRight
            color: Theme.controls.labelColor
            font.pixelSize: Theme.controls.labelFontSize
            text: parent.label
        }
        Label {
            color: parent.valueColor
            font.pixelSize: Theme.controls.labelFontSize
            font.features: { "tnum": 1 }
            text: parent.value
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                color: Theme.controls.labelColor
                font.pixelSize: Theme.controls.labelFontSize
                text: qsTr("Install into")
            }
            PathField {
                Layout.fillWidth: true
                path: root.folder === "" ? "" : root.folder.replace(/\/+$/, "") + "/" + root.gameFolder
            }
            SettingsButton {
                text: qsTr("Change")
                onClicked: folderDialog.active = true
            }
        }
    }

    ColumnLayout {
        visible: root.hasWine
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        SettingsCheckBox {
            id: sharedBox
            Layout.fillWidth: true
            checked: false
            text: qsTr("Use the shared Wine prefix")
        }
        RowLayout {
            visible: !sharedBox.checked
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                color: Theme.controls.labelColor
                font.pixelSize: Theme.controls.labelFontSize
                text: qsTr("Wine prefix")
            }
            PathField {
                Layout.fillWidth: true
                path: root.wineInfo.prefix ?? ""
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                color: Theme.controls.labelColor
                font.pixelSize: Theme.controls.labelFontSize
                text: qsTr("Wine version")
            }
            SettingsComboBox {
                id: wineBox
                Layout.preferredWidth: Theme.controls.inputHeight * 7
                model: (root.wineInfo.wines ?? []).map(w => w.label)
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        Row2 {
            label: qsTr("Download")
            value: root.downloadSize >= 0 ? GameRunner.sizeText(root.downloadSize)
                 : root.checking ? qsTr("Checking size…") : qsTr("Size unknown")
        }
        Row2 {
            label: qsTr("On disk")
            value: root.diskSize >= 0 ? GameRunner.sizeText(root.diskSize)
                 : root.checking ? qsTr("Checking size…") : qsTr("Size unknown")
        }
        Row2 {
            label: qsTr("Free space")
            value: root.free >= 0 ? GameRunner.sizeText(root.free) : qsTr("Unknown")
        }
    }

    Label {
        visible: root.missing > 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.errorColor
        text: qsTr("Not enough space: %1 more is needed.").arg(GameRunner.sizeText(root.missing))
    }
    Label {
        visible: root.error !== ""
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.errorColor
        text: root.error
    }

    footer: [
        SettingsButton {
            text: qsTr("Cancel")
            onClicked: root.close()
        },
        SettingsButton {
            id: installButton
            text: qsTr("Install")
            primary: true
            enabled: root.folder !== "" && root.missing === 0 && !root.checking
            onClicked: root.install()
        }
    ]
}
