import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Rectangle {
    id: root

    property string phase: ""
    property string title
    property var match: null
    property var candidates: []
    property string hint
    property var differs: []
    property bool filledAny: false
    property bool canChooseArt: false

    signal cancel()
    signal undo()
    signal notThisGame()
    signal replaceToo()
    signal chooseArt()
    signal picked(var candidate)
    signal noneOfThese()
    signal notOnIgdb()
    signal detailedLookup()
    signal dismissed()

    visible: phase !== ""
    implicitHeight: content.implicitHeight + 2 * Theme.gameEditor.lookupPadding
    radius: Theme.gameEditor.lookupRadius
    color: Theme.gameEditor.lookupBackground

    component Action: SettingsButton {
        implicitHeight: Theme.controls.buttonHeight
        leftPadding: 10
        rightPadding: 10
    }

    component Cover: Rectangle {
        property string source
        implicitHeight: Theme.gameEditor.pickCoverHeight
        implicitWidth: implicitHeight * 3 / 4
        radius: Theme.controls.inputRadius
        color: Theme.gameEditor.artPreviewBackground
        clip: true

        FadeImage {
            anchors.fill: parent
            source: parent.source
            sourceSize.height: parent.height * 2
            fillMode: Image.PreserveAspectCrop
        }
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Theme.gameEditor.lookupPadding
        spacing: Theme.layout.spacing

        RowLayout {
            visible: root.phase === "running"
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Icon {
                path: Icons.loader
                size: Theme.controls.iconSize
                color: Theme.gameEditor.lookupText
                RotationAnimation on rotation {
                    running: root.phase === "running"
                    from: 0
                    to: 360
                    duration: Theme.iconButton.spinDuration
                    loops: Animation.Infinite
                }
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Looking up %1…").arg(root.title)
                color: Theme.gameEditor.lookupText
                elide: Text.ElideRight
            }
        }

        RowLayout {
            visible: root.phase === "matched"
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Cover {
                source: root.match?.cover ?? ""
                implicitHeight: Theme.gameEditor.pickCoverHeight * 0.6
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing / 2

                Label {
                    Layout.fillWidth: true
                    readonly property string game: root.match === null ? ""
                        : root.match.year > 0 ? qsTr("%1 (%2)").arg(root.match.name).arg(root.match.year) : root.match.name
                    text: root.filledAny ? qsTr("Filled in from IGDB: %1").arg(game) : qsTr("Found on IGDB: %1").arg(game)
                    color: Theme.gameEditor.lookupText
                    elide: Text.ElideRight
                }
                RowLayout {
                    visible: root.differs.length > 0
                    Layout.fillWidth: true
                    spacing: Theme.layout.spacing

                    Label {
                        Layout.fillWidth: true
                        text: root.differs.length === 1 ? qsTr("%1 differs from what you have").arg(root.differs[0])
                                                        : qsTr("%1 differ from what you have").arg(root.differs.join(", "))
                        color: Theme.gameEditor.hintColor
                        font.pixelSize: Theme.gameEditor.hintFontSize
                        wrapMode: Text.WordWrap
                    }
                }
            }
            Action {
                visible: root.differs.length > 0
                text: root.differs.length === 1 ? qsTr("Replace it too") : qsTr("Replace these too")
                onClicked: root.replaceToo()
            }
            Action {
                visible: root.canChooseArt
                text: qsTr("Choose art")
                onClicked: root.chooseArt()
            }
            Action {
                text: qsTr("Undo lookup")
                iconPath: Icons.undo
                onClicked: root.undo()
            }
            Action {
                text: qsTr("Not this game?")
                onClicked: root.notThisGame()
            }
            Action {
                text: qsTr("Detailed lookup")
                tip: qsTr("Search another name or platform, and see every result")
                onClicked: root.detailedLookup()
            }
        }

        Label {
            visible: root.phase === "pick"
            Layout.fillWidth: true
            text: root.candidates.length > 0 ? qsTr("Which game is %1?").arg(root.title)
                                             : qsTr("IGDB has no other game called %1.").arg(root.title)
            color: Theme.gameEditor.lookupText
            elide: Text.ElideRight
        }
        Flow {
            visible: root.phase === "pick" && root.candidates.length > 0
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Repeater {
                model: root.candidates.slice(0, 5)

                Rectangle {
                    id: candidate
                    required property var modelData
                    width: Math.min(pickRow.implicitWidth + 2 * Theme.layout.spacing, content.width)
                    height: pickRow.implicitHeight + 2 * Theme.layout.spacing / 2
                    radius: Theme.controls.inputRadius
                    color: pickMouse.containsMouse ? Theme.gameEditor.pickHover : "transparent"

                    RowLayout {
                        id: pickRow
                        anchors.fill: parent
                        anchors.margins: Theme.layout.spacing / 2
                        anchors.leftMargin: Theme.layout.spacing
                        spacing: Theme.layout.spacing

                        Cover { source: candidate.modelData.cover }
                        ColumnLayout {
                            spacing: 2
                            Label {
                                text: candidate.modelData.name
                                color: Theme.gameEditor.lookupText
                                font.bold: true
                                Layout.maximumWidth: Theme.gameEditor.width / 4
                                elide: Text.ElideRight
                            }
                            Label {
                                text: candidate.modelData.year > 0 ? String(candidate.modelData.year) : qsTr("Year unknown")
                                color: Theme.gameEditor.hintColor
                                font.pixelSize: Theme.gameEditor.hintFontSize
                            }
                            Label {
                                text: candidate.modelData.platforms.join(", ")
                                visible: text !== ""
                                color: Theme.gameEditor.hintColor
                                font.pixelSize: Theme.gameEditor.hintFontSize
                                Layout.maximumWidth: Theme.gameEditor.width / 4
                                elide: Text.ElideRight
                            }
                        }
                    }
                    MouseArea {
                        id: pickMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.picked(candidate.modelData)
                    }
                }
            }
        }
        RowLayout {
            visible: root.phase === "pick"
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Item { Layout.fillWidth: true }
            Action {
                text: qsTr("Detailed lookup")
                tip: qsTr("Search another name or platform, and see every result")
                onClicked: root.detailedLookup()
            }
            Action {
                text: qsTr("None of these")
                onClicked: root.noneOfThese()
            }
            Action {
                text: qsTr("Not on IGDB")
                tip: qsTr("Remember that IGDB doesn't have this game, so looking up missing info skips it")
                onClicked: root.notOnIgdb()
            }
        }

        RowLayout {
            visible: root.phase === "hint"
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                text: root.hint
                color: Theme.gameEditor.lookupText
                wrapMode: Text.WordWrap
            }
            Action {
                text: qsTr("Detailed lookup")
                tip: qsTr("Search another name or platform, and see every result")
                onClicked: root.detailedLookup()
            }
            Action {
                iconPath: Icons.close
                tip: qsTr("Close")
                onClicked: root.dismissed()
            }
        }
    }
}
