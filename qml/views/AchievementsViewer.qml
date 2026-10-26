import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
MotionPopup {
    id: root

    property string gameTitle
    property var items: []
    property string query: ""
    property var revealed: ({})
    readonly property int unlocked: items.filter(a => a.unlocked).length
    readonly property string needle: query.trim().toLowerCase()
    readonly property var matches: items
        .filter(a => needle === "" || !concealed(a)
                     && (a.name.toLowerCase().includes(needle) || a.description.toLowerCase().includes(needle)))
        .map(a => Object.assign({ group: a.unlocked ? 0 : a.hidden ? 2 : 1 }, a))
        .sort((a, b) => a.group - b.group)
    readonly property var groupNames: [qsTr("Unlocked"), qsTr("Locked"), qsTr("Hidden")]

    function show(title, list) {
        gameTitle = title
        items = list
        query = ""
        revealed = ({})
        open()
    }

    function concealed(a): bool {
        return a.hidden && !a.unlocked && !revealed[a.apiName]
    }

    function reveal(a) {
        const next = Object.assign({}, revealed)
        next[a.apiName] = true
        revealed = next
    }

    function description(a): string {
        return concealed(a) ? qsTr("Details for this achievement will be revealed once you've unlocked it") : a.description
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
            radius: Theme.achievements.radius
            fill: Theme.achievements.viewerBackground
            border.color: Theme.achievements.border
            border.width: Theme.achievements.borderWidth

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.achievements.viewerPadding
                spacing: Theme.layout.spacing * 3

                Label {
                    Layout.fillWidth: true
                    text: root.gameTitle
                    elide: Text.ElideRight
                    color: Theme.achievements.titleColor
                    font.pixelSize: Theme.achievements.viewerTitleSize
                    font.weight: Theme.font.strongWeight
                }

                Surface {
                    Layout.fillWidth: true
                    implicitHeight: summary.implicitHeight + 2 * Theme.achievements.padding
                    radius: Theme.achievements.radius
                    fill: Theme.achievements.background
                    fillEnd: Theme.achievements.backgroundEnd
                    direction: Theme.achievements.gradientDirection

                    ColumnLayout {
                        id: summary
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter
                                  margins: Theme.achievements.padding }
                        spacing: Theme.layout.spacing * 2

                        Label {
                            readonly property bool complete: root.items.length > 0 && root.unlocked === root.items.length
                            text: qsTr("%1 / %2 achievements unlocked (%3%)").arg(root.unlocked).arg(root.items.length)
                                      .arg(root.items.length > 0 ? Math.round(100 * root.unlocked / root.items.length) : 0)
                            color: complete ? Theme.achievements.completeColor : Theme.achievements.titleColor
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: Theme.achievements.barHeight
                            radius: height / 2
                            color: Theme.achievements.barColor

                            Rectangle {
                                width: root.items.length > 0 ? parent.width * root.unlocked / root.items.length : 0
                                height: parent.height
                                radius: parent.radius
                                color: Theme.achievements.barFill
                            }
                        }
                    }
                }

                SettingsSearchField {
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search achievements")
                    text: root.query
                    onTextChanged: root.query = text
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
                        height: implicitHeight + Theme.layout.spacing * 3
                        verticalAlignment: Text.AlignBottom
                        text: root.groupNames[Number(section)].toUpperCase()
                              + " (" + root.matches.filter(a => a.group === Number(section)).length + ")"
                        color: Theme.achievements.sectionColor
                        font.weight: Theme.font.strongWeight
                    }

                    delegate: Item {
                        id: row
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        height: Theme.achievements.viewerIconSize + 2 * Theme.layout.spacing * 2

                        TapHandler {
                            enabled: root.concealed(row.modelData)
                            onTapped: root.reveal(row.modelData)
                        }
                        HoverHandler {
                            id: rowHover
                            cursorShape: root.concealed(row.modelData) ? Qt.PointingHandCursor : Qt.ArrowCursor
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.achievements.iconRadius
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
                            anchors.verticalCenter: parent.verticalCenter
                            width: Theme.achievements.viewerIconSize
                            aspect: 1
                            cornerRadius: Theme.achievements.iconRadius
                            color: Theme.achievements.iconPlaceholder
                            failedIcon: Icons.medal
                            iconWhenEmpty: !root.concealed(row.modelData)
                            source: root.concealed(row.modelData) ? "" : row.modelData.icon ?? ""
                            opacity: row.modelData.unlocked ? 1 : Theme.achievements.lockedOpacity

                            Label {
                                visible: root.concealed(row.modelData)
                                anchors.centerIn: parent
                                text: "?"
                                color: Theme.achievements.countColor
                                font.pixelSize: parent.height * 0.5
                            }
                        }
                        ColumnLayout {
                            anchors { left: icon.right; right: stamp.left; verticalCenter: parent.verticalCenter
                                      leftMargin: Theme.layout.spacing * 3; rightMargin: Theme.layout.spacing * 3 }
                            spacing: 0

                            Label {
                                Layout.fillWidth: true
                                text: root.concealed(row.modelData) ? qsTr("Hidden achievement") : row.modelData.name
                                elide: Text.ElideRight
                                color: Theme.achievements.titleColor
                                font.weight: Theme.font.strongWeight
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: text !== ""
                                text: root.description(row.modelData)
                                elide: Text.ElideRight
                                color: Theme.achievements.countColor
                            }
                        }
                        Label {
                            id: stamp
                            anchors { right: parent.right; verticalCenter: parent.verticalCenter
                                      rightMargin: Theme.layout.spacing * 3 }
                            text: row.modelData.unlocked
                                  ? qsTr("Unlocked %1").arg(row.modelData.time.toLocaleString(Qt.locale(), General.shortDateFormat + ", HH:mm"))
                                  : ""
                            color: Theme.achievements.countColor
                        }
                    }
                }
            }
        }

        IconButton {
            readonly property real gap: Theme.achievements.viewerCloseGap
            plain: true
            x: Math.min(parent.width - width - gap, card.x + card.width + gap)
            y: card.y
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
