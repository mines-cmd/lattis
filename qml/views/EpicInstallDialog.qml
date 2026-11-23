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
    property bool sharedPrefix: false
    property bool prefixShownShared: false

    readonly property string sizeState: plan.sizeState ?? "unknown"
    readonly property double downloadSize: plan.downloadSize ?? -1
    readonly property double diskSize: plan.diskSize ?? -1
    readonly property double missing: free >= 0 && diskSize >= 0 && free < diskSize ? diskSize - free : 0
    readonly property bool checking: sizeState === "checking"
    readonly property bool hasWine: (plan.platform ?? "Windows") === "Windows" && (wineInfo.wines ?? []).length > 0
    readonly property string gameFolder: plan.gameFolder || (game.title ?? "").replace(/[^\p{L}\p{N}]+/gu, "")
    readonly property real labelWidth: Math.max(installLabel.implicitWidth, prefixLabel.implicitWidth,
                                                versionLabel.implicitWidth)

    // `view` is for screenshots: "shared" picks the shared prefix.
    function ask(g, view) {
        game = g
        plan = GameRunner.epicInstallPlan(g.id)
        folder = plan.folder ?? ""
        error = ""
        wineInfo = GameRunner.epicInstallWine(g.id)
        prefixSwap.stop()
        sharedPrefix = view === "shared"
        prefixShownShared = sharedPrefix
        prefixField.opacity = 1
        const wines = wineInfo.wines ?? []
        const remembered = wines.findIndex(w => w.wine.bin === (wineInfo.lastWine ?? ""))
        wineBox.currentIndex = remembered >= 0 ? remembered
                                               : Math.max(0, wines.findIndex(w => w.wine.bin === (wineInfo.wine?.bin ?? "")))
        refreshFree()
        open()
    }

    function refreshFree() {
        free = folder !== "" ? GameRunner.freeSpace(folder) : -1
    }

    function install() {
        const e = hasWine ? GameRunner.installEpic(game.id, folder, wineInfo.wines[wineBox.currentIndex].wine, !sharedPrefix)
                          : GameRunner.installEpic(game.id, folder)
        if (e !== "")
            error = e
        else
            close()
    }

    function sizeText(bytes) {
        return bytes >= 0 ? GameRunner.sizeText(bytes) : checking ? qsTr("Checking…") : qsTr("Unknown")
    }

    carded: false
    size: "medium"
    title: qsTr("Install %1").arg(game.title ?? "")

    onOpened: installButton.forceActiveFocus()
    onSharedPrefixChanged: Motion.fades ? prefixSwap.restart() : prefixShownShared = sharedPrefix

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

    SequentialAnimation {
        id: prefixSwap
        MotionNumber { target: prefixField; property: "opacity"; to: 0; fade: true; speed: "fast" }
        ScriptAction { script: root.prefixShownShared = root.sharedPrefix }
        MotionNumber { target: prefixField; property: "opacity"; to: 1; fade: true; speed: "fast" }
    }

    component FieldLabel: Label {
        Layout.preferredWidth: root.labelWidth
        color: Theme.controls.labelColor
        font.pixelSize: Theme.controls.labelFontSize
    }

    component SizeItem: Row {
        property string icon
        property string value
        property string label

        spacing: Theme.layout.spacing / 2

        Icon {
            visible: parent.icon !== ""
            anchors.verticalCenter: parent.verticalCenter
            path: parent.icon
            size: Theme.controls.iconSize
            color: Theme.controls.descriptionColor
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: parent.value
            color: Theme.controls.labelColor
            font.pixelSize: Theme.controls.labelFontSize
            font.features: { "tnum": 1 }
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: parent.label
            color: Theme.controls.descriptionColor
            font.pixelSize: Theme.controls.labelFontSize
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        FieldLabel {
            id: installLabel
            text: qsTr("Install into")
        }
        PathField {
            Layout.fillWidth: true
            copyable: false
            path: root.folder === "" ? "" : root.folder.replace(/\/+$/, "") + "/" + root.gameFolder
        }
        SettingsButton {
            text: qsTr("Change")
            onClicked: folderDialog.active = true
        }
    }

    ColumnLayout {
        visible: root.hasWine
        Layout.fillWidth: true
        spacing: Theme.gameEditor.rowSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            FieldLabel {
                id: prefixLabel
                text: qsTr("Wine prefix")
            }
            SegmentedToggle {
                Layout.preferredHeight: Theme.controls.inputHeight
                accessibleName: qsTr("Wine prefix")
                choices: [{ id: "own", text: qsTr("Own"), tip: qsTr("A prefix for this game alone") },
                          { id: "shared", text: qsTr("Shared"), tip: qsTr("The prefix Heroic's games share") }]
                current: root.sharedPrefix ? "shared" : "own"
                onPicked: id => root.sharedPrefix = id === "shared"
            }
            PathField {
                id: prefixField
                Layout.fillWidth: true
                copyable: false
                path: root.prefixShownShared ? root.wineInfo.sharedPrefix ?? "" : root.wineInfo.prefix ?? ""
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            FieldLabel {
                id: versionLabel
                text: qsTr("Wine version")
            }
            SettingsComboBox {
                id: wineBox
                Layout.fillWidth: true
                model: (root.wineInfo.wines ?? []).map(w => w.label)
            }
        }
    }

    PlainLabel {
        visible: root.error !== ""
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.errorColor
        text: root.error
    }

    footerStart: ColumnLayout {
        spacing: Theme.layout.spacing / 2

        Row {
            spacing: Theme.layout.spacing * 2

            SizeItem {
                icon: Icons.download
                value: root.sizeText(root.downloadSize)
                label: qsTr("download")
            }
            SizeItem {
                icon: Icons.hardDrive2Line
                value: root.sizeText(root.diskSize)
                label: qsTr("on disk")
            }
            Label {
                visible: root.free >= 0 && root.missing === 0
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("%1 free").arg(GameRunner.sizeText(root.free))
                color: Theme.controls.descriptionColor
                font.pixelSize: Theme.controls.labelFontSize
                font.features: { "tnum": 1 }
            }
        }
        Row {
            visible: root.missing > 0
            spacing: Theme.layout.spacing / 2

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                path: Icons.alertTriangleLine
                size: Theme.controls.iconSize
                color: Theme.gameEditor.warningColor
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Only %1 free on this drive. Pick another folder.").arg(GameRunner.sizeText(root.free))
                color: Theme.gameEditor.warningColor
                font.pixelSize: Theme.controls.labelFontSize
            }
        }
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
