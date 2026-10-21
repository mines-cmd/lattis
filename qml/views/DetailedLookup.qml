import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    property string gameTitle
    property var gamePlatforms: []
    property string name
    property int platformIndex: 0
    property var results: []
    property int selected: -1
    property int request: 0
    property string error
    property string searchedName
    property string searchedPlatforms

    readonly property var builtIn: Metadata.igdbPlatforms()
    readonly property var platformChoices: {
        const choices = []
        if (gamePlatforms.length > 0) {
            const names = gamePlatforms.map(id => builtIn.find(p => p.id === id)?.name).filter(n => n !== undefined)
            const on = names.length > 1 ? qsTr("%1 or %2").arg(names.slice(0, -1).join(", ")).arg(names[names.length - 1])
                                        : names.length === 1 ? names[0] : qsTr("this game's platforms")
            choices.push({ label: names.length > 0 ? qsTr("This game's platforms (%1)").arg(names.join(", "))
                                                   : qsTr("This game's platforms"),
                           on: on, ids: gamePlatforms })
        }
        choices.push({ label: qsTr("Every platform"), on: "", ids: [] })
        for (const p of builtIn)
            choices.push({ label: p.name, on: p.name, ids: [p.id] })
        return choices
    }
    readonly property int everyPlatformIndex: gamePlatforms.length > 0 ? 1 : 0
    readonly property var current: selected >= 0 && selected < results.length ? results[selected] : null

    signal chosen(var candidate, string searched)
    signal closed()

    function openFor(title, platforms, searchName, everyPlatform) {
        gameTitle = title
        gamePlatforms = platforms.map(id => Number(id))
        name = searchName
        nameField.text = searchName
        platformIndex = everyPlatform ? everyPlatformIndex : 0
        results = []
        selected = -1
        popup.open()
        search()
    }

    function search() {
        const text = nameField.text.trim()
        if (text === "")
            return
        Metadata.cancel(request)
        name = text
        error = ""
        searchedName = text
        searchedPlatforms = platformChoices[platformIndex].on
        request = Metadata.lookup(text, platformChoices[platformIndex].ids, 0, 0, 50)
    }

    function searchEveryPlatform() {
        platformIndex = everyPlatformIndex
        search()
    }

    function use(index) {
        if (index < 0 || index >= results.length)
            return
        chosen(results[index], searchedName)
        popup.close()
    }

    Connections {
        target: Metadata
        function onFinished(id, result) {
            if (id !== root.request)
                return
            root.request = 0
            if (result.error !== "") {
                root.error = result.error
                return
            }
            root.results = result.candidates
            root.selected = result.candidates.length > 0 ? 0 : -1
            list.positionViewAtBeginning()
        }
    }

    MotionPopup {
        id: popup

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(Theme.gameEditor.detailedWidth, parent.width - 2 * Theme.layout.margin)
        height: parent.height * Theme.gameEditor.detailedMaxHeight
        modal: true
        focus: true
        padding: Theme.gameEditor.padding
        closePolicy: Popup.CloseOnEscape
        onOpened: nameField.forceActiveFocus()
        onClosed: {
            Metadata.cancel(root.request)
            root.request = 0
            root.closed()
        }

        Overlay.modal: Scrim {}
        background: Surface {
            radius: Theme.gameEditor.radius
            fill: Theme.gameEditor.background
            fillEnd: Theme.gameEditor.backgroundEnd
            direction: Theme.gameEditor.gradientDirection
            border.color: Theme.gameEditor.border
            border.width: Theme.gameEditor.borderWidth
        }

        contentItem: ColumnLayout {
            spacing: Theme.gameEditor.rowSpacing

            Label {
                Layout.fillWidth: true
                Layout.rightMargin: Theme.about.closeSize + Theme.gameEditor.columnSpacing
                text: qsTr("Look up %1 in IGDB").arg(root.gameTitle)
                color: Theme.gameEditor.headingColor
                font.bold: true
                font.pixelSize: Theme.gameEditor.headingFontSize
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                SettingsTextField {
                    id: nameField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Name to look up")
                    onAccepted: root.search()
                    Keys.onDownPressed: list.forceActiveFocus()
                }
                SettingsComboBox {
                    id: platformBox
                    Layout.preferredWidth: Theme.gameEditor.detailedDetailsWidth
                    model: root.platformChoices.map(c => c.label)
                    currentIndex: root.platformIndex
                    onActivated: index => {
                        root.platformIndex = index
                        root.search()
                    }
                }
                SettingsButton {
                    text: qsTr("Search")
                    enabled: nameField.text.trim() !== ""
                    onClicked: root.search()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.gameEditor.rowSpacing

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: list
                        anchors.fill: parent
                        clip: true
                        opacity: root.request !== 0 ? Theme.gameEditor.detailedBusyOpacity : 1
                        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                        model: root.results
                        currentIndex: root.selected
                        keyNavigationEnabled: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: SettingsScrollBar {}
                        onCurrentIndexChanged: root.selected = currentIndex
                        Keys.onReturnPressed: root.use(root.selected)
                        Keys.onEnterPressed: root.use(root.selected)

                        delegate: Rectangle {
                            id: row
                            required property var modelData
                            required property int index
                            width: ListView.view.width - Theme.layout.spacing
                            height: rowLayout.implicitHeight + Theme.layout.spacing
                            radius: Theme.controls.inputRadius
                            color: root.selected === index ? Theme.gameEditor.detailedSelected
                                 : rowMouse.containsMouse ? Theme.gameEditor.pickHover : "transparent"
                            Behavior on color { enabled: Motion.fades; MotionColor {} }

                            RowLayout {
                                id: rowLayout
                                anchors.fill: parent
                                anchors.margins: Theme.layout.spacing / 2
                                anchors.leftMargin: Theme.layout.spacing
                                spacing: Theme.layout.spacing

                                Rectangle {
                                    implicitHeight: Theme.gameEditor.detailedCoverHeight
                                    implicitWidth: implicitHeight * 3 / 4
                                    radius: Theme.controls.inputRadius
                                    color: Theme.gameEditor.artPreviewBackground
                                    clip: true
                                    FadeImage {
                                        anchors.fill: parent
                                        source: row.modelData.cover
                                        sourceSize.height: parent.height * 2
                                        fillMode: Image.PreserveAspectCrop
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Label {
                                        Layout.fillWidth: true
                                        text: row.modelData.name
                                        color: Theme.gameEditor.lookupText
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: [row.modelData.year > 0 ? String(row.modelData.year) : qsTr("Year unknown")]
                                                  .concat(row.modelData.platforms).join(" · ")
                                        color: Theme.gameEditor.hintColor
                                        font.pixelSize: Theme.gameEditor.hintFontSize
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                            MouseArea {
                                id: rowMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.selected = row.index
                                    list.forceActiveFocus()
                                }
                                onDoubleClicked: root.use(row.index)
                            }
                        }
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: parent.width - 2 * Theme.gameEditor.padding
                        visible: (root.request !== 0 && root.results.length === 0) || root.error !== ""
                                 || (root.request === 0 && root.results.length === 0 && root.searchedName !== "")
                        spacing: Theme.layout.spacing

                        Icon {
                            Layout.alignment: Qt.AlignHCenter
                            visible: root.request !== 0
                            path: Icons.loader
                            size: Theme.controls.iconSize
                            color: Theme.gameEditor.hintColor
                            RotationAnimation on rotation {
                                running: root.request !== 0
                                from: 0
                                to: 360
                                duration: Theme.iconButton.spinDuration
                                loops: Animation.Infinite
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            text: root.request !== 0 ? qsTr("Looking up %1…").arg(root.searchedName)
                                : root.error !== "" ? root.error
                                : root.searchedPlatforms !== "" ? qsTr("IGDB has nothing called %1 on %2.").arg(root.searchedName).arg(root.searchedPlatforms)
                                : qsTr("IGDB has nothing called %1.").arg(root.searchedName)
                            color: root.error !== "" && root.request === 0 ? Theme.gameEditor.errorColor : Theme.gameEditor.hintColor
                        }
                        SettingsButton {
                            Layout.alignment: Qt.AlignHCenter
                            visible: root.request === 0 && (root.error !== "" || root.searchedPlatforms !== "")
                            text: root.error !== "" ? qsTr("Retry") : qsTr("Search every platform")
                            onClicked: root.error !== "" ? root.search() : root.searchEveryPlatform()
                        }
                    }
                }

                Flickable {
                    id: details
                    Layout.preferredWidth: Theme.gameEditor.detailedDetailsWidth
                    Layout.fillHeight: true
                    visible: root.current !== null
                    clip: true
                    contentHeight: detailsColumn.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: SettingsScrollBar {}

                    ColumnLayout {
                        id: detailsColumn
                        width: details.width - Theme.layout.spacing
                        spacing: Theme.layout.spacing

                        MotionNumber {
                            id: detailsFade
                            target: detailsColumn
                            property: "opacity"
                            from: 0
                            to: 1
                            linear: true
                        }
                        Connections {
                            target: root
                            function onCurrentChanged() {
                                if (Motion.on && popup.opened)
                                    detailsFade.restart()
                            }
                        }

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            implicitHeight: Theme.gameEditor.detailedLargeCoverHeight
                            implicitWidth: implicitHeight * 3 / 4
                            radius: Theme.controls.inputRadius
                            color: Theme.gameEditor.artPreviewBackground
                            clip: true
                            FadeImage {
                                anchors.fill: parent
                                source: root.current?.coverLarge ?? ""
                                sourceSize.height: parent.height * 2
                                fillMode: Image.PreserveAspectCrop
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: root.current?.name ?? ""
                            color: Theme.gameEditor.headingColor
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }
                        Label {
                            Layout.fillWidth: true
                            text: {
                                const c = root.current
                                if (!c)
                                    return ""
                                const parts = [c.year > 0 ? String(c.year) : qsTr("Year unknown")]
                                if (c.rating >= 0)
                                    parts.push(qsTr("%1 of 5 stars").arg(c.rating))
                                return parts.join(" · ")
                            }
                            color: Theme.gameEditor.hintColor
                            wrapMode: Text.WordWrap
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: text !== ""
                            text: (root.current?.platforms ?? []).join(", ")
                            color: Theme.gameEditor.hintColor
                            wrapMode: Text.WordWrap
                        }
                        Label {
                            Layout.fillWidth: true
                            text: (root.current?.summary ?? "") !== "" ? root.current.summary : qsTr("No description")
                            color: (root.current?.summary ?? "") !== "" ? Theme.gameEditor.lookupText : Theme.gameEditor.hintColor
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Label {
                    Layout.fillWidth: true
                    visible: root.results.length > 0
                    text: root.results.length === 1 ? qsTr("1 game") : qsTr("%1 games").arg(root.results.length)
                    color: Theme.gameEditor.hintColor
                    elide: Text.ElideRight
                }
                Item { Layout.fillWidth: true; visible: root.results.length === 0 }
                SettingsButton {
                    text: qsTr("Cancel")
                    onClicked: popup.close()
                }
                SettingsButton {
                    text: qsTr("Use this game")
                    primary: true
                    enabled: root.current !== null
                    onClicked: root.use(root.selected)
                }
            }
        }

        Icon {
            parent: popup.background
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: Theme.gameEditor.padding
            path: Icons.close
            size: Theme.about.closeSize
            color: closeMouse.containsMouse ? Theme.about.closeHoverColor : Theme.about.closeColor

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                anchors.margins: -6
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: popup.close()
            }

            ToolTip.visible: closeMouse.containsMouse
            ToolTip.delay: Theme.tooltip.delay
            ToolTip.text: qsTr("Close")
        }
    }
}
