import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// A tab of GameEditor; the editor keeps the state and reads these controls.
ColumnLayout {
    required property var editor
    property alias titleField: titleField
    property alias descriptionArea: descriptionArea
    property alias yearField: yearField
    property alias rating: rating
    property alias favoriteSwitch: favoriteSwitch
    property alias hoursField: hoursField
    property alias minutesField: minutesField
    property alias lastPlayedEditor: lastPlayedEditor
    property alias statusBox: statusBox
    property alias tagsEditor: tagsEditor

    Layout.fillWidth: true
    Layout.fillHeight: false // as tall as its cards; layouts fill by default
    Layout.alignment: Qt.AlignTop
    spacing: Theme.gameEditor.cardSpacing

    EditorCard {
        title: qsTr("About")
        actions: EditorTextAction {
            text: editor.lookupStatus.phase === "running" ? qsTr("Cancel lookup") : qsTr("Look up in IGDB")
            iconPath: Icons.search
            clickable: Metadata.hasIgdbKeys && titleField.text.trim() !== ""
            tip: qsTr("Fill in the title, description and year from IGDB, and add the tags that fit")
            disabledTip: Metadata.hasIgdbKeys ? "" : qsTr("Add your API keys in Settings → Metadata")
            onClicked: editor.lookUp()
        }

        EditorField {
            label: qsTr("Title")
            marked: editor.fromLookup("title", titleField.text)

            SettingsTextField {
                id: titleField
                Layout.fillWidth: true
                onAccepted: editor.save()
            }
        }

        EditorField {
            label: qsTr("Description")
            marked: editor.fromLookup("description", descriptionArea.text)

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.gameEditor.descriptionHeight

                TextArea {
                    id: descriptionArea
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    color: Theme.controls.inputText
                    placeholderText: qsTr("A paragraph or two about the game")
                    placeholderTextColor: Theme.controls.placeholderColor
                    // Tab moves on, as in the other fields, instead of typing a tab.
                    KeyNavigation.tab: yearField
                    KeyNavigation.backtab: titleField
                    HoverHandler { id: descriptionPointer }
                    ContextMenu.menu: TextContextMenu { field: descriptionArea; pointer: descriptionPointer }
                    background: Rectangle {
                        radius: Theme.controls.inputRadius
                        color: Theme.controls.inputBackground
                        border.color: descriptionArea.activeFocus ? Theme.controls.inputBorderFocus
                                                                  : Theme.controls.inputBorder
                        border.width: descriptionArea.activeFocus ? Theme.controls.inputBorderFocusWidth
                                                                  : Theme.controls.inputBorderWidth
                    }
                }
            }
        }
    }

    EditorCard {
        title: qsTr("Details")

        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: Theme.gameEditor.detailsColumnSpacing
            rowSpacing: Theme.gameEditor.rowSpacing

            EditorField {
                Layout.fillWidth: false
                label: qsTr("Year")
                marked: editor.fromLookup("year", yearField.text)

                SettingsTextField {
                    id: yearField
                    Layout.preferredWidth: Theme.gameEditor.shortFieldWidth
                    placeholderText: qsTr("Unknown")
                    validator: IntValidator { bottom: 1; top: 9999 }
                    onAccepted: editor.save()
                }
            }
            EditorField {
                Layout.fillWidth: false
                label: qsTr("Rating")
                labelTip: qsTr("Double-click the stars to clear the rating")
                marked: editor.fromLookup("rating", editor.ratingText(rating.value))
                    || (!editor.adding && editor.game.ratingSource === "igdb" && rating.value === editor.game.rating)

                RatingEditor {
                    id: rating
                    Layout.preferredHeight: yearField.implicitHeight
                }
            }
            EditorField {
                label: qsTr("Favorite")

                SettingsSwitch {
                    id: favoriteSwitch
                    Layout.preferredHeight: yearField.implicitHeight
                }
            }

            EditorField {
                Layout.fillWidth: false
                label: qsTr("Play time")
                marked: editor.onSteam
                markerText: qsTr("from Steam")

                RowLayout {
                    spacing: Theme.layout.spacing
                    enabled: !editor.onSteam
                    opacity: editor.onSteam ? Theme.gameEditor.readOnlyOpacity : 1

                    StepField {
                        id: hoursField
                        minDigits: 4
                        placeholderText: "0"
                        // The most that still fits play_minutes with 59 minutes.
                        validator: IntValidator { bottom: 0; top: 35791393 }
                        onStepped: direction => editor.setPlayMinutes(editor.playMinutes() + 60 * direction)
                        onEditingFinished: editor.setPlayMinutes(editor.playMinutes())
                        onAccepted: editor.save()
                    }
                    Label {
                        text: qsTr("h")
                        color: Theme.gameEditor.labelColor
                    }
                    StepField {
                        id: minutesField
                        Layout.leftMargin: Theme.layout.spacing
                        placeholderText: "00"
                        validator: IntValidator { bottom: 0; top: 59 }
                        onStepped: direction => editor.setPlayMinutes(editor.playMinutes() + direction)
                        onEditingFinished: editor.setPlayMinutes(editor.playMinutes())
                        onAccepted: editor.save()
                    }
                    Label {
                        text: qsTr("m")
                        color: Theme.gameEditor.labelColor
                    }
                }
            }
            EditorField {
                Layout.fillWidth: false
                label: qsTr("Last played")
                marked: editor.onSteam
                markerText: qsTr("from Steam")

                DateEditor {
                    id: lastPlayedEditor
                    enabled: !editor.onSteam
                    opacity: editor.onSteam ? Theme.gameEditor.readOnlyOpacity : 1
                    Layout.preferredHeight: yearField.implicitHeight
                    onAccepted: editor.save()
                }
            }
            EditorField {
                label: qsTr("Status")

                SettingsComboBox {
                    id: statusBox
                    Layout.preferredHeight: yearField.implicitHeight
                    Layout.fillWidth: true
                    model: editor.statuses.map(s => s.name)
                }
            }
        }
    }

    EditorCard {
        title: qsTr("Tags")
        actions: EditorTextAction {
            text: qsTr("New tag")
            iconPath: Icons.add
            tip: qsTr("Create a tag and add it to this game")
            onClicked: tagsEditor.newTag()
        }

        TagsEditor {
            id: tagsEditor
            Layout.fillWidth: true
        }
    }
}
