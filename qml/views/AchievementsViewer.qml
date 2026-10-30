import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects

// See docs/code/qml.md.
MotionPopup {
    id: root

    property var game: ({})
    property var items: []
    property string query: ""
    property bool hiddenShown: false
    property int filter: 0
    property var recent: ({})
    readonly property int unlocked: items.filter(a => a.unlocked).length
    readonly property string needle: query.trim().toLowerCase()
    readonly property int concealedCount: items.filter(concealed).length
    readonly property var matches: items
        .filter(a => !concealed(a) && wanted(a.unlocked)
                     && (needle === "" || a.name.toLowerCase().includes(needle) || a.description.toLowerCase().includes(needle)))
        .map(a => Object.assign({ group: a.unlocked ? 0 : a.hidden ? 2 : 1 }, a))
        .concat(needle === "" && concealedCount > 0 && wanted(false) ? [{ group: 2, summary: true, count: concealedCount }] : [])
        .sort((a, b) => a.group - b.group)
    readonly property var groupNames: [qsTr("Unlocked"), qsTr("Locked"), qsTr("Hidden")]

    function show(g, list) {
        game = g
        items = list
        query = ""
        filter = 0
        hiddenShown = false
        const history = Library.pageHistory(g.id ?? "")
        recent = history.recent ?? {}
        open()
    }

    function playTime(minutes): string {
        return Library.playTime(minutes ?? 0, General.playTimeFormat)
    }

    Connections {
        target: Library
        function onPageHistoryReady(id, result) {
            if (id === (root.game.id ?? ""))
                root.recent = result.recent ?? {}
        }
    }

    function wanted(isUnlocked): bool {
        return filter === 0 || (filter === 1) === isUnlocked
    }

    function concealed(a): bool {
        return a.hidden && !a.unlocked && !hiddenShown
    }

    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent.width
    height: parent.height
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape
    Overlay.modal: Rectangle {
        color: Theme.achievements.viewerScrim

        Behavior on opacity {
            id: fade
            enabled: Motion.fades
            MotionNumber { fade: true; speed: fade.targetValue > 0 ? "normal" : "fast"; linear: true }
        }
    }
    background: null

    contentItem: Item {
        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
        }

        Surface {
            id: card
            anchors.horizontalCenter: parent.horizontalCenter
            y: Theme.achievements.viewerMargin
            width: Math.min(parent.width - 2 * Theme.achievements.viewerMargin, Theme.achievements.viewerWidth)
            height: parent.height - 2 * Theme.achievements.viewerMargin
            radius: Theme.achievements.viewerRadius
            fill: Theme.achievements.viewerBackground
            border.color: Theme.achievements.viewerBorder
            border.width: Theme.achievements.viewerBorderWidth

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
            }

            Item {
                id: backdropClip
                anchors.fill: parent
                visible: Theme.backdrop.enabled
                layer.enabled: visible
                layer.effect: MultiEffect {
                    maskEnabled: true
                    maskSource: backdropMask
                }

                Backdrop {
                    width: parent.width
                    height: list.y + Theme.achievements.viewerPadding + list.height * 0.3
                    flat: true
                    background: Theme.achievements.viewerBackground
                    game: root.game
                }
            }

            Rectangle {
                id: backdropMask
                anchors.fill: parent
                radius: card.radius
                visible: false
                layer.enabled: true
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.achievements.viewerPadding
                spacing: Theme.layout.spacing * 3

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.layout.spacing * 3

                    ArtThumb {
                        visible: Theme.achievements.viewerCoverHeight > 0
                        Layout.preferredHeight: Theme.achievements.viewerCoverHeight
                        Layout.preferredWidth: implicitWidth
                        implicitHeight: Theme.achievements.viewerCoverHeight
                        cornerRadius: Theme.achievements.viewerCoverRadius
                        color: Theme.achievements.viewerIconPlaceholder
                        iconWhenEmpty: true
                        source: root.game.coverUrl ?? ""
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignBottom
                        spacing: Theme.layout.spacing * 2

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.rightMargin: Math.max(0, closeButton.width + Theme.achievements.viewerCloseGap - Theme.achievements.viewerPadding)
                            spacing: Theme.layout.spacing

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.layout.spacing * 2

                                Label {
                                    Layout.maximumWidth: parent.width - (year.visible ? year.implicitWidth + parent.spacing : 0)
                                    Layout.alignment: Qt.AlignBaseline
                                    text: root.game.title ?? ""
                                    elide: Text.ElideRight
                                    color: Theme.achievements.viewerTitleColor
                                    font.pixelSize: Theme.achievements.viewerTitleSize
                                    font.weight: Theme.achievements.viewerTitleWeight
                                }
                                Label {
                                    id: year
                                    Layout.alignment: Qt.AlignBaseline
                                    visible: text !== ""
                                    text: root.game.year ?? ""
                                    color: Theme.achievements.viewerYearColor
                                    font.pixelSize: Theme.achievements.viewerYearSize
                                    font.weight: Theme.achievements.viewerYearWeight
                                }
                                Item { Layout.fillWidth: true }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: [qsTr("Play time: %1").arg(root.playTime(root.game.playMinutes) || qsTr("None")),
                                       qsTr("This week: %1").arg(root.playTime(root.recent.week) || qsTr("None")),
                                       qsTr("This month: %1").arg(root.playTime(root.recent.month) || qsTr("None"))]
                                      .slice(0, root.recent.week === undefined ? 1 : 3).join("  ·  ")
                                elide: Text.ElideRight
                                color: Theme.achievements.viewerPlayTimeColor
                            }
                        }

                        Surface {
                            Layout.fillWidth: true
                            implicitHeight: summary.implicitHeight + 2 * Theme.achievements.viewerSummaryPadding
                            radius: Theme.achievements.viewerSummaryRadius
                            fill: Theme.achievements.viewerSummaryBackground

                            ColumnLayout {
                                id: summary
                                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter
                                          margins: Theme.achievements.viewerSummaryPadding }
                                spacing: Theme.layout.spacing * 2

                                Label {
                                    readonly property bool complete: root.items.length > 0 && root.unlocked === root.items.length
                                    textFormat: Text.StyledText
                                    text: qsTr("%1 / %2 achievements unlocked %3").arg(root.unlocked).arg(root.items.length)
                                              .arg("<font color=\"" + Theme.achievements.viewerPercentColor + "\">"
                                                   + qsTr("(%1%)").arg(root.items.length > 0 ? Math.round(100 * root.unlocked / root.items.length) : 0)
                                                   + "</font>")
                                    color: complete ? Theme.achievements.viewerCompleteColor : Theme.achievements.viewerProgressColor
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: Theme.achievements.viewerBarHeight
                                    radius: height / 2
                                    color: Theme.achievements.viewerBarColor

                                    Rectangle {
                                        width: root.items.length > 0 ? parent.width * root.unlocked / root.items.length : 0
                                        height: parent.height
                                        radius: parent.radius
                                        color: Theme.achievements.viewerBarFill
                                    }
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.layout.spacing

                    SettingsSearchField {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.achievements.viewerFilterHeight
                        fieldBackground: Theme.achievements.viewerFilterBackground
                        textColor: Theme.achievements.viewerFilterText
                        placeholderColor: Theme.achievements.viewerFilterPlaceholder
                        iconColor: Theme.achievements.viewerFilterIcon
                        placeholderText: qsTr("Search achievements")
                        text: root.query
                        onTextChanged: root.query = text
                    }
                    SettingsComboBox {
                        fieldBackground: Theme.achievements.viewerFilterBackground
                        hoverBackground: Theme.achievements.viewerFilterHoverBackground
                        textColor: Theme.achievements.viewerFilterText
                        iconColor: Theme.achievements.viewerFilterIcon
                        Layout.preferredWidth: Theme.achievements.viewerFilterWidth
                        Layout.preferredHeight: Theme.achievements.viewerFilterHeight
                        model: [qsTr("All achievements"), qsTr("Unlocked achievements"), qsTr("Locked achievements")]
                        currentIndex: root.filter
                        onActivated: index => root.filter = index
                    }
                }

                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.matches
                    ScrollBar.vertical: ScrollBar {}

                    section.property: "group"
                    section.delegate: Label {
                        required property string section
                        width: ListView.view.width
                        height: implicitHeight + Theme.layout.spacing
                        verticalAlignment: Text.AlignBottom
                        text: root.groupNames[Number(section)].toUpperCase()
                              + " (" + root.matches.filter(a => a.group === Number(section))
                                                   .reduce((n, a) => n + (a.summary ? a.count : 1), 0) + ")"
                        color: Theme.achievements.sectionColor
                        font.weight: Theme.achievements.viewerSectionWeight
                    }

                    delegate: Item {
                        id: row
                        required property var modelData
                        required property int index
                        readonly property bool summary: modelData.summary === true
                        width: ListView.view.width
                        height: Theme.achievements.viewerIconSize + 2 * Theme.layout.spacing * 2

                        TapHandler {
                            enabled: row.summary
                            onTapped: root.hiddenShown = true
                        }
                        HoverHandler {
                            id: rowHover
                            cursorShape: row.summary ? Qt.PointingHandCursor : Qt.ArrowCursor
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.achievements.viewerRowRadius
                            color: Theme.achievements.viewerRowHover
                            opacity: rowHover.hovered ? 1 : 0
                            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true; speed: "fast" } }
                        }

                        Rectangle {
                            visible: row.index > 0 && root.matches[row.index - 1].group === row.modelData.group
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            height: 1
                            color: Theme.achievements.viewerDivider
                        }

                        ArtThumb {
                            id: icon
                            anchors { left: parent.left; verticalCenter: parent.verticalCenter
                                      leftMargin: Theme.layout.spacing * 2 }
                            width: Theme.achievements.viewerIconSize
                            height: Theme.achievements.viewerIconSize
                            aspect: 1
                            cornerRadius: Theme.achievements.viewerIconRadius
                            color: Theme.achievements.viewerIconPlaceholder
                            failedIcon: Icons.medal
                            iconWhenEmpty: !row.summary
                            property bool useFallback: false
                            source: row.summary ? "" : (useFallback ? row.modelData.iconFallback : row.modelData.icon) ?? ""
                            onStatusChanged: {
                                if (status === Image.Error && !useFallback)
                                    useFallback = true
                            }
                            opacity: row.modelData.unlocked ? 1 : Theme.achievements.viewerLockedOpacity

                            Label {
                                visible: row.summary
                                anchors.centerIn: parent
                                text: "?"
                                color: Theme.achievements.viewerHiddenMarkColor
                                font.pixelSize: parent.height * 0.5
                            }
                        }
                        ColumnLayout {
                            anchors { left: icon.right; right: stamp.left; verticalCenter: parent.verticalCenter
                                      leftMargin: Theme.layout.spacing * 3; rightMargin: Theme.layout.spacing * 3 }
                            spacing: 0

                            Label {
                                Layout.fillWidth: true
                                text: row.summary ? qsTr("%n hidden achievement(s)", "", row.modelData.count) : row.modelData.name
                                elide: Text.ElideRight
                                color: Theme.achievements.viewerNameColor
                                font.weight: Theme.achievements.viewerNameWeight
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: text !== ""
                                text: row.summary ? qsTr("Details for each will be revealed once you've unlocked it. Click to show them anyway.")
                                                  : row.modelData.description
                                elide: Text.ElideRight
                                color: Theme.achievements.viewerDescriptionColor
                            }
                        }
                        Label {
                            id: stamp
                            anchors { right: parent.right; verticalCenter: parent.verticalCenter
                                      rightMargin: Theme.layout.spacing * 2 }
                            text: row.modelData.unlocked
                                  ? qsTr("Unlocked %1").arg(row.modelData.time.toLocaleString(Qt.locale(), General.shortDateFormat + ", HH:mm"))
                                  : ""
                            color: Theme.achievements.viewerDateColor
                        }
                    }
                }
            }

            IconButton {
                id: closeButton
                anchors { top: parent.top; right: parent.right; margins: Theme.achievements.viewerCloseGap }
                plain: true
                implicitWidth: Theme.media.viewerButtonSize
                implicitHeight: Theme.media.viewerButtonSize
                radius: height / 2
                icon: Icons.close
                iconSize: Theme.media.viewerIconSize
                iconColor: Theme.media.viewerCloseColor
                hoverTint: Theme.media.viewerButtonHover
                tip: qsTr("Close (Esc)")
                onClicked: root.close()
            }
        }
    }
}
