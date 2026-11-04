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
    property int ownId: 0
    property string ownTitle
    readonly property bool other: steamGridDbId !== ownId
    property bool noGame: false

    property bool searching: false
    property string searchText
    property string searchedText
    property var searchResults: []
    property int searchHighlight: -1
    property int searchRequest: 0
    property string searchError
    property string kind: "cover"
    property var picks: ({})
    property var tabs: ({})
    readonly property var models: ({ cover: coverModel, hero: heroModel, logo: logoModel, icon: iconModel })
    readonly property var tab: tabs[kind] ?? ({})
    property var downloading: ({})
    property var downloaded: ({})

    // 0: not known yet.
    property int steamApp: 0
    property var steamImages: []
    property bool steamLoading: false
    property bool steamLoaded: false
    property string steamError
    property int steamRequest: 0
    property int steamFindRequest: 0
    readonly property var steamKinds: ["cover", "hero", "logo"].filter(k => shownKinds.includes(k))
    readonly property bool steamAvailable: steamKinds.length > 0
    readonly property var shownSteamImages: steamImages.filter(i => steamKinds.includes(i.kind))
    readonly property var tabList: kinds.filter(k => shownKinds.includes(k.id))
                                   .concat(steamAvailable ? [{ id: "steam", name: qsTr("Steam") }] : [])

    property bool bulk: false
    property var steps: []
    property int stepIndex: 0
    property var shownKinds: kinds.map(k => k.id)

    signal chosen(var files)
    signal stepPicked(string id, string kind, var image)
    signal gamePicked(int steamGridDbId)
    signal steamAppFound(int appId)
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
        openFor(step.steamGridDbId, step.title, startKind === "steam" || step.kinds.includes(startKind) ? startKind : step.kinds[0],
                step.picks, step.steamApp)
    }

    function openFor(steamGridDbId, title, startKind, preselected, steamAppId) {
        if (!popup.opened && !bulk)
            shownKinds = kinds.map(k => k.id)
        ownId = steamGridDbId
        ownTitle = title
        picks = Object.assign({}, preselected ?? {})
        downloading = {}
        downloaded = {}
        error.text = ""
        noGame = false
        resetSteam(steamAppId)
        resetSearch()
        showGame(steamGridDbId, title, startKind === "steam" && !steamAvailable ? shownKinds[0] : startKind)
        if (!bulk)
            popup.open()
    }

    function openWithoutGame(title, startKind, steamAppId) {
        ownId = 0
        ownTitle = title
        picks = {}
        downloading = {}
        downloaded = {}
        error.text = ""
        shownKinds = kinds.map(k => k.id)
        resetSteam(steamAppId)
        resetSearch()
        resetTabs(0, title, startKind)
        noGame = true
        searching = true
        popup.open()
    }

    function resetTabs(id, title, startKind) {
        for (const k of Object.keys(tabs))
            Metadata.cancel(tabs[k].request)
        steamGridDbId = id
        gameTitle = title
        kind = startKind ?? kind
        for (const k of kinds)
            models[k.id].clear()
        const fresh = {}
        for (const k of kinds)
            fresh[k.id] = { style: "", page: 0, end: false, loading: false, error: "", request: 0 }
        tabs = fresh
    }

    // The tabs start over; the picks stay.
    function showGame(id, title, startKind) {
        resetTabs(id, title, startKind)
        if (kind === "steam")
            loadSteam()
        else
            load(kind)
    }

    function resetSteam(appId) {
        Metadata.cancel(steamRequest)
        Metadata.cancel(steamFindRequest)
        steamApp = appId ?? 0
        steamImages = []
        steamLoading = false
        steamLoaded = false
        steamError = ""
        steamRequest = 0
        steamFindRequest = 0
    }

    function loadSteam() {
        if (steamLoading || steamLoaded)
            return
        steamError = ""
        const local = steamApp > 0 ? localSteamImages() : []
        if (steamKinds.every(k => local.some(i => i.kind === k))) {
            steamImages = local
            steamLoaded = true
            return
        }
        steamLoading = true
        if (steamApp > 0)
            steamRequest = Metadata.steamArt(steamApp)
        else
            steamFindRequest = Metadata.findSteamApp(ownTitle)
    }

    function steamAppSearched(result) {
        steamFindRequest = 0
        if (result.error !== "") {
            steamLoading = false
            steamError = result.error
        } else if (result.steamAppId > 0) {
            steamApp = result.steamAppId
            if (bulk) {
                const s = steps.slice()
                s[stepIndex] = Object.assign({}, s[stepIndex], { steamApp: steamApp })
                steps = s
            }
            steamAppFound(steamApp)
            steamRequest = Metadata.steamArt(steamApp)
        } else {
            steamLoading = false
            steamLoaded = true
        }
    }

    function steamArtLoaded(result) {
        steamRequest = 0
        steamLoading = false
        if (result.error !== "") {
            steamError = result.error
            return
        }
        const local = localSteamImages()
        steamImages = local.concat(result.images.filter(i => !local.some(l => l.kind === i.kind)))
        steamLoaded = true
    }

    // Files in Steam's own library cache and grid folder come first; the web fills in what's missing.
    function localSteamImages() {
        const files = Library.steamLocalArt(steamApp)
        return Object.keys(files).map(k => ({ kind: k, url: files[k], thumbnail: files[k], width: 0, height: 0, local: true }))
    }

    // Steam's logo path can't tell whether the app has one, so a missing logo shows up here as a failed load.
    function dropSteamImage(url) {
        steamImages = steamImages.filter(i => i.url !== url)
    }

    function thumbHeight(k) {
        return ({ cover: Theme.gameEditor.browserCoverHeight, hero: Theme.gameEditor.browserHeroHeight,
                  logo: Theme.gameEditor.browserLogoHeight, icon: Theme.gameEditor.browserIconSize })[k] ?? 0
    }

    function thumbWidth(k) {
        const h = thumbHeight(k)
        return ({ cover: h * 2 / 3, hero: h * 1920 / 620, logo: h * 2.5, icon: h })[k] ?? 0
    }

    function openSearch() {
        searching = true
        searchHighlight = searchResults.findIndex(g => g.steamgriddb === steamGridDbId)
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    function closeSearch() {
        if (noGame)
            popup.close()
        else
            searching = false
    }

    function resetSearch() {
        Metadata.cancel(searchRequest)
        searchTimer.stop()
        searching = false
        searchRequest = 0
        searchText = ""
        searchedText = ""
        searchResults = []
        searchHighlight = -1
        searchError = ""
    }

    function searchSoon() {
        Metadata.cancel(searchRequest)
        searchRequest = 0
        searchError = ""
        searchResults = []
        searchHighlight = -1
        searchedText = ""
        if (searchText.trim() === "")
            searchTimer.stop()
        else
            searchTimer.restart()
    }

    function searchNow() {
        searchedText = searchText.trim()
        searchRequest = Metadata.searchArtGames(searchedText)
    }

    function pickHighlighted() {
        const game = searchResults[Math.max(0, searchHighlight)]
        if (game)
            pickGame(game)
    }

    function moveHighlight(by) {
        if (searchResults.length === 0)
            return
        searchHighlight = Math.max(0, Math.min(searchResults.length - 1, searchHighlight + by))
        resultList.positionViewAtIndex(searchHighlight, ListView.Contain)
    }

    function pickGame(game) {
        if (noGame) {
            noGame = false
            searching = false
            ownId = game.steamgriddb
            gamePicked(game.steamgriddb)
            showGame(ownId, ownTitle)
            return
        }
        closeSearch()
        if (game.steamgriddb === ownId)
            backToOwn()
        else
            showGame(game.steamgriddb, game.name)
    }

    function backToOwn() {
        if (steamGridDbId !== ownId)
            showGame(ownId, ownTitle)
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

    function toggle(image, pickKind) {
        const next = Object.assign({}, picks)
        if (next[pickKind]?.id === image.id)
            delete next[pickKind]
        else
            next[pickKind] = image
        picks = next
        if (bulk) {
            const s = steps.slice()
            s[stepIndex] = Object.assign({}, s[stepIndex], { picks: next })
            steps = s
            stepPicked(s[stepIndex].id, pickKind, next[pickKind] ?? {})
        }
    }

    function useSelected() {
        const running = {}
        const files = {}
        for (const k of Object.keys(picks)) {
            if (picks[k].url.startsWith("file:"))
                files[k] = picks[k].url
            else
                running[k] = Metadata.download(picks[k].url, k)
        }
        downloading = running
        downloaded = files
        error.text = ""
        if (Object.keys(running).length === 0) {
            chosen(files)
            popup.close()
        }
    }

    Connections {
        target: Metadata
        function onFinished(request, result) {
            if (request === root.searchRequest) {
                root.searchRequest = 0
                if (result.error !== "")
                    root.searchError = result.error
                else
                    root.searchResults = result.games
                return
            }
            if (request === root.steamFindRequest && request !== 0) {
                root.steamAppSearched(result)
                return
            }
            if (request === root.steamRequest && request !== 0) {
                root.steamArtLoaded(result)
                return
            }
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

    Timer {
        id: searchTimer
        interval: 300
        onTriggered: root.searchNow()
    }

    component Checker: Canvas {
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
        onOpened: if (root.searching) searchField.forceActiveFocus()
        onClosed: {
            for (const k of Object.keys(root.tabs))
                Metadata.cancel(root.tabs[k].request)
            Metadata.cancel(root.steamRequest)
            Metadata.cancel(root.steamFindRequest)
            for (const k of Object.keys(root.downloading))
                Metadata.cancel(root.downloading[k])
            root.downloading = {}
            root.resetSearch()
            root.noGame = false
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

        contentItem: ColumnLayout {
            spacing: Theme.gameEditor.rowSpacing

            RowLayout {
                Layout.fillWidth: true
                Layout.rightMargin: Theme.about.closeSize + Theme.gameEditor.columnSpacing
                spacing: Theme.layout.spacing

                Label {
                    Layout.fillWidth: true
                    Layout.maximumWidth: implicitWidth
                    text: (root.bulk ? qsTr("%1 of %2 · ").arg(root.stepIndex + 1).arg(root.steps.length) : "")
                          + (root.other ? qsTr("Art from %1") : qsTr("Art for %1")).arg(root.gameTitle)
                    color: Theme.gameEditor.headingColor
                    font.weight: Theme.font.strongWeight
                    font.pixelSize: Theme.gameEditor.headingFontSize
                    elide: Text.ElideRight
                }
                TextLink {
                    id: backLink
                    visible: root.other
                    text: qsTr("Back to %1").arg(root.ownTitle)
                    onClicked: {
                        root.closeSearch()
                        root.backToOwn()
                    }
                }
                Item { Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Repeater {
                    model: root.tabList

                    AbstractButton {
                        id: tabButton
                        required property var modelData
                        readonly property bool current: root.kind === modelData.id
                        implicitHeight: Theme.controls.buttonHeight
                        implicitWidth: tabRow.implicitWidth + 24
                        hoverEnabled: true
                        onClicked: {
                            if (root.noGame) {
                                root.kind = modelData.id
                                return
                            }
                            root.closeSearch()
                            root.kind = modelData.id
                            if (modelData.id === "steam")
                                root.loadSteam()
                            else
                                root.load(modelData.id)
                        }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }

                        background: Rectangle {
                            radius: Theme.controls.inputRadius
                            color: tabButton.current ? Theme.controls.buttonBackground
                                 : tabButton.hovered ? Theme.controls.hover : "transparent"
                        }
                        contentItem: Item {
                            RowLayout {
                                id: tabRow
                                anchors.centerIn: parent
                                spacing: 6
                                Label {
                                    text: tabButton.modelData.name
                                    color: Theme.controls.buttonText
                                    font.weight: tabButton.current ? Theme.font.strongWeight : Font.Normal
                                }
                                Rectangle {
                                    visible: tabButton.modelData.id === "steam"
                                             ? Object.values(root.picks).some(p => p.steam === true)
                                             : root.picks[tabButton.modelData.id] !== undefined
                                    implicitWidth: 7
                                    implicitHeight: 7
                                    radius: height / 2
                                    color: Theme.gameEditor.selectedColor
                                }
                            }
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                SettingsTextField {
                    id: searchField
                    visible: root.searching
                    Layout.preferredWidth: parent.width / 2
                    implicitHeight: Theme.controls.buttonHeight
                    leftPadding: Theme.controls.iconSize + 22
                    rightPadding: Theme.controls.iconSize + 22
                    placeholderText: qsTr("Search SteamGridDB")
                    text: root.searchText
                    onTextEdited: {
                        root.searchText = text
                        root.searchSoon()
                    }
                    Keys.onEscapePressed: root.closeSearch()
                    Keys.onUpPressed: root.moveHighlight(-1)
                    Keys.onDownPressed: root.moveHighlight(1)
                    Keys.onReturnPressed: root.pickHighlighted()
                    Keys.onEnterPressed: root.pickHighlighted()

                    Icon {
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        path: Icons.search
                        size: Theme.controls.iconSize
                        color: Theme.controls.iconColor
                    }
                    Icon {
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        path: Icons.close
                        size: Theme.controls.iconSize
                        color: searchCloseMouse.containsMouse ? Theme.controls.inputText : Theme.controls.iconColor

                        MouseArea {
                            id: searchCloseMouse
                            anchors.fill: parent
                            anchors.margins: -6
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (!root.noGame) {
                                    root.closeSearch()
                                    return
                                }
                                root.searchText = ""
                                root.searchSoon()
                                searchField.forceActiveFocus()
                            }
                        }
                    }
                }
                Label {
                    visible: !root.searching && root.kind !== "steam"
                    text: qsTr("Style")
                    color: Theme.gameEditor.labelColor
                }
                SettingsComboBox {
                    id: styleBox
                    visible: !root.searching && root.kind !== "steam"
                    readonly property var styles: [{ id: "", name: qsTr("All styles") }].concat(Metadata.styles(root.kind))
                    Layout.preferredWidth: 170
                    model: styles.map(s => s.name)
                    currentIndex: Math.max(0, styles.findIndex(s => s.id === (root.tab.style ?? "")))
                    onActivated: index => {
                        root.setTab(root.kind, { style: styles[index].id })
                        root.load(root.kind, true)
                    }
                }
                SettingsButton {
                    visible: !root.searching && root.kind !== "steam"
                    text: qsTr("Other game")
                    tip: qsTr("Browse art from a different SteamGridDB game, such as a remaster")
                    onClicked: root.openSearch()
                }
            }

            Surface {
                id: results
                visible: root.searching
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.gameEditor.cardRadius
                fill: Theme.gameEditor.cardBackground
                fillEnd: Theme.gameEditor.cardBackgroundEnd
                direction: Theme.gameEditor.gradientDirection
                border.color: Theme.gameEditor.cardBorder
                border.width: Theme.gameEditor.cardBorderWidth

                ListView {
                    id: resultList
                    anchors.fill: parent
                    anchors.margins: Theme.gameEditor.cardBorderWidth
                    clip: true
                    model: root.searchResults
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: SettingsScrollBar {}

                    delegate: Item {
                        id: resultRow
                        required property var modelData
                        required property int index
                        readonly property bool highlighted: root.searchHighlight === index
                        width: ListView.view.width
                        height: Theme.settingsWindow.toolbarHeight

                        Rectangle {
                            readonly property real corner: Math.max(0, Theme.gameEditor.cardRadius - Theme.gameEditor.cardBorderWidth)
                            anchors.fill: parent
                            topLeftRadius: resultRow.index === 0 ? corner : 0
                            topRightRadius: topLeftRadius
                            bottomLeftRadius: resultRow.index === root.searchResults.length - 1 && resultList.contentHeight >= resultList.height ? corner : 0
                            bottomRightRadius: bottomLeftRadius
                            color: resultRow.highlighted ? Theme.controls.hover : "transparent"
                        }
                        Rectangle {
                            visible: resultRow.index < root.searchResults.length - 1
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: Theme.gameEditor.cardBorderWidth
                            color: Theme.gameEditor.cardBorder
                        }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.layout.spacing
                            anchors.rightMargin: Theme.layout.spacing
                            spacing: Theme.layout.spacing

                            Label {
                                Layout.fillWidth: true
                                text: resultRow.modelData.name
                                color: Theme.controls.buttonText
                                elide: Text.ElideRight
                            }
                            Rectangle {
                                visible: resultRow.modelData.steamgriddb === root.ownId
                                Layout.rightMargin: Theme.layout.spacing
                                implicitWidth: thisGameLabel.implicitWidth + 2 * Theme.layout.spacing
                                implicitHeight: thisGameLabel.implicitHeight + Theme.layout.spacing
                                radius: height / 2
                                color: Qt.alpha(Theme.gameEditor.selectedColor, Theme.gameEditor.bulkBadgeOpacity)

                                Label {
                                    id: thisGameLabel
                                    anchors.centerIn: parent
                                    text: qsTr("This game")
                                    color: Qt.lighter(Theme.gameEditor.selectedColor, Theme.segmented.lightBrightness)
                                    font.pixelSize: Theme.gameEditor.hintFontSize
                                }
                            }
                            Label {
                                text: resultRow.modelData.year > 0 ? resultRow.modelData.year : qsTr("–")
                                color: Theme.gameEditor.hintColor
                                font.weight: Theme.font.emphasisWeight
                            }
                            Icon {
                                path: Icons.chevronRight
                                size: Theme.controls.iconSize
                                color: Theme.controls.iconColor
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onEntered: root.searchHighlight = resultRow.index
                            onClicked: root.pickGame(resultRow.modelData)
                        }
                    }
                }

                Icon {
                    id: searchSpinner
                    readonly property bool waiting: root.searchRequest !== 0 || searchTimer.running
                    anchors.centerIn: parent
                    visible: waiting && root.searchText.trim() !== ""
                    path: Icons.loader
                    size: Theme.controls.iconSize * 1.5
                    color: Theme.gameEditor.hintColor
                    RotationAnimation on rotation {
                        running: searchSpinner.visible
                        from: 0
                        to: 360
                        duration: Theme.iconButton.spinDuration
                        loops: Animation.Infinite
                    }
                }
                Label {
                    anchors.centerIn: parent
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    visible: !searchSpinner.visible && root.searchResults.length === 0
                    text: root.searchError !== "" ? root.searchError
                        : root.searchText.trim() === "" ? qsTr("Search SteamGridDB for a game whose art you want")
                        : qsTr("No games match \"%1\"").arg(root.searchedText)
                    color: root.searchError !== "" ? Theme.gameEditor.errorColor : Theme.gameEditor.hintColor
                }
            }

            GridView {
                id: grid
                visible: !root.searching && root.kind !== "steam"

                readonly property real thumbHeight: root.thumbHeight(root.kind)
                readonly property real thumbWidth: root.thumbWidth(root.kind)

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

                    Checker {
                        anchors.fill: parent
                        visible: root.kind === "logo" || root.kind === "icon"
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
                            radius: height / 2
                            color: Theme.gameEditor.selectedColor
                            Icon {
                                anchors.centerIn: parent
                                path: Icons.check
                                size: 16
                                color: Theme.controls.buttonPrimaryText
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
                                                 style: thumb.model.style, author: thumb.model.author }, root.kind)
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

            Item {
                id: steamView
                visible: !root.searching && root.kind === "steam"
                Layout.fillWidth: true
                Layout.fillHeight: true

                Flow {
                    width: parent.width
                    spacing: Theme.gameEditor.browserSpacing

                    Repeater {
                        model: root.shownSteamImages

                        ColumnLayout {
                            id: steamThumb
                            required property var modelData
                            readonly property bool picked: root.picks[modelData.kind]?.id === modelData.url
                            readonly property bool checkered: modelData.kind === "logo"
                            spacing: Theme.layout.spacing / 2

                            Item {
                                implicitWidth: root.thumbWidth(steamThumb.modelData.kind)
                                implicitHeight: root.thumbHeight(steamThumb.modelData.kind)

                                Checker {
                                    anchors.fill: parent
                                    visible: steamThumb.checkered
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    visible: !steamThumb.checkered
                                    color: Theme.gameEditor.artPreviewBackground
                                }
                                FadeImage {
                                    anchors.fill: parent
                                    anchors.margins: steamThumb.checkered ? 6 : 0
                                    source: steamThumb.modelData.thumbnail
                                    sourceSize.width: width * 2
                                    fillMode: steamThumb.checkered ? Image.PreserveAspectFit : Image.PreserveAspectCrop
                                    onStatusChanged: if (status === Image.Error && steamThumb.modelData.kind === "logo")
                                        root.dropSteamImage(steamThumb.modelData.url)
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    color: "transparent"
                                    border.color: Theme.gameEditor.selectedColor
                                    border.width: Theme.gameEditor.selectedWidth
                                    opacity: steamThumb.picked ? 1 : 0
                                    visible: opacity > 0
                                    Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                                }
                                MarkPop {
                                    shown: steamThumb.picked
                                    anchors.top: parent.top
                                    anchors.right: parent.right
                                    anchors.margins: 6
                                    width: 22
                                    height: 22

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: height / 2
                                        color: Theme.gameEditor.selectedColor
                                        Icon {
                                            anchors.centerIn: parent
                                            path: Icons.check
                                            size: 16
                                            color: Theme.controls.buttonPrimaryText
                                        }
                                    }
                                }
                                MouseArea {
                                    id: steamMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.toggle({ id: steamThumb.modelData.url, url: steamThumb.modelData.url,
                                                             thumbnail: steamThumb.modelData.thumbnail,
                                                             width: steamThumb.modelData.width, height: steamThumb.modelData.height,
                                                             style: "", author: "", steam: true }, steamThumb.modelData.kind)
                                }
                                ToolTip.visible: steamMouse.containsMouse && steamThumb.modelData.width > 0
                                ToolTip.delay: Theme.tooltip.delay
                                ToolTip.text: qsTr("%1 × %2").arg(steamThumb.modelData.width).arg(steamThumb.modelData.height)
                            }
                            Label {
                                text: root.kinds.find(k => k.id === steamThumb.modelData.kind)?.name ?? ""
                                color: Theme.gameEditor.hintColor
                                font.pixelSize: Theme.gameEditor.hintFontSize
                            }
                        }
                    }
                }

                Icon {
                    id: steamSpinner
                    anchors.centerIn: parent
                    visible: root.steamLoading
                    path: Icons.loader
                    size: Theme.controls.iconSize * 1.5
                    color: Theme.gameEditor.hintColor
                    RotationAnimation on rotation {
                        running: steamSpinner.visible
                        from: 0
                        to: 360
                        duration: Theme.iconButton.spinDuration
                        loops: Animation.Infinite
                    }
                }
                Label {
                    anchors.centerIn: parent
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    visible: !root.steamLoading && (root.steamError !== "" || (root.steamLoaded && root.shownSteamImages.length === 0))
                    text: root.steamError !== "" ? root.steamError
                        : root.steamApp <= 0 ? qsTr("Steam has no game called \"%1\"").arg(root.ownTitle)
                        : qsTr("Steam has no library art for this game")
                    color: root.steamError !== "" ? Theme.gameEditor.errorColor : Theme.gameEditor.hintColor
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
