import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts

MotionPopup {
    id: root

    required property var editor
    readonly property string typed: field.text.trim()
    readonly property string needle: typed.toLowerCase()
    readonly property var libraryTags: editor.libraryTags
    readonly property var suggestions: libraryTags.filter(t => !editor.tags.some(c => c.name === t.name)
                                                             && t.name.toLowerCase().includes(needle))
    readonly property bool canCreate: typed !== "" && !libraryTags.some(t => t.name === typed)
                                      && !editor.tags.some(t => t.name === typed)
    readonly property string caseTwin: {
        if (typed === "" || libraryTags.some(t => t.name === typed))
            return ""
        return libraryTags.find(t => t.name.toLowerCase() === needle)?.name ?? ""
    }

    function openFor(chip) {
        parent = chip
        x = 0
        y = chip.height + Theme.layout.spacing / 2
        field.text = ""
        open()
    }

    function add(name) {
        editor.add(name)
        field.text = ""
        field.forceActiveFocus()
    }

    function accept() {
        if (typed === "")
            return
        if (canCreate || suggestions.some(t => t.name === typed))
            add(typed)
        else if (suggestions.length > 0)
            add(suggestions[0].name)
    }

    width: Theme.gameEditor.tagPopupWidth
    padding: Theme.layout.spacing
    transformOrigin: Popup.TopLeft
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: field.forceActiveFocus()

    background: Item {
        RectangularShadow {
            visible: Theme.gameEditor.bulkPopupShadowSize > 0
            anchors.fill: parent
            offset.y: Theme.gameEditor.bulkPopupShadowSize / 6
            radius: Theme.gameEditor.cardRadius
            blur: Theme.gameEditor.bulkPopupShadowSize
            color: Theme.gameEditor.bulkPopupShadowColor
        }
        Rectangle {
            anchors.fill: parent
            radius: Theme.gameEditor.cardRadius
            color: Theme.menu.popupBackground
            border.color: Theme.gameEditor.cardBorder
            border.width: Theme.gameEditor.cardBorderWidth
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.layout.spacing

        SettingsSearchField {
            id: field
            Layout.fillWidth: true
            placeholderText: qsTr("Search or create a tag")
            onAccepted: root.accept()
        }

        Flickable {
            id: scroll
            visible: root.suggestions.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, Theme.gameEditor.tagPopupListHeight)
            contentWidth: width
            contentHeight: others.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: SettingsScrollBar { thickness: Theme.gameEditor.bulkScrollbarWidth; trackless: true }

            Flow {
                id: others
                width: scroll.width - Theme.gameEditor.bulkScrollbarWidth - Theme.layout.spacing / 2
                spacing: Theme.tags.spacing

                Repeater {
                    model: root.suggestions

                    TagChip {
                        id: suggestion
                        required property var modelData
                        name: modelData.name
                        colorIndex: modelData.color
                        outlined: true
                        outlineWidth: Theme.filters.tagBorderWidth
                        plus: false
                        textColor: Qt.hsla(suggestion.tagColor.hslHue, suggestion.tagColor.hslSaturation,
                                           Theme.filters.tagTextLightness, 1)
                        fillOpacity: suggestionMouse.containsMouse ? Theme.gameEditor.suggestionHoverFillOpacity
                                                                   : Theme.gameEditor.suggestionFillOpacity
                        raised: suggestionMouse.containsMouse
                        raiseBy: Theme.gameEditor.tagHoverLift
                        pressed: suggestionMouse.pressed

                        MouseArea {
                            id: suggestionMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.add(suggestion.modelData.name)
                        }
                    }
                }
            }
        }

        TagChip {
            id: create
            visible: root.canCreate
            Layout.alignment: Qt.AlignLeft
            Layout.maximumWidth: parent.width
            name: qsTr("Create \"%1\"").arg(root.typed)
            colorIndex: root.editor.nextColor()
            outlined: true
            outlineWidth: Theme.filters.tagBorderWidth
            textColor: Qt.hsla(tagColor.hslHue, tagColor.hslSaturation, Theme.filters.tagTextLightness, 1)
            fillOpacity: createMouse.containsMouse ? Theme.gameEditor.suggestionHoverFillOpacity
                                                   : Theme.gameEditor.suggestionFillOpacity
            raised: createMouse.containsMouse
            raiseBy: Theme.gameEditor.tagHoverLift
            pressed: createMouse.pressed

            MouseArea {
                id: createMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.add(root.typed)
            }
        }

        Label {
            visible: root.caseTwin !== ""
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.warningColor
            font.pixelSize: Theme.gameEditor.hintFontSize
            text: qsTr("There's already a tag called %1; tags are case-sensitive, so this makes %2 another one.")
                      .arg(root.caseTwin).arg(root.typed)
        }

        Label {
            visible: root.suggestions.length === 0 && !root.canCreate
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.gameEditor.hintColor
            font.pixelSize: Theme.gameEditor.hintFontSize
            text: root.typed !== "" ? qsTr("None match \"%1\"").arg(root.typed)
                                    : qsTr("Type a name to create a tag")
        }
    }
}
