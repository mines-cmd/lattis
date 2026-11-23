import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
AppDialog {
    id: root

    // [{id, name, from, version, size, notes, icon?}]: one is shown as plain notes, several as a list of rows. `icon`
    // is an image URL; a row without one shows the vector path `rowIcon(row)` gives.
    property var rows: []
    property string openId: ""
    // {status, progress, step, error} of a row: `status` is pending, waiting, active, failed or done.
    property var stateOf: id => ({ status: "pending", progress: -1, step: "", error: "" })
    // Replaces the versions and size under the title when there is one row.
    property string subtitle: ""
    property bool showNotes: true

    readonly property bool single: rows.length === 1
    property var rowIcon: row => ""

    signal updateRequested(string id)

    carded: true
    size: "medium"
    width: Math.min(Theme.gameEditor.updateWidth, parent.width - 2 * Theme.gameEditor.padding)
    height: Math.min(implicitHeight, Math.round(parent.height * Theme.gameEditor.whatsNewHeightPercent / 100))

    function megabytes(bytes) {
        return qsTr("%1 MB").arg(Math.max(1, Math.round(bytes / 1e6)))
    }

    function sizeVersions(row) {
        const versions = row.from !== "" ? qsTr("%1 → %2").arg(row.from).arg(row.version) : row.version
        return row.size > 0 ? qsTr("%1 · %2").arg(versions).arg(megabytes(row.size)) : versions
    }

    Label {
        visible: root.single
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.labelColor
        text: !root.single ? ""
            : root.subtitle !== "" ? root.subtitle
            : root.stateOf(root.rows[0].id).status === "done" ? qsTr("Updated to %1").arg(root.rows[0].version)
            : root.sizeVersions(root.rows[0])
    }

    InstallProgress {
        visible: root.single && root.stateOf(root.rows[0].id).status === "active"
        Layout.fillWidth: true
        progress: root.single ? root.stateOf(root.rows[0].id).progress : -1
    }

    Flickable {
        id: view
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: card.implicitHeight
        contentWidth: width
        contentHeight: card.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: SettingsScrollBar { id: bar }

        // Set from the handlers, not bound: the notes wrap, so their height depends on the width the bar takes.
        // It settles, as a narrower card is only taller.
        property bool scrolls: false
        function recheck() { scrolls = card.implicitHeight > height + 1 }
        onHeightChanged: recheck()

        Connections {
            target: card
            function onImplicitHeightChanged() { view.recheck() }
        }

        // The Platforms page's list: one card, its rows told apart by lines, the open one on a darker panel.
        Rectangle {
            id: card
            width: view.width - (view.scrolls ? bar.reserve : 0)
            implicitHeight: cardColumn.implicitHeight + Theme.settingsWindow.cardPadding * 2
            radius: Theme.settingsWindow.cardRadius
            color: Theme.settingsWindow.cardBackground
            border.color: Theme.settingsWindow.cardBorder
            border.width: Theme.settingsWindow.cardBorderWidth

            ColumnLayout {
                id: cardColumn
                anchors.fill: parent
                anchors.margins: Theme.settingsWindow.cardPadding
                spacing: 0

                Repeater {
                    model: root.rows

                    ColumnLayout {
                        id: row

                        required property var modelData
                        required property int index

                        readonly property var current: root.stateOf(modelData.id)
                        readonly property bool expanded: root.single || root.openId === modelData.id
                        readonly property bool firstInCard: index === 0
                        readonly property bool lastInCard: index === root.rows.length - 1
                        readonly property real cardCornerRadius: Math.max(Theme.settingsWindow.rowRadius,
                                                                          Theme.settingsWindow.cardRadius - Theme.settingsWindow.cardPadding)

                        Layout.fillWidth: true
                        spacing: 0

                        Rectangle {
                            id: header
                            visible: !root.single
                            Layout.fillWidth: true
                            implicitHeight: headerRow.implicitHeight + Theme.settingsWindow.rowPadding * 2
                            radius: Theme.settingsWindow.rowRadius
                            topLeftRadius: row.firstInCard ? row.cardCornerRadius : radius
                            topRightRadius: topLeftRadius
                            bottomLeftRadius: details.progress > 0 ? 0 : row.lastInCard ? row.cardCornerRadius : radius
                            bottomRightRadius: bottomLeftRadius
                            readonly property bool lit: headerHover.hovered || header.activeFocus
                            color: lit && details.progress === 0 ? Theme.controls.hover : "transparent"
                            activeFocusOnTab: true

                            function toggle() { root.openId = row.expanded ? "" : row.modelData.id }

                            Keys.onReturnPressed: toggle()
                            Keys.onEnterPressed: toggle()
                            Keys.onSpacePressed: toggle()

                            HoverHandler { id: headerHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                onTapped: { header.forceActiveFocus(); header.toggle() }
                            }

                            OpenRowCard {
                                row: row
                                progress: details.progress
                                firstInCard: row.firstInCard
                                lastInCard: row.lastInCard
                                lit: header.lit
                            }

                            RowLayout {
                                id: headerRow
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: Theme.settingsWindow.rowPadding
                                spacing: Theme.layout.spacing

                                Icon {
                                    path: Icons.chevronRight
                                    size: Theme.controls.iconSize
                                    color: Theme.controls.iconColor
                                    rotation: row.expanded ? 90 : 0
                                    Behavior on rotation { enabled: Motion.on; MotionNumber {} }
                                }
                                Item {
                                    visible: (row.modelData.icon ?? "") !== "" || root.rowIcon(row.modelData) !== ""
                                    implicitWidth: Theme.controls.iconSize * 1.5
                                    implicitHeight: implicitWidth

                                    Image {
                                        id: rowImage
                                        anchors.fill: parent
                                        visible: status === Image.Ready
                                        source: row.modelData.icon ?? ""
                                        sourceSize.width: width
                                        sourceSize.height: height
                                        fillMode: Image.PreserveAspectFit
                                        asynchronous: true
                                    }
                                    Icon {
                                        anchors.centerIn: parent
                                        visible: !rowImage.visible
                                        path: root.rowIcon(row.modelData)
                                        size: Theme.controls.iconSize
                                        color: Theme.controls.iconColor
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    PlainLabel {
                                        Layout.fillWidth: true
                                        text: row.modelData.name
                                        color: Theme.controls.labelColor
                                        font.pixelSize: Theme.controls.labelFontSize
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: root.sizeVersions(row.modelData)
                                        color: Theme.controls.descriptionColor
                                        font.pixelSize: Theme.controls.descriptionFontSize
                                        elide: Text.ElideRight
                                    }
                                }
                                Item {
                                    implicitWidth: Math.max(update.implicitWidth, waiting.implicitWidth, progress.implicitWidth,
                                                            done.implicitWidth, retry.implicitWidth)
                                    implicitHeight: Math.max(update.implicitHeight, retry.implicitHeight)

                                    SettingsButton {
                                        id: update
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: qsTr("Update")
                                        opacity: row.current.status === "pending" ? 1 : 0
                                        visible: opacity > 0
                                        Behavior on opacity { enabled: Motion.on; MotionNumber {} }
                                        onClicked: root.updateRequested(row.modelData.id)
                                    }
                                    Label {
                                        id: waiting
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: Theme.controls.descriptionColor
                                        font.pixelSize: Theme.controls.descriptionFontSize
                                        text: qsTr("Waiting")
                                        opacity: row.current.status === "waiting" ? 1 : 0
                                        visible: opacity > 0
                                        Behavior on opacity { enabled: Motion.on; MotionNumber {} }
                                    }
                                    ColumnLayout {
                                        id: progress
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 12 * Theme.layout.spacing
                                        spacing: Theme.layout.spacingSmall
                                        opacity: row.current.status === "active" ? 1 : 0
                                        visible: opacity > 0
                                        Behavior on opacity { enabled: Motion.on; MotionNumber {} }

                                        Label {
                                            Layout.alignment: Qt.AlignRight
                                            color: Theme.controls.descriptionColor
                                            font.pixelSize: Theme.controls.descriptionFontSize
                                            text: row.current.step
                                        }
                                        InstallProgress {
                                            Layout.fillWidth: true
                                            progress: row.current.progress
                                        }
                                    }
                                    RowLayout {
                                        id: done
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: Theme.layout.spacingSmall
                                        opacity: row.current.status === "done" ? 1 : 0
                                        visible: opacity > 0
                                        Behavior on opacity { enabled: Motion.on; MotionNumber {} }

                                        Icon {
                                            path: Icons.check
                                            size: Theme.controls.iconSize
                                            color: Theme.settingsWindow.installProgressFill
                                        }
                                        Label {
                                            color: Theme.controls.descriptionColor
                                            font.pixelSize: Theme.controls.descriptionFontSize
                                            text: qsTr("Updated")
                                        }
                                    }
                                    SettingsButton {
                                        id: retry
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: qsTr("Try again")
                                        opacity: row.current.status === "failed" ? 1 : 0
                                        visible: opacity > 0
                                        Behavior on opacity { enabled: Motion.on; MotionNumber {} }
                                        onClicked: root.updateRequested(row.modelData.id)
                                    }
                                }
                            }
                        }

                        PlainLabel {
                            visible: row.current.status === "failed" && row.current.error !== ""
                            Layout.fillWidth: true
                            Layout.leftMargin: root.single ? 0 : Theme.settingsWindow.rowPadding * 2 + Theme.controls.iconSize
                            Layout.rightMargin: Theme.settingsWindow.rowPadding
                            Layout.bottomMargin: Theme.settingsWindow.rowPadding
                            wrapMode: Text.WordWrap
                            color: Theme.controls.problemsText
                            font.pixelSize: Theme.controls.descriptionFontSize
                            text: row.current.error
                        }

                        Unfold {
                            id: details
                            open: row.expanded && root.showNotes
                            Layout.fillWidth: true
                            leftPadding: Theme.settingsWindow.rowPadding
                            rightPadding: Theme.settingsWindow.rowPadding
                            topPadding: Theme.settingsWindow.rowPadding * 1.5
                            bottomPadding: Theme.settingsWindow.rowPadding * 1.5
                            lazyContent: Component {
                                Label {
                                    wrapMode: Text.WordWrap
                                    textFormat: Text.RichText
                                    color: Theme.gameEditor.updateNotesText
                                    text: AppUpdate.notesHtml(row.modelData.notes !== "" ? row.modelData.notes : qsTr("No release notes."),
                                                              Qt.alpha(Theme.gameEditor.updateNotesText, 0.25))
                                    onLinkActivated: link => GameRunner.openUrl(link)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
