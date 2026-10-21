import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts

ColumnLayout {
    id: root

    required property string half
    readonly property bool isColors: half === "colors"
    readonly property string activeName: isColors ? Theme.activeColors : Theme.activeStyle
    readonly property var names: isColors ? Theme.availableColors : Theme.availableStyles

    property bool canReset: false
    property bool canDelete: false
    property int revision: 0

    signal newRequested()
    signal openFileRequested()
    signal resetRequested()
    signal renameRequested()
    signal deleteRequested()
    signal descriptionRequested()

    function refresh() {
        canReset = Theme.canResetToBuiltIn(half)
        canDelete = !(isColors ? Theme.builtInColors : Theme.builtInStyles).includes(activeName)
        revision++
        list.positionViewAtIndex(Math.max(0, names.indexOf(activeName)), ListView.Contain)
    }

    onActiveNameChanged: refresh()
    Component.onCompleted: refresh()

    spacing: Theme.controls.controlSpacing

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.controls.controlSpacing

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.settingsWindow.themePopupPadding
            text: root.isColors ? qsTr("Colors") : qsTr("Style")
            color: Theme.settingsWindow.themePopupHeaderColor
            font.pixelSize: Theme.settingsWindow.themePopupHeaderFontSize
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.8
        }

        SettingsButton {
            id: moreButton
            iconPath: Icons.more
            textColor: Theme.settingsWindow.themePopupHeaderColor
            flat: true
            implicitHeight: Theme.settingsWindow.themePopupButtonHeight
            leftPadding: 6
            rightPadding: 6
            enabled: root.canReset || root.canDelete
            tip: qsTr("Reset, rename or delete")
            onClicked: moreMenu.opened ? moreMenu.close() : moreMenu.open()

            SettingsMenu {
                id: moreMenu
                x: moreButton.width - width
                y: moreButton.height
                pops: true
                transformOrigin: Popup.TopRight
                // The button toggles it; a press on it must not close and reopen it.
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                SettingsMenuItem {
                    visible: root.canReset
                    text: qsTr("Reset to built-in")
                    onTriggered: root.resetRequested()
                }
                SettingsMenuItem {
                    visible: root.canDelete
                    text: qsTr("Rename")
                    onTriggered: root.renameRequested()
                }
                SettingsMenuItem {
                    visible: root.canDelete && !root.isColors
                    text: qsTr("Edit description")
                    onTriggered: root.descriptionRequested()
                }
                SettingsMenuItem {
                    visible: root.canDelete
                    danger: true
                    text: qsTr("Delete")
                    onTriggered: root.deleteRequested()
                }
            }
        }
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.settingsWindow.themePopupListHeight
        model: root.names
        spacing: Theme.settingsWindow.themePopupRowSpacing
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: SettingsScrollBar { thickness: Theme.settingsWindow.themePopupScrollbarWidth }

        readonly property real fadeBottom: Math.min(Theme.settingsWindow.themePopupFadeHeight,
                                                    Math.max(0, contentHeight + originY - contentY - height))
        layer.enabled: fadeBottom > 0
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: fadeMask
            // Without a threshold and spread the mask cuts at any alpha above 0 instead of following it.
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1
        }

        delegate: Rectangle {
            id: row

            required property string modelData
            readonly property bool active: modelData === root.activeName
            readonly property var preview: {
                root.revision
                return root.isColors ? Theme.preview(modelData, Theme.activeStyle) : Theme.preview(Theme.activeColors, modelData)
            }
            readonly property string text: {
                root.revision
                return root.isColors ? "" : (Theme.styleDescription(modelData) || Theme.styleSummary(modelData))
            }

            width: ListView.view.width - scrollBarGap
            readonly property real scrollBarGap: list.contentHeight > list.height ? Theme.settingsWindow.themePopupScrollbarWidth + Theme.controls.controlSpacing : 0
            implicitHeight: content.implicitHeight + 2 * Theme.settingsWindow.themePopupPadding
            height: implicitHeight
            radius: Theme.settingsWindow.themePopupRowRadius
            color: active ? Theme.settingsWindow.themePopupSelectedBackground
                          : hover.hovered ? Theme.controls.hover : Qt.alpha(Theme.controls.hover, 0)
            border.color: Theme.settingsWindow.themePopupSelectedBorder
            border.width: active ? Theme.settingsWindow.themePopupSelectedBorderWidth : 0

            Behavior on color { enabled: Motion.fades; MotionColor {} }

            HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: root.isColors ? Theme.setColors(row.modelData) : Theme.setStyle(row.modelData) }

            ColumnLayout {
                id: content
                anchors.fill: parent
                anchors.margins: Theme.settingsWindow.themePopupPadding
                spacing: Theme.settingsWindow.themePopupRowSpacing + 3

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.controls.controlSpacing

                    Label {
                        Layout.fillWidth: true
                        text: row.modelData
                        color: Theme.controls.inputText
                        elide: Text.ElideRight
                    }

                    Row {
                        visible: root.isColors
                        spacing: Theme.settingsWindow.themePopupSwatchSpacing

                        Repeater {
                            model: root.isColors ? [row.preview.background, row.preview.accent,
                                                    row.preview.launchRunning, row.preview.launchInstall] : []

                            Rectangle {
                                required property var modelData
                                width: Theme.settingsWindow.themePopupSwatchSize
                                height: width
                                radius: width / 2
                                color: modelData ?? "transparent"
                                border.color: Theme.settingsWindow.rowDivider
                                border.width: Theme.settingsWindow.themePopupSwatchBorderWidth
                            }
                        }
                    }
                }

                Label {
                    visible: !root.isColors
                    Layout.fillWidth: true
                    text: row.text
                    color: Theme.controls.descriptionColor
                    font.pixelSize: Theme.settingsWindow.themePopupDescriptionFontSize
                    elide: Text.ElideRight
                }

                StyleScene {
                    visible: !root.isColors
                    Layout.fillWidth: true
                    preview: row.preview
                }
            }
        }
    }

    Rectangle {
        id: fadeMask
        visible: false
        layer.enabled: true
        width: list.width
        height: list.height
        gradient: Gradient {
            GradientStop { position: 0; color: "white" }
            GradientStop { position: list.height > 0 ? 1 - list.fadeBottom / list.height : 1; color: "white" }
            GradientStop { position: 1; color: list.fadeBottom > 0 ? "transparent" : "white" }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.controls.controlSpacing

        SettingsButton {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            implicitHeight: Theme.settingsWindow.themePopupButtonHeight
            iconPath: Icons.add
            text: qsTr("New")
            tip: Actions.tip(root.isColors ? qsTr("Make your own colors, based on another") : qsTr("Make your own style, based on another"),
                             (root.isColors ? Actions.keys.new_colors : Actions.keys.new_style) ?? [])
            onClicked: root.newRequested()
        }
        SettingsButton {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            implicitHeight: Theme.settingsWindow.themePopupButtonHeight
            iconPath: Icons.fileTextLine
            text: qsTr("Open file")
            tip: Actions.tip(root.isColors ? qsTr("Edit the colors file in your editor") : qsTr("Edit the style file in your editor"),
                             (root.isColors ? Actions.keys.open_colors_file : Actions.keys.open_style_file) ?? [])
            onClicked: root.openFileRequested()
        }
    }
}
