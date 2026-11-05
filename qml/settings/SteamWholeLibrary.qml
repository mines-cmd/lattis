import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
SettingsCard {
    id: root

    readonly property var steam: Library.steam
    readonly property string auth: SteamLibrary.authState
    readonly property bool signedIn: auth === "signed_in"
    readonly property bool on: steam.wholeLibrary

    signal failed(string error)
    signal message(string text, var link)
    signal signInRequested()

    function setOn(value) {
        const error = Library.setSteamValue("whole_library", value)
        if (error !== "") {
            failed(error)
            return
        }
        if (value) {
            if (SteamLibrary.configured)
                SteamLibrary.syncNow()
        } else if (SteamLibrary.gameCount > 0) {
            message(qsTr("The games that aren't installed are gone from the library"), {
                text: qsTr("Undo"),
                isUndo: true,
                run: () => root.setOn(true)
            })
        }
    }

    function signOut() {
        SteamLibrary.signOut()
        message(qsTr("Signed out. Steam still lists the session until you remove it there."), {
            text: qsTr("Open Steam's list"),
            run: () => Qt.openUrlExternally(SteamLibrary.devicesUrl())
        })
    }

    component Divider: Rectangle {
        visible: Theme.settingsWindow.rowDividers
        Layout.fillWidth: true
        Layout.leftMargin: -Theme.settingsWindow.rowPadding
        Layout.rightMargin: -Theme.settingsWindow.rowPadding
        implicitHeight: 1
        color: Theme.settingsWindow.rowDivider
    }

    component Line: RowLayout {
        id: line
        property alias label: name.text
        property alias note: noteLabel.text
        property alias noteColor: noteLabel.color
        property alias labelColor: name.color
        default property alias controls: controlRow.data

        Layout.fillWidth: true
        spacing: Theme.layout.spacing * 1.5

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                id: name
                Layout.fillWidth: true
                color: Theme.controls.labelColor
                font.pixelSize: Theme.controls.labelFontSize
                font.weight: Theme.font.strongWeight
                wrapMode: Text.Wrap
            }
            SettingsNote {
                id: noteLabel
                visible: text !== ""
                Layout.fillWidth: true
                linkColor: Theme.gameEditor.linkColor
                onLinkActivated: link => Qt.openUrlExternally(link)
            }
        }
        RowLayout {
            id: controlRow
            spacing: Theme.layout.spacing
        }
    }

    Line {
        label: qsTr("Import the whole library")
        note: root.on ? qsTr("Every game your account owns, and the ones your Steam Family shares with you, installed "
                             + "or not. Install starts the download in Steam.")
                      : qsTr("Off: only the games installed in Steam show up.")

        SettingsSwitch {
            checked: root.on
            onToggled: root.setOn(checked)
        }
    }

    Divider { visible: Theme.settingsWindow.rowDividers && root.on }

    Line {
        visible: root.on
        label: root.signedIn ? qsTr("Signed in as %1").arg(SteamLibrary.accountName)
               : root.auth === "waiting" ? qsTr("Waiting for the Steam app")
               : qsTr("Steam account")
        note: root.signedIn
              ? (SteamLibrary.keyringAvailable ? qsTr("The sign-in is kept in your keyring.")
                                                : qsTr("No keyring found: the sign-in lasts until Lattis quits."))
              : qsTr("Not available for now: Steam can flag a new sign-in as suspicious. Use a Web API key below.")

        SettingsButton {
            visible: root.signedIn
            text: qsTr("Sign out")
            danger: true
            tip: qsTr("Forget the sign-in here; remove it from Steam's authorized devices to end it there too")
            onClicked: root.signOut()
        }
        SettingsButton {
            visible: !root.signedIn
            enabled: false
            text: qsTr("Sign in with Steam")
            onClicked: root.signInRequested()
        }
    }

    Line {
        visible: root.on && !root.signedIn && !SteamLibrary.keyringAvailable
        label: qsTr("No keyring found")
        note: qsTr("A sign-in lasts until Lattis quits, then you sign in again. Install GNOME Keyring or KWallet "
                   + "to keep it.")
    }

    Line {
        visible: root.on && SteamLibrary.messageKind !== ""
        label: SteamLibrary.message
        labelColor: Theme.controls.problemsText

        SettingsButton {
            visible: SteamLibrary.messageKind === "private"
            text: qsTr("Open privacy settings")
            onClicked: Qt.openUrlExternally(SteamLibrary.privacyUrl())
        }
    }

    Divider { visible: Theme.settingsWindow.rowDividers && root.on }

    ColumnLayout {
        visible: root.on
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        Label {
            Layout.fillWidth: true
            text: qsTr("Web API key")
            color: Theme.controls.labelColor
            font.pixelSize: Theme.controls.labelFontSize
            font.weight: Theme.font.strongWeight
        }
        KeyRow {
            id: keyRow
            secret: true
            buttonWidth: showMetric.implicitWidth
            Component.onCompleted: field.text = SteamLibrary.webKey
            onFinished: {
                if (field.text.trim() === SteamLibrary.webKey)
                    return
                const error = SteamLibrary.setWebKey(field.text)
                if (error !== "")
                    root.failed(error)
            }
        }
        SettingsButton { id: showMetric; visible: false; text: qsTr("Show") }
        SettingsNote {
            Layout.fillWidth: true
            linkColor: Theme.gameEditor.linkColor
            text: qsTr("Fetches the games you own, not the family's, without signing in. Your Game details must be "
                       + "public. The key also finds which friends play a game; friends who hide their Game details "
                       + "are simply missing. <a href=\"%1\">Get a key</a>").arg(SteamLibrary.keyUrl())
            onLinkActivated: link => Qt.openUrlExternally(link)
        }
    }

    Divider { visible: Theme.settingsWindow.rowDividers && root.on }

    component Progress: ColumnLayout {
        id: progress
        property string text
        property int done
        property int total

        Layout.fillWidth: true
        spacing: Theme.layout.spacing / 2

        Label {
            Layout.fillWidth: true
            text: progress.text.arg(progress.done).arg(progress.total)
            color: Theme.onboarding.descriptionColor
            font.pixelSize: Theme.controls.labelFontSize
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Theme.gameEditor.progressHeight
            radius: Theme.gameEditor.progressRadius
            color: Theme.gameEditor.progressTrack
            clip: true

            Rectangle {
                height: parent.height
                radius: parent.radius
                color: Theme.gameEditor.progressFill
                width: progress.total > 0 ? parent.width * Math.min(1, progress.done / progress.total) : 0
                Behavior on width { enabled: Motion.on; MotionNumber { speed: "normal" } }
            }
        }
    }

    ColumnLayout {
        id: background
        readonly property bool art: SteamArtFetch.running
        readonly property bool lookup: ScanLookup.running

        visible: root.on && (art || lookup)
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        Progress {
            visible: background.art
            text: qsTr("Fetching art: %1 of %2")
            done: SteamArtFetch.progressDone
            total: SteamArtFetch.progressTotal
        }
        Progress {
            visible: background.lookup
            text: qsTr("Looking up new games: %1 of %2")
            done: ScanLookup.progressDone
            total: ScanLookup.progressTotal
        }
    }

    Divider { visible: Theme.settingsWindow.rowDividers && root.on && (SteamArtFetch.running || ScanLookup.running) }

    Line {
        visible: root.on
        label: SteamLibrary.syncing ? qsTr("Syncing")
               : SteamLibrary.statusLine !== "" ? SteamLibrary.statusLine
               : SteamLibrary.configured ? qsTr("Not synced yet")
               : qsTr("Nothing to fetch with")
        note: SteamLibrary.configured ? qsTr("Fetched once a day, and when you press Sync now.")
                                      : qsTr("Sign in or add a key, or nothing is fetched.")

        SettingsButton {
            text: qsTr("Sync now")
            enabled: SteamLibrary.configured && !SteamLibrary.syncing
            onClicked: SteamLibrary.syncNow()
        }
    }
}
