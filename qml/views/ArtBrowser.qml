import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    readonly property var kinds: [
        { id: "cover", name: qsTr("Cover") },
        { id: "hero", name: qsTr("Hero") },
        { id: "logo", name: qsTr("Logo") },
        { id: "icon", name: qsTr("Icon") }
    ]
    property int steamGridDbId: 0
    property string gameTitle
    property string kind: "cover"
    property var picks: ({})
    property var tabs: ({})
    readonly property var models: ({ cover: coverModel, hero: heroModel, logo: logoModel, icon: iconModel })
    readonly property var tab: tabs[kind] ?? ({})
    property var downloading: ({})
    property var downloaded: ({})

    property bool bulk: false
    property var steps: []
    property int stepIndex: 0
    property var shownKinds: kinds.map(k => k.id)

    signal chosen(var files)
    signal stepPicked(string id, string kind, var image)
    signal closed()

    function openSteps(allSteps, index, startKind) {
        bulk = true
        steps = allSteps
        showStep(index, startKind)
        popup.open()
    }

    function showStep(index, startKind) {
        stepIndex = index
        const step = steps[index]
        shownKinds = step.kinds
        openFor(step.steamGridDbId, step.title, step.kinds.includes(startKind) ? startKind : step.kinds[0], step.picks)
    }

    function openFor(steamGridDbId, title, startKind, preselected) {
        if (!popup.opened && !bulk)
            shownKinds = kinds.map(k => k.id)
        root.steamGridDbId = steamGridDbId
        gameTitle = title
        kind = startKind
        picks = Object.assign({}, preselected ?? {})
        downloading = {}
        downloaded = {}
        error.text = ""
        for (const k of kinds)
            models[k.id].clear()
        const fresh = {}
        for (const k of kinds)
            fresh[k.id] = { style: "", page: 0, end: false, loading: false, error: "", request: 0 }
        tabs = fresh
        if (!bulk)
            popup.open()
        load(kind)
    }

    function setTab(k, values) {
        const next = Object.assign({}, tabs)
        next[k] = Object.assign({}, tabs[k], values)
        tabs = next
    }

    function load(k, restart) {
        const t = tabs[k]
        if (restart) {
            Metadata.cancel(t.request)
            models[k].clear()
        } else if (t.loading || t.end) {
            return
        }
        const page = restart ? 0 : t.page
        setTab(k, { page: page, end: false, loading: true, error: "",
                    request: Metadata.artChoices(steamGridDbId, k, t.style, page) })
    }

    function toggle(image) {
        const next = Object.assign({}, picks)
        if (next[kind]?.id === image.id)
            delete next[kind]
        else
            next[kind] = image
        picks = next
        if (bulk) {
            const s = steps.slice()
            s[stepIndex] = Object.assign({}, s[stepIndex], { picks: next })
            steps = s
            stepPicked(s[stepIndex].id, kind, next[kind] ?? {})
        }
    }

    function useSelected() {
        const running = {}
        for (const k of Object.keys(picks))
            running[k] = Metadata.download(picks[k].url, k)
        downloading = running
        downloaded = {}
        error.text = ""
        if (Object.keys(running).length === 0) {
            chosen({})
            popup.close()
        }
    }

    Connections {
        target: Metadata
        function onFinished(request, result) {
            for (const k of Object.keys(root.tabs)) {
                if (root.tabs[k].request !== request)
                    continue
                if (result.error !== "") {
                    root.setTab(k, { loading: false, request: 0, error: result.error })
                    return
                }
                for (const image of result.images)
                    root.models[k].append(image)
                root.setTab(k, { loading: false, request: 0, page: root.tabs[k].page + 1, end: result.end })
                return
            }
            for (const k of Object.keys(root.downloading)) {
                if (root.downloading[k] !== request)
                    continue
                const running = Object.assign({}, root.downloading)
                delete running[k]
                root.downloading = running
                if (result.error !== "") {
                    error.text = result.error
                    for (const other of Object.keys(running))
                        Metadata.cancel(running[other])
                    root.downloading = {}
                    return
                }
                const files = Object.assign({}, root.downloaded)
                files[k] = result.path
                root.downloaded = files
                if (Object.keys(running).length === 0) {
                    root.chosen(files)
                    popup.close()
                }
                return
            }
        }
    }

    ListModel { id: coverModel }
    ListModel { id: heroModel }
    ListModel { id: logoModel }
    ListModel { id: iconModel }

    MotionPopup {
        id: popup

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(Theme.gameEditor.browserWidth, parent.width - 2 * Theme.layout.margin)
        height: parent.height * Theme.gameEditor.browserMaxHeight
        modal: true
        focus: true
        padding: Theme.gameEditor.padding
        closePolicy: Popup.CloseOnEscape
        onClosed: {
            for (const k of Object.keys(root.tabs))
                Metadata.cancel(root.tabs[k].request)
            for (const k of Object.keys(root.downloading))
                Metadata.cancel(root.downloading[k])
            root.downloading = {}
            root.bulk = false
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
                text: root.bulk ? qsTr("%1 of %2 · Art for %3").arg(root.stepIndex + 1).arg(root.steps.length).arg(root.gameTitle)
                                : qsTr("Art for %1").arg(root.gameTitle)
                color: Theme.gameEditor.headingColor
                font.bold: true
                font.pixelSize: Theme.gameEditor.headingFontSize
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Repeater {
                    model: root.kinds.filter(k => root.shownKinds.includes(k.id))

                    AbstractButton {
                        id: tabButton
                        required property var modelData
                        readonly property bool current: root.kind === modelData.id
                        implicitHeight: Theme.settingsWindow.toolbarHeight
                        implicitWidth: tabRow.implicitWidth + 24
                        hoverEnabled: true
                        onClicked: {
                            root.kind = modelData.id
                            root.load(modelData.id)
                        }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }

                        background: Rectangle {
                            radius: Theme.settingsWindow.inputRadius
                            color: tabButton.current ? Theme.settingsWindow.buttonBackground
                                 : tabButton.hovered ? Theme.settingsWindow.rowHover : "transparent"
                        }
                        contentItem: Item {
                            RowLayout {
                                id: tabRow
                                anchors.centerIn: parent
                                spacing: 6
                                Label {
                                    text: tabButton.modelData.name
                                    color: Theme.settingsWindow.buttonText
                                    font.bold: tabButton.current
                                }
                                Rectangle {
                                    visible: root.picks[tabButton.modelData.id] !== undefined
                                    implicitWidth: 7
                                    implicitHeight: 7
                                    radius: 3.5
                                    color: Theme.gameEditor.selectedColor
                                }
                            }
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: qsTr("Style")
                    color: Theme.gameEditor.labelColor
                }
                SettingsComboBox {
                    id: styleBox
                    readonly property var styles: [{ id: "", name: qsTr("All styles") }].concat(Metadata.styles(root.kind))
                    Layout.preferredWidth: 170
                    model: styles.map(s => s.name)
                    currentIndex: Math.max(0, styles.findIndex(s => s.id === (root.tab.style ?? "")))
                    onActivated: index => {
                        root.setTab(root.kind, { style: styles[index].id })
                        root.load(root.kind, true)
                    }
                }
            }

            GridView {
                id: grid

                readonly property real thumbHeight: ({ cover: Theme.gameEditor.browserCoverHeight,
                                                       hero: Theme.gameEditor.browserHeroHeight,
                                                       logo: Theme.gameEditor.browserLogoHeight,
                                                       icon: Theme.gameEditor.browserIconSize })[root.kind]
                readonly property real thumbWidth: ({ cover: thumbHeight * 2 / 3, hero: thumbHeight * 1920 / 620,
                                                      logo: thumbHeight * 2.5, icon: thumbHeight })[root.kind]

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.models[root.kind]
                cellWidth: thumbWidth + Theme.gameEditor.browserSpacing
                cellHeight: thumbHeight + Theme.gameEditor.browserSpacing
                boundsBehavior: Flickable.StopAtBounds
                cacheBuffer: 0
                ScrollBar.vertical: SettingsScrollBar {}

                MotionNumber {
                    id: tabFade
                    target: grid
                    property: "opacity"
                    from: 0
                    to: 1
                    linear: true
                }
                Connections {
                    target: root
                    function onKindChanged() {
                        if (Motion.on && popup.opened)
                            tabFade.restart()
                    }
                }

                delegate: Item {
                    id: thumb
                    required property var model
                    readonly property bool picked: root.picks[root.kind]?.id === model.id
                    width: grid.thumbWidth
                    height: grid.thumbHeight

                    Canvas {
                        anchors.fill: parent
                        visible: root.kind === "logo" || root.kind === "icon"
                        onPaint: {
                            const ctx = getContext("2d")
                            const size = Theme.gameEditor.checkerSize
                            for (let y = 0; y < height; y += size) {
                                for (let x = 0; x < width; x += size) {
                                    ctx.fillStyle = ((x + y) / size) % 2 === 0 ? Theme.gameEditor.checkerLight
                                                                               : Theme.gameEditor.checkerDark
                                    ctx.fillRect(x, y, size, size)
                                }
                            }
                        }
                    }
                    Rectangle {
                        anchors.fill: parent
                        visible: root.kind === "cover" || root.kind === "hero"
                        color: Theme.gameEditor.artPreviewBackground
                    }
                    FadeImage {
                        anchors.fill: parent
                        anchors.margins: root.kind === "logo" ? 6 : 0
                        source: thumb.model.thumbnail
                        sourceSize.width: width * 2
                        fillMode: root.kind === "cover" || root.kind === "hero" ? Image.PreserveAspectCrop
                                                                               : Image.PreserveAspectFit
                    }
                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        border.color: Theme.gameEditor.selectedColor
                        border.width: Theme.gameEditor.selectedWidth
                        opacity: thumb.picked ? 1 : 0
                        visible: opacity > 0
                        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                    }
                    MarkPop {
                        shown: thumb.picked
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 6
                        width: 22
                        height: 22

                        Rectangle {
                            anchors.fill: parent
                            radius: 11
                            color: Theme.gameEditor.selectedColor
                            Icon {
                                anchors.centerIn: parent
                                path: Icons.check
                                size: 16
                                color: Theme.settingsWindow.buttonPrimaryText
                            }
                        }
                    }
                    MouseArea {
                        id: thumbMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.toggle({ id: thumb.model.id, url: thumb.model.url, thumbnail: thumb.model.thumbnail,
                                                 width: thumb.model.width, height: thumb.model.height,
                                                 style: thumb.model.style, author: thumb.model.author })
                    }
                    ToolTip.visible: thumbMouse.containsMouse
                    ToolTip.delay: Theme.tooltip.delay
                    ToolTip.text: (thumb.model.width > 0 ? qsTr("%1 × %2").arg(thumb.model.width).arg(thumb.model.height) : "")
                                  + (thumb.model.author !== "" ? qsTr(" · by %1").arg(thumb.model.author) : "")
                }

                footer: Item {
                    width: grid.width
                    height: loadMore.visible || status.visible ? loadMore.implicitHeight + 2 * Theme.layout.spacing : 0

                    SettingsButton {
                        id: loadMore
                        anchors.centerIn: parent
                        visible: !root.tab.end && !root.tab.loading && (root.tab.error ?? "") === "" && grid.count > 0
                        text: qsTr("Load more")
                        onClicked: root.load(root.kind)
                    }
                    Label {
                        id: status
                        anchors.centerIn: parent
                        visible: root.tab.loading || grid.count === 0 || (root.tab.error ?? "") !== ""
                        text: (root.tab.error ?? "") !== "" ? root.tab.error
                            : root.tab.loading ? qsTr("Loading…")
                            : root.tab.style !== "" ? qsTr("No images in this style")
                            : qsTr("SteamGridDB has no images of this kind for this game")
                        color: (root.tab.error ?? "") !== "" ? Theme.gameEditor.errorColor : Theme.gameEditor.hintColor
                    }
                }
            }

            Label {
                id: error
                visible: text !== ""
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.gameEditor.errorColor
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Label {
                    Layout.fillWidth: true
                    text: Object.keys(root.downloading).length > 0 ? qsTr("Downloading…")
                        : root.bulk ? qsTr("The picked image of each kind is used; unpick one to leave that kind out.")
                        : qsTr("Pick one image per tab. They're saved with the game.")
                    color: Theme.gameEditor.hintColor
                    elide: Text.ElideRight
                }
                SettingsButton {
                    visible: root.bulk
                    text: qsTr("Previous game")
                    enabled: root.stepIndex > 0
                    onClicked: root.showStep(root.stepIndex - 1, root.kind)
                }
                SettingsButton {
                    visible: root.bulk
                    text: qsTr("Next game")
                    enabled: root.stepIndex < root.steps.length - 1
                    onClicked: root.showStep(root.stepIndex + 1, root.kind)
                }
                SettingsButton {
                    visible: root.bulk
                    primary: true
                    text: qsTr("Done")
                    onClicked: popup.close()
                }
                SettingsButton {
                    visible: !root.bulk
                    text: qsTr("Cancel")
                    onClicked: popup.close()
                }
                SettingsButton {
                    visible: !root.bulk
                    text: qsTr("Use selected")
                    primary: true
                    enabled: Object.keys(root.picks).length > 0 && Object.keys(root.downloading).length === 0
                    onClicked: root.useSelected()
                }
            }
        }
    }
}
