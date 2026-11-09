import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
MotionPopup {
    id: root

    signal closedByKey()
    signal collectionDialogRequested(string mode, string id, string name)
    signal collectionDeleted(string id)
    signal steamImportRequested()

    width: Theme.collections.popupWidth
    padding: Theme.filters.padding
    modal: false
    focus: true
    closePolicy: Popup.CloseOnPressOutsideParent

    onOpened: root.forceActiveFocus(Qt.PopupFocusReason)

    background: Surface {
        radius: Theme.filters.radius
        fill: Theme.filters.background
        fillEnd: Theme.filters.backgroundEnd
        direction: Theme.filters.gradientDirection
        border.color: Theme.filters.border
        border.width: Theme.filters.borderWidth
    }

    component PressDip: Item {
        id: dip
        property bool pressed
        property real press: pressed ? Theme.motion.pressScale : 1
        Behavior on press { enabled: Motion.on; MotionNumber {} }
        Binding { target: dip.parent; property: "scale"; value: dip.press }
    }

    component FocusRing: Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: Theme.filters.sortRadius + 3
        color: "transparent"
        border.color: Theme.filters.focusRing
        border.width: Theme.filters.focusRingWidth
        property var control: parent
        visible: control?.visualFocus ?? false
    }

    component Heading: Label {
        property string title
        text: Theme.filters.headingUppercase ? title.toUpperCase() : title
        color: Theme.filters.headingColor
        font.weight: Theme.font.strongWeight
        font.pixelSize: Theme.filters.headingFontSize
        font.letterSpacing: Theme.filters.headingUppercase ? 0.8 : 0
    }

    component CollectionRow: AbstractButton {
        id: row
        property bool selected: false
        property string detail
        property string iconPath
        signal menuRequested(real x, real y)

        width: parent?.width ?? 0
        implicitHeight: Theme.collections.rowHeight
        leftPadding: Theme.layout.spacing
        rightPadding: Theme.layout.spacing
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true

        PressDip { pressed: row.pressed }
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: eventPoint => row.menuRequested(eventPoint.position.x, eventPoint.position.y)
        }
        Keys.onMenuPressed: row.menuRequested(row.width / 2, row.height)
        ToolTip.visible: hovered && nameLabel.truncated
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: row.text

        background: Rectangle {
            radius: Theme.collections.rowRadius
            color: row.selected ? Theme.collections.rowSelected
                 : row.hovered ? Theme.collections.rowHover : Theme.collections.rowBackground
        }
        contentItem: RowLayout {
            spacing: Theme.layout.spacing

            Icon {
                visible: row.iconPath !== ""
                path: row.iconPath
                size: Theme.controls.iconSize
                color: row.selected ? Theme.collections.rowSelectedText : Theme.collections.rowText
            }
            Label {
                id: nameLabel
                Layout.fillWidth: true
                text: row.text
                elide: Text.ElideRight
                font.weight: row.selected ? Theme.font.onAccentWeight : Font.Normal
                color: row.selected ? Theme.collections.rowSelectedText : Theme.collections.rowText
            }
            Label {
                text: row.detail
                font.weight: Theme.font.emphasisWeight
                color: row.selected ? Theme.collections.rowSelectedText : Theme.collections.rowCountColor
                opacity: row.selected ? 0.75 : 1
            }
        }
        FocusRing {}
    }

    contentItem: ColumnLayout {
        spacing: Theme.layout.spacing

        Keys.onEscapePressed: {
            root.close()
            root.closedByKey()
        }

        Heading { title: qsTr("Collections") }
        Flickable {
            id: collectionsFlick
            visible: Library.collections.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(collectionRows.implicitHeight, Theme.collections.listMaxHeight)
            contentHeight: collectionRows.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height

            Grid {
                id: collectionRows
                width: collectionsFlick.width
                columns: 2
                spacing: Theme.collections.rowSpacing

                Repeater {
                    model: Library.collections

                    CollectionRow {
                        required property var modelData
                        width: (collectionRows.width - collectionRows.spacing) / 2
                        text: modelData.name
                        detail: modelData.count
                        selected: Library.collectionId === modelData.id
                        onClicked: Library.openCollection(modelData.id)
                        onMenuRequested: (x, y) => {
                            collectionMenu.collection = modelData
                            collectionMenu.popup(this, x, y)
                        }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            CollectionRow {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                iconPath: Icons.add
                text: qsTr("New collection")
                onClicked: root.collectionDialogRequested("new", "", "")
            }
            CollectionRow {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                visible: Library.steam.found
                iconPath: Icons.steamLine
                text: qsTr("Import from Steam")
                onClicked: root.steamImportRequested()
            }
        }

        SettingsMenu {
            id: collectionMenu
            property var collection: ({})
            atPointer: true

            SettingsMenuItem {
                iconPath: Icons.editLine
                text: qsTr("Rename")
                onTriggered: root.collectionDialogRequested("rename", collectionMenu.collection.id,
                                                            collectionMenu.collection.name)
            }
            SettingsMenuItem {
                iconPath: Icons.gear
                text: qsTr("Edit")
                onTriggered: root.collectionDialogRequested("edit", collectionMenu.collection.id,
                                                            collectionMenu.collection.name)
            }
            SettingsMenuItem {
                iconPath: Icons.deleteBin6Line
                text: qsTr("Delete")
                danger: true
                onTriggered: root.collectionDeleted(collectionMenu.collection.id)
            }
        }
    }
}
