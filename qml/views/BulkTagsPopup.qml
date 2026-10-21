import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts

MotionPopup {
    id: root

    required property var lookup
    property Item anchorItem
    property string query: ""
    readonly property string needle: query.trim().toLowerCase()
    readonly property var matches: lookup.allTags.filter(t => needle === "" || t.toLowerCase().includes(needle))
    readonly property color matchColor: Qt.lighter(Theme.segmented.checkedText, Theme.segmented.lightBrightness)

    function openFor(button, search) {
        anchorItem = button
        field.text = search ?? ""
        const at = button.mapToItem(Overlay.overlay, 0, 0)
        x = at.x + button.width + Theme.layout.spacing
        y = Math.max(Theme.layout.spacing, Math.min(at.y - Theme.layout.spacing,
                                                    Overlay.overlay.height - implicitHeight - Theme.layout.spacing))
        open()
    }

    function colorOf(name) {
        const palette = Theme.tags.palette
        return palette.length > 0 ? palette[(Library.tags.find(t => t.name === name)?.color ?? 0) % palette.length]
                                  : Theme.palette.surfaceRaised
    }

    function toggle(name) {
        lookup.tags = lookup.tags.includes(name) ? lookup.tags.filter(t => t !== name) : lookup.tags.concat([name])
    }

    function highlighted(name) {
        const escape = s => s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
        const at = needle === "" ? -1 : name.toLowerCase().indexOf(needle)
        if (at < 0)
            return escape(name)
        return escape(name.slice(0, at)) + "<font color=\"" + matchColor + "\"><b>" + escape(name.slice(at, at + needle.length))
               + "</b></font>" + escape(name.slice(at + needle.length))
    }

    parent: Overlay.overlay
    width: Theme.gameEditor.bulkTagPopupWidth
    padding: Theme.layout.spacing
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    // Dims nothing: the list behind shows what the tags pick.
    Overlay.modal: Item {}
    onOpened: field.forceActiveFocus()
    onClosed: anchorItem?.forceActiveFocus()

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
            placeholderText: qsTr("Search tags")
            onTextChanged: root.query = text
            onAccepted: {
                if (root.needle !== "" && root.matches.length > 0) {
                    root.toggle(root.matches[0])
                    selectAll()
                }
            }
            Keys.onDownPressed: {
                list.currentIndex = 0
                list.forceActiveFocus()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, Theme.gameEditor.bulkTagListHeight)
            clip: true
            model: root.matches
            currentIndex: -1
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: SettingsScrollBar { thickness: Theme.gameEditor.bulkScrollbarWidth; trackless: true }
            Keys.onUpPressed: {
                if (currentIndex <= 0)
                    field.forceActiveFocus()
                else
                    decrementCurrentIndex()
            }
            Keys.onDownPressed: incrementCurrentIndex()
            Keys.onReturnPressed: if (currentIndex >= 0) root.toggle(root.matches[currentIndex])
            Keys.onSpacePressed: if (currentIndex >= 0) root.toggle(root.matches[currentIndex])

            delegate: BulkOption {
                id: option
                required property string modelData
                required property int index
                width: list.width - Theme.gameEditor.bulkScrollbarWidth + Theme.layout.spacing / 2
                text: root.highlighted(modelData)
                textFormat: Text.StyledText
                checked: root.lookup.tags.includes(modelData)
                swatch: root.colorOf(modelData)
                count: String(root.lookup.tagCounts[modelData] ?? 0)
                dimmed: (root.lookup.tagCounts[modelData] ?? 0) === 0
                highlighted: (index === 0 && root.needle !== "" && !list.activeFocus)
                             || (list.activeFocus && list.currentIndex === index)
                onClicked: root.toggle(modelData)
            }

            Label {
                anchors.centerIn: parent
                visible: root.matches.length === 0
                color: Theme.gameEditor.hintColor
                text: qsTr("No tag called \"%1\".").arg(root.query.trim())
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.controls.inputBorder
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.layout.spacing
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                color: Theme.gameEditor.hintColor
                font.pixelSize: Theme.gameEditor.hintFontSize
                elide: Text.ElideRight
                text: root.lookup.tags.length > 0 ? qsTr("Games with any of %1").arg(root.lookup.tags.length)
                                                  : qsTr("%1 tags").arg(root.lookup.allTags.length)
            }
            TextLink {
                text: qsTr("Clear")
                enabled: root.lookup.tags.length > 0
                onClicked: {
                    root.lookup.tags = []
                    field.forceActiveFocus()
                }
            }
            SettingsButton {
                implicitHeight: Theme.controls.buttonHeight
                leftPadding: Theme.layout.spacing * 1.25
                rightPadding: leftPadding
                text: qsTr("Done")
                onClicked: root.close()
            }
        }
    }
}
