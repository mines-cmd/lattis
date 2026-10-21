import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects

// See docs/code/qml.md.
ColumnLayout {
    id: root

    required property var guide

    readonly property var steam: Library.steam
    readonly property bool pending: Library.setupPending
    readonly property bool looking: steam.enabled && !steam.found && !steam.problem
    readonly property int notInLibrary: steam.shortcuts - steam.shortcutsInLibrary
    readonly property var tools: steam.presets.filter(p => p.builtIn && (p.program === "" || p.programFound))

    spacing: Theme.onboarding.spacing

    function set(key, value) {
        const error = Library.setSteamValue(key, value)
        if (error !== "")
            guide.message(error)
    }

    function setDefault(id, on) {
        const ids = steam.defaultPresets.filter(p => p !== id)
        if (on)
            ids.push(id)
        set("default_presets", steam.presets.map(p => p.id).filter(p => ids.includes(p)))
    }

    component Divider: Rectangle {
        visible: Theme.settingsWindow.rowDividers
        Layout.fillWidth: true
        Layout.leftMargin: -Theme.settingsWindow.rowPadding
        Layout.rightMargin: -Theme.settingsWindow.rowPadding
        implicitHeight: 1
        color: Theme.settingsWindow.rowDivider
    }

    component Row: RowLayout {
        id: row
        property alias label: name.text
        property alias note: noteLabel.text
        property alias noteColor: noteLabel.color
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
                wrapMode: Text.Wrap
            }
            SettingsNote {
                id: noteLabel
                visible: text !== ""
                Layout.fillWidth: true
            }
        }
        RowLayout {
            id: controlRow
            spacing: Theme.layout.spacing
        }
    }

    SettingsCard {
        panel: true

        RowLayout {
            visible: root.steam.enabled
            Layout.fillWidth: true
            spacing: Theme.layout.spacing * 1.5

            Rectangle {
                opacity: root.steam.found ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                implicitWidth: Theme.onboarding.foundIconSize
                implicitHeight: Theme.onboarding.foundIconSize
                radius: width / 2
                color: Qt.alpha(Theme.onboarding.foundColor, Theme.onboarding.stepDoneOpacity)

                Icon {
                    anchors.centerIn: parent
                    path: Icons.check
                    size: Math.round(Theme.onboarding.foundIconSize / 2)
                    color: Theme.onboarding.foundColor
                }
            }
            Row {
                label: root.looking ? qsTr("Looking for Steam…")
                       : root.steam.found ? qsTr("Steam found") : qsTr("Steam wasn't found")
                note: root.looking ? ""
                      : !root.steam.found ? (root.steam.path !== "" ? qsTr("There's no Steam in %1.").arg(root.steam.path)
                                             : qsTr("Not in ~/.steam/steam or the Flatpak's folder."))
                      : root.steam.problem ? root.steam.status
                      : (root.steam.flatpak ? qsTr("%1 · Flatpak") : qsTr("%1 · native")).arg(root.steam.shownRoot)
                noteColor: root.steam.problem ? Theme.controls.problemsText : Theme.controls.descriptionColor

                SettingsNote {
                    visible: account.visible
                    text: qsTr("Account")
                }
                SteamAccountBox {
                    id: account
                    visible: root.steam.found && root.steam.users.length > 1
                    Layout.preferredWidth: Theme.settingsWindow.pickerWidth / 3
                    onFailed: message => root.guide.message(message)
                }
                Label {
                    visible: root.steam.found && root.steam.users.length === 1
                    text: root.steam.users[0]?.name ?? ""
                    color: Theme.controls.labelColor
                    font.pixelSize: Theme.controls.labelFontSize
                    font.bold: true
                }
                SettingsButton {
                    visible: !root.steam.found && !root.looking && root.steam.path !== ""
                    text: qsTr("Find automatically")
                    tip: qsTr("Look in ~/.steam/steam, then the Flatpak's folder")
                    onClicked: root.set("path", "")
                }
                SettingsButton {
                    visible: !root.steam.found && !root.looking
                    text: qsTr("Choose folder")
                    tip: qsTr("Choose Steam's folder: the one with steamapps in it")
                    onClicked: folderDialog.active = true
                }
            }
        }
        Divider { visible: Theme.settingsWindow.rowDividers && root.steam.enabled }

        Row {
            label: qsTr("Import Steam games")
            note: !root.steam.enabled ? qsTr("Off: Steam games won't be imported.")
                  : !root.steam.found ? ""
                  : root.steam.installed === 0 ? qsTr("No installed games.")
                  : root.pending ? (root.steam.installed === 1 ? qsTr("1 installed game will be added when you finish.")
                                    : qsTr("%1 installed games will be added when you finish.").arg(root.steam.installed))
                  : root.steam.installed === 1 ? qsTr("1 game installed.")
                  : qsTr("%1 games installed.").arg(root.steam.installed)

            SettingsSwitch {
                checked: root.steam.enabled
                onToggled: root.set("enabled", checked)
            }
        }

        Divider { visible: Theme.settingsWindow.rowDividers && root.steam.found }
        Row {
            opacity: root.steam.found ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
            label: qsTr("Non-Steam games")
            note: !root.steam.shortcutsOk ? qsTr("Steam's list of non-Steam games couldn't be read.")
                  : root.steam.shortcuts === 0 ? qsTr("Steam has no non-Steam games.")
                  : root.notInLibrary === 0 ? qsTr("Every non-Steam game in Steam is in the library.")
                  : root.pending && root.guide.shortcutsToAdd.length > 0
                    ? qsTr("%1 of %2 will be added when you finish.").arg(root.guide.shortcutsToAdd.length)
                          .arg(root.notInLibrary)
                  : root.notInLibrary === 1 ? qsTr("1 shortcut in Steam isn't in the library yet.")
                  : qsTr("%1 shortcuts in Steam aren't in the library yet.").arg(root.notInLibrary)

            SettingsButton {
                text: qsTr("Add existing shortcuts")
                enabled: root.notInLibrary > 0
                tip: qsTr("Choose which of Steam's non-Steam games to bring into the library")
                onClicked: {
                    picker.active = true
                    picker.item.openFresh(root.pending && root.guide.shortcutsToAdd.length > 0
                                          ? root.guide.shortcutsToAdd : undefined)
                }
            }
        }
    }

    SettingsCard {
        panel: true
        opacity: root.steam.found && root.tools.length > 0 ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }

        Row {
            label: qsTr("Start new Steam games with")
            note: qsTr("Added to a game's launch options when it's first imported. Only tools found on this "
                       + "computer are listed.")
        }
        Repeater {
            model: root.tools
            RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                SettingsCheckBox {
                    text: modelData.name
                    checked: root.steam.defaultPresets.includes(modelData.id)
                    onToggled: root.setDefault(modelData.id, checked)
                }
                SettingsNote {
                    Layout.fillWidth: true
                    text: modelData.note
                }
            }
        }
    }

    SettingsNote {
        visible: root.pending && (root.steam.found || !root.steam.enabled)
        Layout.fillWidth: true
        text: qsTr("Nothing is imported until you finish, so your games come in with the info from "
                   + "“Game info”.")
    }
    Item {
        id: covers
        readonly property real tileHeight: Math.min(Theme.onboarding.steamCoverMaxHeight, height)
        readonly property real tileWidth: Math.round(tileHeight * 2 / 3)
        readonly property int fit: Math.max(0, Math.floor((width + Theme.onboarding.steamCoverSpacing)
                                                          / (tileWidth + Theme.onboarding.steamCoverSpacing)))
        readonly property var all: {
            root.steam
            return root.steam.enabled && root.steam.found ? Library.steamCovers(24) : []
        }
        readonly property int shown: Math.min(all.length, root.steam.installed > fit ? fit - 1 : fit)
        readonly property int rest: root.steam.installed - shown

        // Always in the layout: it holds the leftover height (the rest of the page stays at the
        // top), and hiding it would leave it no height to measure.
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.bottomMargin: Theme.onboarding.spacing

        // Here, not among the page's children: an empty Loader in the layout still gets a
        // spacing gap, which showed as extra room above the first card.
        Loader {
            id: picker
            active: false
            sourceComponent: ShortcutPicker {
                addNow: !root.pending
                onChosen: appIds => root.guide.shortcutsToAdd = appIds
            }
        }
        Loader {
            id: folderDialog
            active: false
            sourceComponent: RomFolderDialog {
                onPicked: path => root.set("path", path)
                onClosed: folderDialog.active = false
            }
        }

        Item {
            readonly property bool wanted: root.steam.enabled && !root.looking
                                           && (!root.steam.found || root.steam.installed === 0)
                                           && covers.tileHeight >= Theme.onboarding.steamCoverMinHeight
            anchors.fill: parent
            opacity: wanted ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }

            RowLayout {
                id: ghosts
                anchors.centerIn: parent
                spacing: Theme.onboarding.steamCoverSpacing

                Repeater {
                    model: Math.min(covers.fit, Theme.onboarding.steamGhostTiles)

                    Surface {
                        required property int index
                        Layout.preferredWidth: covers.tileWidth
                        Layout.preferredHeight: covers.tileHeight
                        opacity: 1 - Math.abs(index - (Math.min(covers.fit, Theme.onboarding.steamGhostTiles) - 1) / 2) * 0.25
                        radius: Theme.onboarding.steamCoverRadius
                        fill: Theme.onboarding.panelBackground
                        fillEnd: Theme.onboarding.panelBackgroundEnd
                        direction: Theme.onboarding.gradientDirection
                        border.width: 1
                        border.color: Theme.onboarding.panelBorder
                    }
                }
            }
            Label {
                anchors.centerIn: parent
                text: qsTr("No installed games")
                color: Theme.onboarding.descriptionColor
                font.pixelSize: Theme.controls.labelFontSize
            }
        }

        // Not Row: this file's own Row (a setting's row) shadows QtQuick's.
        RowLayout {
            anchors.verticalCenter: parent.verticalCenter
            x: Math.round((covers.width - width) / 2)
            opacity: covers.all.length > 0 && covers.tileHeight >= Theme.onboarding.steamCoverMinHeight
                     && covers.shown > 0 ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
            spacing: Theme.onboarding.steamCoverSpacing

            Repeater {
                model: covers.all.slice(0, Math.max(0, covers.shown))

                ArtThumb {
                    required property var modelData
                    Layout.preferredWidth: covers.tileWidth
                    Layout.preferredHeight: covers.tileHeight
                    cornerRadius: Theme.onboarding.steamCoverRadius
                    source: modelData.cover
                    Accessible.name: modelData.name

                    HoverHandler { id: coverHover }
                    ToolTip.visible: coverHover.hovered
                    ToolTip.delay: Theme.tooltip.delay
                    ToolTip.text: modelData.name
                }
            }
            Surface {
                id: moreTile
                visible: covers.rest > 0
                Layout.preferredWidth: covers.tileWidth
                Layout.preferredHeight: covers.tileHeight
                radius: Theme.onboarding.steamCoverRadius
                // Nothing of the panel under the picture, or its edge shows as a faint outline.
                fill: moreArt.visible ? "transparent" : Theme.onboarding.panelBackground
                fillEnd: moreArt.visible ? "transparent" : Theme.onboarding.panelBackgroundEnd
                direction: Theme.onboarding.gradientDirection
                border.width: moreArt.visible ? 0 : 1
                border.color: Theme.onboarding.panelBorder

                readonly property string next: covers.all[covers.shown]?.cover ?? ""

                Rectangle {
                    id: moreMask
                    anchors.fill: parent
                    radius: parent.radius
                    visible: false
                    layer.enabled: true
                }
                Item {
                    id: moreArt
                    anchors.fill: parent
                    visible: moreTile.next !== "" && moreImage.status === Image.Ready
                    layer.enabled: visible
                    layer.effect: MultiEffect {
                        maskEnabled: true
                        maskSource: moreMask
                        // Without a threshold every half-covered edge pixel counts as inside, which eats the curve of the corner.
                        maskThresholdMin: 0.5
                        maskSpreadAtMin: 1.0
                    }

                    FadeImage {
                        id: moreImage
                        // Larger than the tile, so the blur's faded rim falls outside the mask.
                        anchors.fill: parent
                        anchors.margins: -Theme.onboarding.steamMoreBlur / 2
                        source: moreTile.next
                        sourceSize.height: height
                        fillMode: Image.PreserveAspectCrop
                        layer.enabled: status === Image.Ready
                        layer.effect: MultiEffect {
                            blurEnabled: true
                            blur: 1
                            blurMax: Theme.onboarding.steamMoreBlur
                        }
                    }
                    Rectangle {
                        anchors.fill: parent
                        color: Theme.onboarding.steamMoreDim
                    }
                }

                Label {
                    anchors.centerIn: parent
                    text: qsTr("+%1").arg(covers.rest)
                    color: Theme.onboarding.steamMoreColor
                    font.pixelSize: Theme.onboarding.steamMoreFontSize
                    font.bold: true
                }
            }
        }
    }
}
