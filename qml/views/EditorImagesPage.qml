import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// A tab of GameEditor; the editor keeps the state and reads these controls.
ColumnLayout {
    required property var editor
    property alias artEditor: artEditor
    property alias mediaMenu: mediaMenu

    Layout.fillWidth: true
    Layout.fillHeight: false // as tall as its cards; layouts fill by default
    Layout.alignment: Qt.AlignTop
    spacing: Theme.gameEditor.cardSpacing

    EditorCard {
        title: qsTr("Images")
        actions: EditorTextAction {
            text: qsTr("Browse SteamGridDB")
            iconPath: Icons.image
            clickable: Metadata.hasSteamGridDbKey
            tip: qsTr("Pick a hero, cover, logo and icon from SteamGridDB and download them together")
            disabledTip: qsTr("Add your SteamGridDB key in Settings → Metadata")
            onClicked: editor.chooseArt()
        }

        ArtEditor {
            id: artEditor
            Layout.fillWidth: true
        }
        Label {
            Layout.fillWidth: true
            visible: !editor.adding
            text: qsTr("Right-click the logo on the game's page to resize or move it.")
            color: Theme.gameEditor.hintColor
            wrapMode: Text.WordWrap
        }
    }

    EditorCard {
        title: qsTr("Media folders")
        visible: editor.mediaGame

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: Theme.controls.inputHeight
                radius: Theme.controls.inputRadius
                color: Theme.controls.inputBackground
                border.color: Theme.controls.inputBorder
                border.width: Theme.controls.inputBorderWidth

                Label {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    verticalAlignment: Text.AlignVCenter
                    readonly property int count: editor.mediaFoldersShown.length
                    readonly property string folders: count === 1 ? qsTr("1 folder")
                                                                  : qsTr("%1 folders").arg(count)
                    text: editor.onSteam ? (count === 0 ? qsTr("Steam's") : qsTr("Steam's + %1").arg(folders))
                        : editor.romGame ? (count === 0 ? qsTr("Emulator's") : qsTr("Emulator's + %1").arg(folders))
                                         : (count === 0 ? qsTr("None") : folders)
                    color: Theme.controls.inputText
                    font.pixelSize: Theme.gameEditor.pathFontSize
                    elide: Text.ElideRight
                }
            }
            SettingsButton {
                id: mediaEdit
                iconPath: Icons.editLine
                tip: qsTr("Choose the media folders")
                onClicked: mediaMenu.opened ? mediaMenu.close() : mediaMenu.open()

                Popup {
                    id: mediaMenu

                    x: mediaEdit.width - width
                    y: mediaEdit.height + 4
                    width: Theme.gameEditor.mediaPopupWidth
                    padding: Theme.menu.padding
                    property real pop: 1
                    property real slide: 0
                    scale: pop
                    transformOrigin: Popup.TopRight
                    // Pressing the button must not close it before the click toggles it, or the click reopens it.
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                    enter: MenuPopEnter {}
                    exit: MenuPopExit {}

                    // Faded as separate items, the rows would show over what's behind the
                    // see-through panel, so it closes as one flattened image.
                    function flatten(on) {
                        const popupItem = background?.parent
                        if (popupItem) {
                            popupItem.layer.smooth = true
                            popupItem.layer.enabled = on
                        }
                    }
                    onAboutToShow: flatten(false)
                    onAboutToHide: flatten(true)
                    onClosed: flatten(false)

                    background: Rectangle {
                        transform: Translate { y: mediaMenu.slide }
                        radius: Theme.controls.inputRadius
                        color: Theme.menu.popupBackground
                        border.color: Theme.controls.inputBorder
                        border.width: Math.max(1, Theme.controls.inputBorderWidth)
                    }
                    contentItem: ColumnLayout {
                        transform: Translate { y: mediaMenu.slide }
                        spacing: 0

                        EditorMediaFolderRow {
                            visible: editor.onSteam
                            steam: true
                            exists: true
                            onOpenClicked: {
                                if (!Library.showSteamMediaFolder(editor.game.id))
                                    editor.message(qsTr("Steam's folder isn't found"))
                            }
                        }
                        Repeater {
                            model: editor.mediaFoldersShown

                            EditorMediaFolderRow {
                                required property string modelData
                                path: modelData
                                exists: GameRunner.folderExists(modelData)
                                onOpenClicked: {
                                    const e = GameRunner.openPath(modelData)
                                    if (e !== "")
                                        editor.message(e)
                                }
                                onRemoveClicked: editor.newMediaFolders =
                                    editor.mediaFoldersShown.filter(f => f !== modelData)
                            }
                        }
                        Rectangle {
                            visible: editor.onSteam || editor.mediaFoldersShown.length > 0
                            Layout.fillWidth: true
                            Layout.topMargin: Theme.menu.padding
                            Layout.bottomMargin: Theme.menu.padding
                            implicitHeight: 1
                            color: Theme.settingsWindow.rowDivider
                        }
                        AbstractButton {
                            id: addFolder
                            Layout.fillWidth: true
                            implicitHeight: Theme.menu.itemHeight
                            hoverEnabled: true
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            onClicked: editor.folderPickerLoader.pickFor("media")
                            background: Rectangle {
                                radius: Theme.menu.itemRadius
                                color: addFolder.hovered ? Theme.controls.hover
                                                         : Qt.alpha(Theme.controls.hover, 0)
                                Behavior on color { enabled: Motion.fades; MotionColor {} }
                            }
                            contentItem: RowLayout {
                                spacing: Theme.controls.controlSpacing

                                Item { Layout.preferredWidth: 8 }
                                Icon {
                                    path: Icons.add
                                    size: Theme.controls.iconSize
                                    color: Theme.palette.text
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Add folder")
                                    color: Theme.palette.text
                                }
                            }
                        }
                    }
                }
            }
        }
        EditorHint {
            text: editor.onSteam
                  ? qsTr("Pictures and videos shown in the game's Media row and the Gallery: Steam's own "
                         + "clips and screenshots, plus every picture and video in the folders you add "
                         + "(OBS recordings, a screenshot tool's folder) and their subfolders.")
                  : qsTr("Pictures and videos shown in the game's Media row and the Gallery: every "
                         + "picture and video in the folders you add (OBS recordings, a screenshot "
                         + "tool's folder) and their subfolders.")
        }
    }
}
