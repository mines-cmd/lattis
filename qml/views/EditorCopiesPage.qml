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
        const time = editor.copyInfo.totalPlayTimeText ?? ""
        const last = editor.copyInfo.totalLastPlayedText ?? ""
        if (time !== "" && last !== "")
            return qsTr("Together: %1, last played %2 · media shared").arg(time).arg(last)
        if (time !== "")
            return qsTr("Together: %1 · media shared").arg(time)
        return qsTr("Together: not played yet · media shared")
    }

    component Badge: Rectangle {
        property alias text: badgeLabel.text
        property bool main: false

        implicitWidth: badgeLabel.implicitWidth + 2 * Theme.layout.spacing
        implicitHeight: badgeLabel.implicitHeight + Theme.layout.spacingSmall
        radius: Theme.gameEditor.copyBadgeRadius
        color: main ? Theme.gameEditor.copyMainBackground : Theme.gameEditor.copyBadgeBackground

        Label {
            id: badgeLabel
            anchors.centerIn: parent
            color: parent.main ? Theme.gameEditor.copyMainText : Theme.gameEditor.copyBadgeText
            font.pixelSize: Theme.gameEditor.hintFontSize
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

        ListButton {
            id: openSizer
            visible: false
            text: qsTr("Open")
        }
        ListButton {
            id: makeMainSizer
            visible: false
            text: qsTr("Make main")
        }
        ListButton {
            id: unlinkAllSizer
            visible: false
            text: qsTr("Unlink all")
        }

        Repeater {
            model: copyModel

            RowLayout {
                id: copyRow
                required property var model
                readonly property bool here: model.id === page.editor.game.id
                readonly property bool main: model.id === page.editor.copyMainShown
                readonly property bool dimmed: page.editor.copyIsDimmed(model.id)
                readonly property bool unlinkPending: page.editor.copyUnlinked[model.id] === true
                // Linking a row that isn't in a group yet has nothing to take back; its own row only goes with the others.
                readonly property bool canUnlink: model.pendingNew || (page.editor.copyBase.length > 0 && (!dimmed || unlinkPending))
                readonly property bool unlinksAll: main && !model.pendingNew && page.editor.copyBase.length > 0
                readonly property bool canMakeMain: !main && !dimmed
                property bool entered: !page.armed || copyModel.rebuilt

                Layout.fillWidth: true
                spacing: Theme.controls.controlSpacing
                opacity: copyRow.model.leaving || !entered ? 0 : 1

                Behavior on opacity {
                    enabled: Motion.fades
                    MotionNumber { fade: true }
                }
                Component.onCompleted: entered = true

                HoverHandler { id: rowHover }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.controls.controlSpacing
                    opacity: copyRow.dimmed ? Theme.gameEditor.copyDimmedOpacity : 1

                    Behavior on opacity {
                        enabled: Motion.fades
                        MotionNumber { fade: true }
                    }

                    Rectangle {
                        implicitHeight: Theme.gameEditor.copyCoverHeight
                        implicitWidth: implicitHeight * 3 / 4
                        radius: Theme.controls.inputRadius
                        color: Theme.gameEditor.artPreviewBackground
                        clip: true

                        FadeImage {
                            anchors.fill: parent
                            source: copyRow.model.coverUrl
                            sourceSize.height: parent.height * 2
                            fillMode: Image.PreserveAspectCrop
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.layout.spacingSmall

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.layout.spacing

                            Label {
                                Layout.minimumWidth: 0
                                text: copyRow.model.title
                                color: Theme.gameEditor.headingColor
                                elide: Text.ElideRight
                            }
                            Badge {
                                id: mainBadge
                                main: true
                                text: qsTr("Main")
                                opacity: copyRow.main ? 1 : 0
                                visible: opacity > 0
                                Behavior on opacity {
                                    enabled: Motion.fades
                                    MotionNumber { fade: true }
                                }
                            }
                            Badge {
                                visible: copyRow.model.pendingNew
                                text: qsTr("New")
                            }
                            Badge {
                                visible: copyRow.model.hidden
                                text: qsTr("Hidden")
                            }
                            Badge {
                                visible: copyRow.model.notInstalled
                                text: qsTr("Not installed")
                            }
                            Item { Layout.fillWidth: true }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.layout.spacing

                            Label {
                                text: copyRow.model.platformName
                                color: Theme.gameEditor.hintColor
                                font.pixelSize: Theme.gameEditor.hintFontSize
                            }
                            Badge {
                                visible: copyRow.here
                                text: qsTr("This copy")
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }

                    ColumnLayout {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: Theme.gameEditor.copyNumbersWidth
                        spacing: Theme.layout.spacingSmall

                        Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                            text: copyRow.model.playTimeText !== "" ? copyRow.model.playTimeText : qsTr("Not played")
                            color: Theme.gameEditor.labelColor
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                            text: copyRow.model.lastPlayedText
                            color: Theme.gameEditor.hintColor
                            font.pixelSize: Theme.gameEditor.hintFontSize
                            elide: Text.ElideRight
                        }
                    }
                }

                Item {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: openSizer.implicitWidth
                    Layout.preferredHeight: openSizer.implicitHeight

                    ListButton {
                        anchors.centerIn: parent
                        visible: !copyRow.here && !copyRow.model.pendingNew
                        enabled: !copyRow.dimmed && !copyRow.model.leaving
                        text: qsTr("Open")
                        tip: qsTr("Open this copy's Properties")
                        onClicked: page.editor.openCopy(copyRow.model.id)
                    }
                }

                Item {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: makeMainSizer.implicitWidth
                    Layout.preferredHeight: makeMainSizer.implicitHeight

                    ListButton {
                        id: makeMain
                        anchors.centerIn: parent
                        text: qsTr("Make main")
                        tip: qsTr("Count this copy as the main one")
                        enabled: opacity > 0 && copyRow.canMakeMain && !copyRow.model.leaving
                        opacity: copyRow.canMakeMain && (rowHover.hovered || activeFocus) ? 1 : 0
                        visible: opacity > 0
                        Behavior on opacity {
                            enabled: Motion.fades
                            MotionNumber { fade: true }
                        }
                        onClicked: page.editor.setCopyMain(copyRow.model.id)
                    }
                }

                Item {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: unlinkAllSizer.implicitWidth
                    Layout.preferredHeight: unlinkAllSizer.implicitHeight

                    ListButton {
                        anchors.centerIn: parent
                        visible: copyRow.canUnlink
                        enabled: !copyRow.model.leaving
                        text: copyRow.unlinkPending || copyRow.model.pendingNew || !copyRow.unlinksAll ? "" : qsTr("Unlink all")
                        iconPath: copyRow.unlinkPending ? Icons.undo : copyRow.unlinksAll ? "" : Icons.close
                        tip: copyRow.unlinkPending ? qsTr("Keep it linked")
                           : copyRow.model.pendingNew ? qsTr("Don't link this copy")
                           : copyRow.unlinksAll ? qsTr("Unlink every copy from this game")
                           : qsTr("Unlink this copy")
                        onClicked: page.editor.toggleCopyUnlink(copyRow.model.id)
                    }
                }
            }
        }

        Rectangle {
            visible: page.linked
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.settingsWindow.rowDivider
        }
        Label {
            visible: page.linked
            Layout.fillWidth: true
            text: page.editor.copiesDirty ? qsTr("The combined numbers update when you save.") : page.together()
            color: page.editor.copiesDirty ? Theme.gameEditor.hintColor : Theme.gameEditor.labelColor
            wrapMode: Text.WordWrap
        }
    }

    EditorCard {
        title: qsTr("Link another copy")

        Repeater {
            model: page.candidates

            RowLayout {
                id: candidate
                required property var modelData
                Layout.fillWidth: true
                spacing: Theme.controls.controlSpacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.layout.spacingSmall

                    Label {
                        Layout.fillWidth: true
                        text: candidate.modelData.title
                        color: Theme.gameEditor.headingColor
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: candidate.modelData.linkedTo !== ""
                              ? qsTr("%1 · linked to %2").arg(candidate.modelData.platformName).arg(candidate.modelData.linkedTo)
                              : candidate.modelData.sameIgdb
                                ? qsTr("%1 · same IGDB entry").arg(candidate.modelData.platformName)
                                : qsTr("%1 · same title").arg(candidate.modelData.platformName)
                        color: Theme.gameEditor.hintColor
                        font.pixelSize: Theme.gameEditor.hintFontSize
                        elide: Text.ElideRight
                    }
                }
                ListButton {
                    Layout.alignment: Qt.AlignVCenter
                    enabled: candidate.modelData.linkedTo === ""
                    text: qsTr("Link")
                    tip: enabled ? qsTr("Count this as the same game") : qsTr("Unlink it from %1 first").arg(candidate.modelData.linkedTo)
                    onClicked: page.editor.addCopy(candidate.modelData.id)
                }
            }
        }

        Label {
            visible: page.candidates.length === 0
            Layout.fillWidth: true
            text: qsTr("No other game has this title.")
            color: Theme.gameEditor.hintColor
            wrapMode: Text.WordWrap
        }

        ListButton {
            text: qsTr("Choose games…")
            tip: qsTr("Link a copy that has a different title")
            onClicked: page.editor.chooseCopy()
        }
    }
}
