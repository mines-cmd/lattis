import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Properties → Launch, for a Linux game run from an AppImage. The typed values live in GameEditor
// (changes() writes them); this is loaded only for such a game, so the Installer isn't made for any other.
EditorCard {
    id: card

    required property var editor
    readonly property var info: Installer.games[editor.game.id] ?? ({})
    readonly property string state: info.state ?? "idle"
    readonly property bool working: state === "checking" || state === "downloading" || state === "extracting"
                                    || state === "installing"
    readonly property bool gitlab: editor.updSource.startsWith("gitlab:")
    readonly property bool canCheck: editor.updSource.trim() !== "" && editor.updAsset.trim() !== "" && !working
    // The last check's answer holds for the fields it was asked with, not for ones edited since.
    readonly property bool statusFresh: !editor.updDirty || editor.updCheckedWith === editor.updKey
    readonly property string updateVersion: statusFresh ? (info.updateVersion ?? "") : ""

    function check() {
        const g = editor.game
        editor.updCheckedWith = editor.updKey
        Installer.checkGameWith(g.id, editor.exeShown, g.title, editor.updSource, editor.updAsset, editor.updPre)
    }

    function update() {
        const g = editor.game
        Installer.setGameSource(g.id, editor.exeShown, g.title, editor.updSource, editor.updAsset, editor.updPre)
        if (editor.updDirty || editor.updReset) {
            editor.save()
            if (editor.dialogOpened) // it said why it didn't save
                return
        }
        Installer.updateGame(g.id)
    }

    function statusText() {
        if (!statusFresh)
            return ""
        if (state === "checking")
            return qsTr("Checking…")
        if (state === "downloading" || state === "extracting" || state === "installing")
            return info.progress >= 0 && state === "downloading"
                   ? qsTr("Updating · %1%").arg(Math.round(info.progress * 100)) : qsTr("Updating")
        if (state === "failed")
            return info.error ?? ""
        if (updateVersion !== "")
            return qsTr("Update available: %1").arg(updateVersion)
        if (info.upToDate ?? false)
            return (info.version ?? "") !== "" ? qsTr("Up to date · %1").arg(info.version) : qsTr("Up to date")
        return ""
    }

    title: qsTr("Updates")

    EditorHint {
        open: editor.updFromAppImage
        text: qsTr("From the AppImage")
    }

    EditorField {
        label: qsTr("Source")
        help: qsTr("Where the game's releases are: github:owner/repo or gitlab:host/group/project. "
                   + "Paste the repository's address and it is turned into one.")
        actions: EditorTextAction {
            visible: editor.updCanUseEmbedded
            text: qsTr("Use the AppImage's")
            iconPath: Icons.undo
            tip: qsTr("Go back to the update information inside the AppImage")
            onClicked: editor.useEmbeddedSource()
        }

        SettingsTextField {
            id: sourceField
            Layout.fillWidth: true
            placeholderText: qsTr("github:owner/repo")
            text: editor.updSource
            onTextChanged: if (!activeFocus) shownLength = text.length
            property int shownLength: 0
            // A paste lands in one edit; typing a URL by hand is turned into a source only when the field is left.
            function useAddress(onlyIfPasted) {
                const pasted = Installer.sourceFromText(text)
                const convert = pasted !== "" && (!onlyIfPasted || Math.abs(text.length - shownLength) > 1)
                // Through the editor's value, so the field keeps following it ("Use the AppImage's").
                editor.updSource = convert ? pasted : text
                shownLength = convert ? pasted.length : text.length
            }
            onTextEdited: {
                editor.updReset = false
                useAddress(true)
            }
            onEditingFinished: useAddress(false)
            onAccepted: card.check()
        }
    }

    EditorField {
        label: qsTr("Release file name")
        help: qsTr("The AppImage to take from a release. * matches any text and ? one character; "
                   + "separate several patterns with ;")

        SettingsTextField {
            Layout.fillWidth: true
            placeholderText: qsTr("Game-*-x86_64.AppImage")
            text: editor.updAsset
            onTextEdited: {
                editor.updAsset = text
                editor.updReset = false
            }
            onAccepted: card.check()
        }
        EditorHint {
            readonly property bool noMatch: card.statusFresh && (card.info.matched ?? false)
                                            && (card.info.match ?? "") === ""
            open: card.statusFresh && (card.info.match ?? "") !== "" || noMatch
            color: noMatch ? Theme.gameEditor.warningColor : Theme.gameEditor.hintColor
            text: noMatch ? qsTr("No file in the latest release matches")
                          : qsTr("Matches %1").arg(card.info.match ?? "")
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        Label {
            Layout.fillWidth: true
            text: qsTr("Allow pre-releases")
            color: Theme.gameEditor.labelColor
            elide: Text.ElideRight
            opacity: card.gitlab ? 0.45 : 1

            ToolTip.visible: hover.hovered
            ToolTip.delay: Theme.tooltip.delay
            ToolTip.text: card.gitlab ? qsTr("Only for GitHub releases")
                                      : qsTr("Take the newest release that has the file, even a pre-release")
            HoverHandler { id: hover }
        }
        SettingsSwitch {
            enabled: !card.gitlab
            checked: editor.updPre && !card.gitlab
            onToggled: {
                editor.updPre = checked
                editor.updReset = false
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        Label {
            Layout.fillWidth: true
            visible: text !== ""
            text: card.statusText()
            color: card.state === "failed" && card.statusFresh ? Theme.gameEditor.warningColor
                                                               : Theme.gameEditor.hintColor
            font.pixelSize: Theme.gameEditor.hintFontSize
            wrapMode: Text.WordWrap
        }
        Item {
            Layout.fillWidth: true
            visible: card.statusText() === ""
        }
        SettingsButton {
            visible: card.updateVersion !== "" && !card.working
            text: qsTr("Update")
            primary: true
            tip: qsTr("Downloads the new AppImage over this one; the old file goes to the trash")
            onClicked: card.update()
        }
        SettingsButton {
            text: qsTr("Check now")
            enabled: card.canCheck
            tip: qsTr("Looks for a newer release with what is typed here, before Save")
            onClicked: card.check()
        }
    }

    EditorHint {
        open: editor.updSource.trim() === "" && editor.updAsset.trim() === ""
        text: qsTr("Add the game's GitHub or GitLab repository to get its updates")
    }
}
