import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    readonly property string description: qsTr("Show the Epic games you have in Heroic, and launch them through it.")

    readonly property string currentSection: ""
    function jumpTo(section) {}
    function jumpBy(step) {}
    function focusSearch() {}
    function clearSearch() { return false }

    signal message(string text, var link)

    readonly property var heroic: Library.heroic

    function set(key, value) {
        error.text = Library.setHeroicValue(key, value)
        return error.text === ""
    }

    function setWholeLibrary(on) {
        if (set("whole_library", on) && !on) {
            message(qsTr("The games that aren't installed are gone from the library"), {
                text: qsTr("Undo"),
                isUndo: true,
                run: () => root.setWholeLibrary(true)
            })
        }
    }

    component SwitchRow: RowLayout {
        property alias label: name.text
        property alias checked: control.checked
        signal toggled(bool on)

        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        Label {
            id: name
            Layout.fillWidth: true
            color: Theme.controls.labelColor
            font.pixelSize: Theme.controls.labelFontSize
        }
        SettingsSwitch {
            id: control
            onToggled: parent.toggled(checked)
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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.settingsWindow.contentPadding
        spacing: Theme.layout.spacing

        Label {
            id: error
            visible: text !== ""
            Layout.fillWidth: true
            color: Theme.controls.problemsText
            font.pixelSize: Theme.controls.descriptionFontSize
            wrapMode: Text.WordWrap
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: content.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: SettingsScrollBar { id: scrollBar }

            ColumnLayout {
                id: content
                width: flick.width - scrollBar.width - Theme.layout.spacing
                spacing: Theme.layout.spacing

                SettingsNote {
                    Layout.fillWidth: true
                    text: qsTr("Heroic stays in charge of installing, updating, Wine and cloud saves. "
                               + "Lattis only reads its files, and changes nothing in them.")
                }
                SettingsCard {
                    SwitchRow {
                        label: qsTr("Import Epic games")
                        checked: root.heroic.enabled
                        onToggled: on => root.set("enabled", on)
                    }
                    SettingsNote {
                        Layout.fillWidth: true
                        text: root.heroic.status
                        color: root.heroic.problem ? Theme.controls.problemsText : Theme.controls.descriptionColor
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.layout.spacing
                        enabled: root.heroic.enabled
                        spacing: Theme.layout.spacing / 2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.layout.spacing

                            Label {
                                text: qsTr("Folder")
                                color: Theme.controls.labelColor
                                font.pixelSize: Theme.controls.labelFontSize
                            }
                            Label {
                                visible: root.heroic.found && root.heroic.path === ""
                                Layout.fillWidth: true
                                text: qsTr("found automatically")
                                color: Theme.settingsWindow.markerColor
                                font.pixelSize: Theme.settingsWindow.markerFontSize
                                elide: Text.ElideRight
                            }
                        }
                        PathField {
                            Layout.fillWidth: true
                            path: root.heroic.found ? root.heroic.root : root.heroic.path
                            onCopied: root.message(qsTr("Copied"), undefined)
                            missingText: root.heroic.enabled ? qsTr("Not found") : qsTr("Not looked for")
                            missingColor: root.heroic.problem ? Theme.controls.problemsText
                                                              : Theme.controls.descriptionColor

                            SettingsButton {
                                visible: root.heroic.path !== ""
                                text: qsTr("Find automatically")
                                tip: qsTr("Look in ~/.config/heroic, then the Flatpak's folder")
                                onClicked: root.set("path", "")
                            }
                            SettingsButton {
                                text: qsTr("Change")
                                tip: qsTr("Choose Heroic's folder: the one with store_cache in it")
                                onClicked: folderDialog.active = true
                            }
                        }
                    }
                }

                SettingsCard {
                    enabled: root.heroic.enabled

                    SwitchRow {
                        label: qsTr("Import the whole library")
                        checked: root.heroic.wholeLibrary
                        onToggled: on => root.setWholeLibrary(on)
                    }
                    SettingsNote {
                        Layout.fillWidth: true
                        text: qsTr("Also list the games you own that aren't installed, from what Heroic last read "
                                   + "from your account. They show as not installed, and Install hands them to Heroic.")
                    }
                }

                SettingsButton {
                    enabled: root.heroic.found && root.heroic.programFound
                    text: qsTr("Open Heroic")
                    onClicked: error.text = GameRunner.openHeroic()
                }
            }
        }
    }
}
