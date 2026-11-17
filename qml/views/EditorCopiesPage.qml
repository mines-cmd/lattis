import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// A tab of GameEditor; the editor keeps the state and reads these controls.
ColumnLayout {
    id: page

    required property var editor
    readonly property var rows: editor.copyRowList
    readonly property bool linked: rows.length > 0
    // False until the page has been built, so the rows that are there at the start don't fade in.
    property bool armed: false
    readonly property var candidates: {
        editor.copyInfo
        if (editor.adding || editor.game.id === undefined)
            return []
        return Library.sameGameCandidates(editor.game.id).filter(c => !rows.some(r => r.id === c.id))
    }

    Component.onCompleted: Qt.callLater(() => { armed = true })

    function together() {
        // A group that doesn't exist yet has no combined numbers until Save makes it.
        if (editor.copyBase.length === 0)
            return qsTr("The combined numbers show once you save.")
        const time = editor.copyInfo.totalPlayTimeText ?? ""
        const last = editor.copyInfo.totalLastPlayedText ?? ""
        const text = time !== "" && last !== "" ? qsTr("Together: %1, last played %2 · media shared").arg(time).arg(last)
                   : time !== "" ? qsTr("Together: %1 · media shared").arg(time)
                   : qsTr("Together: not played yet · media shared")
        return editor.copiesDirty ? qsTr("%1 (updates when you save)").arg(text) : text
    }

    component CopyRow: Rectangle {
        id: row

        property url coverUrl
        property string title
        property string meta
        property bool main: false
        property bool current: false
        property real contentOpacity: 1
        default property alias trailing: trailingRow.data

        Layout.fillWidth: true
        implicitHeight: rowContent.implicitHeight + 2 * Theme.layout.spacing
        radius: Theme.gameEditor.copyRowRadius
        color: rowHover.hovered ? Theme.gameEditor.copyHover : Qt.alpha(Theme.gameEditor.copyHover, 0)
        Behavior on color { enabled: Motion.fades; MotionColor {} }

        HoverHandler { id: rowHover }

        Rectangle {
            visible: row.current
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.layout.spacingSmall
            anchors.topMargin: Theme.layout.spacing
            anchors.bottomMargin: Theme.layout.spacing
            width: Theme.gameEditor.copyCurrentMarkerWidth
            radius: width / 2
            color: Theme.gameEditor.copyCurrentMarker
        }

        RowLayout {
            id: rowContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 2 * Theme.layout.spacing
            anchors.rightMargin: Theme.layout.spacing
            spacing: Theme.controls.controlSpacing

            Rectangle {
                implicitHeight: Theme.gameEditor.copyCoverHeight
                implicitWidth: implicitHeight * 3 / 4
                radius: Theme.gameEditor.copyCoverRadius
                color: Theme.gameEditor.artPreviewBackground
                clip: true
                opacity: row.contentOpacity

                FadeImage {
                    anchors.fill: parent
                    source: row.coverUrl
                    sourceSize.height: parent.height * 2
                    fillMode: Image.PreserveAspectCrop
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacingSmall
                opacity: row.contentOpacity

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.layout.spacing

                    Label {
                        Layout.minimumWidth: 0
                        text: row.title
                        color: Theme.controls.inputText
                        elide: Text.ElideRight
                    }
                    SettingTag {
                        opacity: row.main ? 1 : 0
                        visible: opacity > 0
                        filled: false
                        text: qsTr("Main")
                        tint: Theme.gameEditor.copyMainTag
                        Behavior on opacity {
                            enabled: Motion.fades
                            MotionNumber { fade: true }
                        }
                    }
                    Item { Layout.fillWidth: true }
                }
                Label {
                    Layout.fillWidth: true
                    text: row.meta
                    color: Theme.controls.descriptionColor
                    font.pixelSize: Theme.controls.descriptionFontSize
                    elide: Text.ElideRight
                }
            }

            RowLayout {
                id: trailingRow
                Layout.alignment: Qt.AlignVCenter
                spacing: Theme.layout.spacingSmall
            }
        }
    }

    Layout.fillWidth: true
    Layout.fillHeight: false
    Layout.alignment: Qt.AlignTop
    spacing: Theme.gameEditor.cardSpacing

    KeyedListModel {
        id: copyModel
        leaveSpeed: "fast"
        scope: page.editor.game.id
        source: page.rows.map(c => ({
            key: c.id, id: c.id, title: c.title, platformName: c.platformName, coverUrl: c.coverUrl,
            hidden: c.hidden, notInstalled: c.notInstalled, playTimeText: c.playTimeText,
            lastPlayedText: c.lastPlayedText, pendingNew: page.editor.copyAdded.some(a => a.id === c.id)
        }))
    }

    EditorCard {
        title: qsTr("Linked copies")

        Label {
            visible: !page.linked
            Layout.fillWidth: true
            text: qsTr("Copies of one game from different sources count as one: play time, history and media together.")
            color: Theme.gameEditor.hintColor
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Repeater {
                model: copyModel

                CopyRow {
                    id: copyRow
                    required property var model
                    readonly property bool here: model.id === page.editor.game.id
                    readonly property bool dimmed: page.editor.copyIsDimmed(model.id)
                    readonly property bool unlinkPending: page.editor.copyUnlinked[model.id] === true
                    // Linking a row that isn't in a group yet has nothing to take back; its own row only goes with the others.
                    readonly property bool canUnlink: model.pendingNew || (page.editor.copyBase.length > 0 && (!dimmed || unlinkPending))
                    readonly property bool unlinksAll: main && !model.pendingNew && page.editor.copyBase.length > 0
                    readonly property bool canMakeMain: !main && !dimmed
                    property bool entered: !page.armed || copyModel.rebuilt

                    coverUrl: model.coverUrl
                    title: model.title
                    main: model.id === page.editor.copyMainShown
                    current: here
                    contentOpacity: dimmed ? Theme.gameEditor.copyDimmedOpacity : 1
                    meta: [model.platformName,
                           here ? qsTr("This copy") : "",
                           model.hidden ? qsTr("Hidden") : "",
                           model.notInstalled ? qsTr("Not installed") : "",
                           model.pendingNew ? qsTr("New") : "",
                           unlinkPending ? qsTr("Unlinks when you save") : ""].filter(s => s !== "").join(" · ")
                    opacity: model.leaving || !entered ? 0 : 1

                    Behavior on opacity {
                        enabled: Motion.fades
                        MotionNumber { fade: true }
                    }
                    Behavior on contentOpacity {
                        enabled: Motion.fades
                        MotionNumber { fade: true }
                    }
                    Component.onCompleted: entered = true

                    ColumnLayout {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.rightMargin: Theme.layout.spacing
                        spacing: Theme.layout.spacingSmall
                        opacity: copyRow.contentOpacity

                        Label {
                            Layout.alignment: Qt.AlignRight
                            visible: copyRow.model.playTimeText !== ""
                            text: copyRow.model.playTimeText
                            color: Theme.controls.inputText
                        }
                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: copyRow.model.playTimeText !== "" ? copyRow.model.lastPlayedText : qsTr("Not played")
                            color: Theme.controls.descriptionColor
                            font.pixelSize: Theme.controls.descriptionFontSize
                        }
                    }

                    ListButton {
                        flat: true
                        iconPath: Icons.openExternal
                        enabled: !copyRow.here && !copyRow.model.pendingNew && !copyRow.dimmed && !copyRow.model.leaving
                        textColor: hovered ? foreground : Theme.controls.descriptionColor
                        tip: qsTr("Open this copy's Properties")
                        onClicked: page.editor.openCopy(copyRow.model.id)
                    }

                    ListButton {
                        id: moreButton
                        flat: true
                        iconPath: Icons.more
                        enabled: (copyRow.canMakeMain || copyRow.canUnlink) && !copyRow.model.leaving
                        textColor: hovered || moreMenu.opened ? foreground : Theme.controls.descriptionColor
                        tip: qsTr("Make main or unlink")
                        onClicked: moreMenu.opened ? moreMenu.close() : moreMenu.open()

                        SettingsMenu {
                            id: moreMenu
                            x: moreButton.width - width
                            y: moreButton.height
                            pops: true
                            popDirection: "below"
                            // The button toggles it; a press on it must not close and reopen it.
                            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                            SettingsMenuItem {
                                want: copyRow.canMakeMain
                                text: qsTr("Make main")
                                onTriggered: page.editor.setCopyMain(copyRow.model.id)
                            }
                            SettingsMenuItem {
                                want: copyRow.canUnlink
                                danger: copyRow.unlinksAll && !copyRow.unlinkPending
                                text: copyRow.unlinkPending ? qsTr("Keep it linked")
                                    : copyRow.model.pendingNew ? qsTr("Don't link this copy")
                                    : copyRow.unlinksAll ? qsTr("Unlink all")
                                    : qsTr("Unlink this copy")
                                onTriggered: page.editor.toggleCopyUnlink(copyRow.model.id)
                            }
                        }
                    }
                }
            }

            Repeater {
                model: page.candidates

                CopyRow {
                    id: candidate
                    required property var modelData

                    coverUrl: modelData.coverUrl
                    title: modelData.title
                    meta: modelData.linkedTo !== ""
                          ? qsTr("%1 · linked to %2").arg(modelData.platformName).arg(modelData.linkedTo)
                          : modelData.sameIgdb ? qsTr("%1 · same IGDB entry").arg(modelData.platformName)
                                               : qsTr("%1 · same title").arg(modelData.platformName)

                    ListButton {
                        enabled: candidate.modelData.linkedTo === ""
                        text: qsTr("Link")
                        tip: enabled ? qsTr("Count this as the same game") : qsTr("Unlink it from %1 first").arg(candidate.modelData.linkedTo)
                        onClicked: page.editor.addCopy(candidate.modelData.id)
                    }
                }
            }

            ListAction {
                Layout.leftMargin: 2 * Theme.layout.spacing
                Layout.topMargin: Theme.layout.spacing
                text: qsTr("Link another copy…")
                tip: qsTr("Link a copy that has a different title")
                onClicked: page.editor.chooseCopy()
            }
        }
    }

    EditorHint {
        open: page.linked
        text: page.together()
    }
}
