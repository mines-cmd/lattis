import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
ApplicationWindow {
    id: root

    objectName: "bulkLookupWindow"
    title: qsTr("Look up missing info")
    width: WindowState.width("bulk_lookup", 1040)
    height: WindowState.height("bulk_lookup", 780)
    minimumWidth: 760
    minimumHeight: 420
    visibility: WindowState.maximized("bulk_lookup") ? Window.Maximized : Window.Windowed
    Component.onCompleted: {
        WindowState.track(this, "bulk_lookup")
    }
    color: Theme.settingsWindow.background
    flags: Qt.Window | Qt.FramelessWindowHint

    font.pixelSize: Theme.font.size
    font.family: Theme.font.family !== "" ? Theme.font.family : Qt.application.font.family
    // Like the main window's: a window of its own doesn't inherit it, and Basic's tooltips are white.
    palette.toolTipBase: Theme.tooltip.background
    palette.toolTipText: Theme.tooltip.text
    palette.dark: Theme.tooltip.border

    property bool reviewing: false
    onReviewingChanged: if (reviewing) tagsPopup.close()
    property bool closeAnyway: false
    readonly property int scrollGap: Theme.gameEditor.bulkScrollbarWidth + Theme.layout.spacing / 2
    property var returnTo: null

    function show() {
        Metadata.reloadKeys()
        if (!lookupModel.running && !lookupModel.applying && lookupModel.resultModel.count === 0) {
            lookupModel.reload()
            reviewing = false
        }
        visible = true
        raise()
        requestActivate()
    }

    function openTags(search) {
        tagsPopup.openFor(tagsButton, search)
    }

    function filterResults(state) {
        lookupModel.resultFilter = state === "all" ? "" : state
    }

    property var collapsed: ({})
    function toggleCollapsed(id) {
        const next = Object.assign({}, collapsed)
        if (next[id])
            delete next[id]
        else
            next[id] = true
        collapsed = next
    }

    readonly property bool allCollapsed: {
        const open = lookupModel.results.filter(r => r.state !== "running" && !collapsed[r.id])
        return lookupModel.results.length > 0 && open.length === 0
    }
    function setAllCollapsed(value) {
        const next = {}
        if (value) {
            for (const r of lookupModel.results)
                next[r.id] = true
        }
        collapsed = next
    }

    function setReplaceExisting(on) {
        lookupModel.replaceExisting = on
    }

    function lookUp() {
        collapsed = ({})
        lookupModel.start()
        reviewing = true
    }

    function suggestTagsForEveryGame() {
        show()
        if (lookupModel.running || lookupModel.applying || lookupModel.resultModel.count > 0)
            return
        search.text = ""
        lookupModel.clearFilters()
        lookupModel.includeComplete = true
        lookupModel.suggestTags = true
        lookupModel.setAllChecked(true)
    }

    onClosing: close => {
        if (!closeAnyway && lookupModel.resultModel.count > 0 && !lookupModel.applying) {
            close.accepted = false
            confirmDiscard.open()
            return
        }
        closeAnyway = false
        lookupModel.clearResults()
        lookupModel.includeComplete = false
        reviewing = false
        if (returnTo !== null && returnTo.visible) {
            returnTo.raise()
            returnTo.requestActivate()
        }
    }

    BulkLookupModel {
        id: lookupModel
        onApplied: written => {
            appliedLabel.text = written === 1 ? qsTr("1 game updated.") : qsTr("%1 games updated.").arg(written)
            if (resultModel.count === 0)
                root.close()
        }
    }

    // The width of a column of the game list: `parts` of a row's width after the checkbox and the
    // icon, which share 6.6 (title 3, platform 1.2, what's missing 2.4). Header and rows alike.
    function gameColumn(parts) {
        const row = gameList.width - scrollGap - 2 * Theme.gameEditor.bulkRowPadding
        const free = row - Theme.gameEditor.checkSize - Theme.gameEditor.bulkIconSize
                     - 4 * Theme.gameEditor.bulkRowPadding
        return Math.max(0, Math.floor(free * parts / 6.6))
    }

    function missingChips(keys) {
        const fields = { description: qsTr("Description"), year: qsTr("Year"), rating: qsTr("Rating") }
        const kinds = { cover: qsTr("Cover"), hero: qsTr("Hero"), logo: qsTr("Logo"), icon: qsTr("Icon") }
        const chips = keys.filter(k => fields[k] !== undefined).map(k => ({ text: fields[k], category: k }))
        const art = keys.filter(k => kinds[k] !== undefined).map(k => kinds[k])
        if (art.length > 0)
            chips.push({ text: qsTr("Art: %1").arg(art.join(", ")), category: "art" })
        return chips
    }

    function artSteps() {
        return lookupModel.results.filter(r => r.state === "matched" && r.art.some(a => !a.none)).map(r => {
            const picks = {}
            for (const a of r.art) {
                if (a.checked && a.image)
                    picks[a.kind] = a.image
            }
            return { id: r.id, title: r.title, steamGridDbId: lookupModel.steamGridDbId(r.id),
                     kinds: r.art.filter(a => !a.none).map(a => a.kind), picks: picks }
        })
    }
    function browseArt(id, kind) {
        const steps = artSteps()
        const index = Math.max(0, steps.findIndex(s => s.id === id))
        if (steps.length === 0)
            return
        artBrowser.active = true
        artBrowser.item.openSteps(steps, index, kind)
    }

    Loader {
        id: artBrowser
        active: false
        sourceComponent: ArtBrowser {
            onStepPicked: (id, kind, image) => lookupModel.setArt(id, kind, image)
        }
    }

    property string detailedId
    function openDetailedLookup(r) {
        detailedId = r.id
        detailedLookup.active = true
        detailedLookup.item.openFor(r.title, r.igdbPlatforms, r.searched, true)
    }

    Loader {
        id: detailedLookup
        active: false
        sourceComponent: DetailedLookup {
            onChosen: (candidate, searched) => lookupModel.choose(root.detailedId, candidate, searched)
        }
    }

    component Check: Rectangle {
        id: check
        property bool checked
        property bool partial
        signal toggled()
        implicitWidth: Theme.gameEditor.checkSize
        implicitHeight: Theme.gameEditor.checkSize
        radius: Theme.settingsWindow.inputRadius / 2
        color: checked || partial ? Theme.gameEditor.checkOn : Theme.settingsWindow.inputBackground
        border.color: Theme.settingsWindow.inputBorder
        border.width: !checked && !partial && checkMouse.containsMouse ? Theme.settingsWindow.inputBorderWidth : 0

        Behavior on color { enabled: Motion.fades; MotionColor {} }

        BoxBump { value: check.checked }
        MarkPop {
            anchors.fill: parent
            shown: check.checked
            hiddenScale: 0
            overshoot: Theme.motion.markOvershoot * 1.3
            Icon {
                anchors.centerIn: parent
                path: Icons.check
                size: parent.width - 4
                color: Theme.gameEditor.checkMark
            }
        }
        MarkPop {
            anchors.fill: parent
            shown: check.partial && !check.checked
            Rectangle {
                anchors.centerIn: parent
                width: parent.width / 2
                height: 2
                radius: 1
                color: Theme.gameEditor.checkMark
            }
        }
        MouseArea {
            id: checkMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: check.toggled()
        }
    }

    component Muted: Label {
        color: Theme.gameEditor.hintColor
        font.pixelSize: Theme.gameEditor.hintFontSize
    }

    component SideHeading: RowLayout {
        property alias text: headingLabel.text
        property string note
        Layout.fillWidth: true
        Layout.leftMargin: Theme.layout.spacing
        Layout.rightMargin: Theme.layout.spacing
        Layout.topMargin: Theme.layout.spacing / 2
        Layout.bottomMargin: Theme.layout.spacing * 0.75
        Muted {
            id: headingLabel
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.8
        }
        Item { Layout.fillWidth: true }
        Muted { visible: note !== ""; text: note }
    }

    BulkTagsPopup {
        id: tagsPopup
        lookup: lookupModel
    }

    readonly property int barButtonHeight: Theme.settingsWindow.buttonHeight
    readonly property int detailIndent: Theme.gameEditor.bulkIconSize + Theme.gameEditor.bulkRowPadding

    function stateColor(state) {
        switch (state) {
        case "matched": return Theme.gameEditor.bulkStateMatched
        case "pick": return Theme.gameEditor.bulkStatePick
        case "notFound": return Theme.gameEditor.bulkStateNotFound
        case "error": return Theme.gameEditor.bulkStateError
        default: return Theme.gameEditor.bulkStateNotOnIgdb
        }
    }

    FontMetrics {
        id: sectionHeadingFont
        font.pixelSize: Theme.gameEditor.hintFontSize
        font.weight: Font.DemiBold
    }
    readonly property int sectionHeadingGap: Theme.layout.spacing * 0.75

    component ChipText: Label {
        color: Theme.gameEditor.lookupText
        font.pixelSize: Theme.font.size - 1
    }

    component ResultChip: AbstractButton {
        id: chip
        hoverEnabled: true
        topPadding: Theme.layout.spacing
        bottomPadding: Theme.layout.spacing
        leftPadding: 2 * Theme.layout.spacing
        rightPadding: 2 * Theme.layout.spacing
        HoverHandler { cursorShape: chip.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
        scale: chip.pressed ? Theme.motion.pressScale : 1
        Behavior on scale { enabled: Motion.on; MotionNumber {} }

        background: Rectangle {
            radius: Theme.radius.small
            color: chip.hovered && chip.enabled ? Theme.settingsWindow.rowHover : "transparent"
        }
    }

    FontMetrics { id: chipFont; font.pixelSize: Theme.font.size - 1 }
    readonly property real artHeaderHeight: Math.max(chipFont.height, Theme.gameEditor.checkSize) + 2 * Theme.layout.spacing

    component ArtCell: ColumnLayout {
        id: cell
        required property string kind
        required property string gameId
        property var entry
        property real previewHeight
        readonly property real aspect: ({ cover: 2 / 3, hero: 3, logo: 2.5, icon: 1 })[kind]
        readonly property bool none: entry?.none ?? false
        readonly property bool found: entry !== undefined && !none
        readonly property string kindName: ({ cover: qsTr("Cover"), hero: qsTr("Hero"), logo: qsTr("Logo"), icon: qsTr("Icon") })[kind]
        spacing: Theme.layout.spacing

        AbstractButton {
            id: heading
            Layout.preferredHeight: root.artHeaderHeight
            enabled: cell.found
            hoverEnabled: true
            // The hover's room around the checkbox, without moving the checkbox off the picture's edge.
            Layout.leftMargin: -Theme.layout.spacing
            leftPadding: Theme.layout.spacing
            rightPadding: Theme.layout.spacing * 1.5
            onClicked: lookupModel.setArtChecked(cell.gameId, cell.kind, !cell.entry.checked)
            HoverHandler { cursorShape: heading.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
            background: Rectangle {
                radius: Theme.radius.tiny
                color: heading.hovered && heading.enabled ? Theme.settingsWindow.rowHover : "transparent"
            }
            contentItem: RowLayout {
                spacing: Theme.layout.spacing
                Check {
                    visible: cell.found
                    checked: cell.entry?.checked ?? false
                    onToggled: heading.clicked()
                }
                ChipText {
                    text: cell.kindName
                    color: cell.found ? Theme.gameEditor.lookupText : Theme.gameEditor.hintColor
                }
                ChipText {
                    visible: cell.found && cell.entry.changed
                    text: qsTr("changed")
                    color: Theme.gameEditor.changedColor
                    font.pixelSize: Theme.gameEditor.hintFontSize
                }
            }
        }
        Item {
            Layout.preferredWidth: Math.round(cell.previewHeight * cell.aspect)
            Layout.preferredHeight: cell.previewHeight
            Rectangle {
                anchors.fill: parent
                visible: !cell.found && !cell.none
                radius: Theme.radius.small
                color: Theme.gameEditor.artPreviewBackground
                opacity: 0.5
            }
            ArtThumb {
                anchors.fill: parent
                visible: cell.found
                cornerRadius: Theme.radius.small
                // Logos and icons are cut out on transparency; cropping would lose their edges.
                fillMode: cell.kind === "logo" || cell.kind === "icon" ? Image.PreserveAspectFit : Image.PreserveAspectCrop
                source: cell.entry?.image?.thumbnail ?? ""
                opacity: cell.entry?.checked ? 1 : 0.35
            }
            Muted {
                anchors.centerIn: parent
                visible: !cell.found
                text: cell.none ? qsTr("None found") : qsTr("Yours")
            }
            Rectangle {
                anchors.fill: parent
                opacity: cell.found && pickArea.containsMouse ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                radius: Theme.radius.small
                color: Theme.gameEditor.artHoverOverlay
                Icon { anchors.centerIn: parent; path: Icons.folderImageFill; size: Theme.settingsWindow.iconSize * 1.5; color: Theme.gameEditor.artHoverIcon }
            }
            MouseArea {
                id: pickArea
                anchors.fill: parent
                enabled: cell.found
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.browseArt(cell.gameId, cell.kind)
                ToolTip.visible: containsMouse
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: qsTr("Choose another %1").arg(cell.kindName.toLowerCase())
            }
        }
    }

    component SectionCard: ColumnLayout {
        id: section
        property string label
        property int padding: Theme.layout.spacing / 2
        default property alias content: sectionBody.data
        Layout.fillWidth: true
        spacing: root.sectionHeadingGap
        Muted {
            Layout.leftMargin: 2
            text: section.label
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.8
        }
        Surface {
            readonly property int inset: section.padding + Theme.gameEditor.bulkCardBorderWidth
            Layout.fillWidth: true
            implicitHeight: sectionBody.implicitHeight + 2 * inset
            radius: Theme.radius.medium
            fill: Theme.gameEditor.cardBackground
            fillEnd: Theme.gameEditor.cardBackgroundEnd
            direction: Theme.gameEditor.gradientDirection
            border.color: Theme.gameEditor.cardBorder
            border.width: Theme.gameEditor.bulkCardBorderWidth
            ColumnLayout {
                id: sectionBody
                x: parent.inset
                y: parent.inset
                width: parent.width - 2 * parent.inset
            }
        }
    }

    function categoryTally(ids) {
        let total = 0
        let on = 0
        for (const id of ids) {
            const c = lookupModel.categories[id]
            total += c?.total ?? 0
            on += c?.on ?? 0
        }
        return { total: total, on: on }
    }
    function setCategories(ids, checked) {
        for (const id of ids) {
            if ((lookupModel.categories[id]?.total ?? 0) > 0)
                lookupModel.setCategoryChecked(id, checked)
        }
    }

    component CategoryGroup: ColumnLayout {
        id: group
        property string title
        property string tip
        property var items: []
        readonly property var ids: items.map(i => i.id)
        readonly property var tally: root.categoryTally(ids)
        visible: tally.total > 0
        Layout.fillWidth: true
        spacing: 2

        BulkOption {
            Layout.fillWidth: true
            font.weight: Font.DemiBold
            text: group.title
            checked: group.tally.on === group.tally.total
            partial: group.tally.on > 0 && !checked
            enabled: !lookupModel.applying
            onClicked: root.setCategories(group.ids, !checked)
            ToolTip.visible: hovered
            ToolTip.delay: Theme.tooltip.delay
            ToolTip.text: group.tip
        }
        Repeater {
            model: group.items
            BulkOption {
                required property var modelData
                readonly property var tally: root.categoryTally([modelData.id])
                visible: tally.total > 0
                Layout.fillWidth: true
                Layout.leftMargin: Theme.layout.spacing * 2
                text: modelData.name
                checked: tally.on === tally.total
                partial: tally.on > 0 && !checked
                count: String(tally.total)
                enabled: !lookupModel.applying
                onClicked: root.setCategories([modelData.id], !checked)
                ToolTip.visible: hovered
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: modelData.tip
            }
        }
    }

    component Badge: Rectangle {
        property string kind
        property alias text: badgeLabel.text
        implicitWidth: badgeRow.implicitWidth + 2 * Theme.layout.spacing
        implicitHeight: badgeRow.implicitHeight + Theme.layout.spacing
        radius: height / 2
        color: Qt.alpha(root.stateColor(kind), kind === "running" ? 0 : Theme.gameEditor.bulkBadgeOpacity)
        RowLayout {
            id: badgeRow
            anchors.centerIn: parent
            spacing: Theme.layout.spacing
            Icon {
                visible: parent.parent.kind === "running"
                path: Icons.loader
                size: Theme.font.size - 1
                color: Theme.gameEditor.hintColor
                RotationAnimation on rotation {
                    running: parent.visible
                    from: 0
                    to: 360
                    duration: Theme.iconButton.spinDuration
                    loops: Animation.Infinite
                }
            }
            Label {
                id: badgeLabel
                color: parent.parent.kind === "running" ? Theme.gameEditor.hintColor
                     : Qt.lighter(root.stateColor(parent.parent.kind), Theme.segmented.lightBrightness)
                font.pixelSize: Theme.gameEditor.hintFontSize
            }
        }
    }

    MotionPopup {
        id: confirmDiscard
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Theme.gameEditor.confirmWidth
        modal: true
        padding: Theme.gameEditor.padding
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
                text: qsTr("Discard the results?")
                color: Theme.gameEditor.headingColor
                font.weight: Font.DemiBold
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.gameEditor.labelColor
                text: qsTr("Nothing has been written yet. Closing drops what was found.")
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing
                Item { Layout.fillWidth: true }
                SettingsButton { text: qsTr("Keep reviewing"); onClicked: confirmDiscard.close() }
                SettingsButton {
                    primary: true
                    text: qsTr("Discard")
                    onClicked: {
                        confirmDiscard.close()
                        root.closeAnyway = true
                        root.close()
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.settingsWindow.contentPadding
        anchors.topMargin: Theme.settingsWindow.contentPadding
        spacing: Theme.gameEditor.rowSpacing

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: windowControls.height
            Layout.bottomMargin: Theme.layout.spacing / 4
            Layout.rightMargin: windowControls.width + Theme.layout.spacing
            spacing: Theme.layout.spacing

            Label {
                Layout.fillWidth: true
                text: root.reviewing ? qsTr("Review what was found") : qsTr("Look up missing info")
                color: Theme.gameEditor.headingColor
                font.weight: Font.DemiBold
                font.pixelSize: Theme.gameEditor.headingFontSize
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ScreenSwap {
                shown: !root.reviewing
                side: -1
                spacing: Theme.gameEditor.rowSpacing

                Label {
                    visible: !Metadata.hasIgdbKeys
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.gameEditor.warningColor
                    text: qsTr("Add your IGDB keys in Settings → Metadata to look games up.")
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.gameEditor.columnSpacing / 2

                    ColumnLayout {
                        Layout.preferredWidth: Theme.gameEditor.bulkSidebarWidth
                        Layout.minimumWidth: Theme.gameEditor.bulkSidebarWidth
                        Layout.maximumWidth: Theme.gameEditor.bulkSidebarWidth
                        Layout.fillHeight: true
                        spacing: Theme.layout.spacing

                        Flickable {
                            id: sidebar
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            contentHeight: sideColumn.implicitHeight
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: SettingsScrollBar { thickness: Theme.gameEditor.bulkScrollbarWidth; trackless: true }

                            ColumnLayout {
                                id: sideColumn
                                width: sidebar.width - scrollGap
                                spacing: Theme.gameEditor.rowSpacing

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    SideHeading { text: qsTr("Show"); Layout.topMargin: 0 }
                                    SegmentedToggle {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: Theme.layout.spacing / 2
                                        Layout.rightMargin: Theme.layout.spacing / 2
                                        accessibleName: qsTr("Show")
                                        choices: [{ id: "missing", text: qsTr("Missing info") },
                                                  { id: "all", text: qsTr("All games") }]
                                        current: lookupModel.includeComplete ? "all" : "missing"
                                        onPicked: id => lookupModel.includeComplete = id === "all"
                                    }
                                    BulkOption {
                                        readonly property int games: lookupModel.notOnIgdbCount
                                        Layout.fillWidth: true
                                        Layout.topMargin: Theme.layout.spacing / 2
                                        text: qsTr("Games not on IGDB")
                                        checked: lookupModel.includeNotOnIgdb
                                        count: String(games)
                                        dimmed: games === 0
                                        onClicked: lookupModel.includeNotOnIgdb = !checked
                                    }
                                }

                                ColumnLayout {
                                    visible: !lookupModel.includeComplete
                                    Layout.fillWidth: true
                                    spacing: 2

                                    SideHeading {
                                        text: lookupModel.replaceExisting ? qsTr("Look for") : qsTr("Missing")
                                        note: lookupModel.replaceExisting ? qsTr("set or not") : ""
                                    }
                                    Repeater {
                                        model: [{ id: "description", name: qsTr("Description") }, { id: "year", name: qsTr("Year") },
                                                { id: "rating", name: qsTr("Rating") }, { id: "art", name: qsTr("Art") }]
                                        BulkOption {
                                            required property var modelData
                                            readonly property int games: lookupModel.missingCounts[modelData.id] ?? 0
                                            readonly property bool uncounted: lookupModel.replaceExisting && modelData.id !== "art"
                                            Layout.fillWidth: true
                                            text: modelData.name
                                            checked: lookupModel.missing.includes(modelData.id)
                                            count: uncounted ? "" : String(games)
                                            dimmed: !uncounted && games === 0
                                            onClicked: lookupModel.missing = checked ? lookupModel.missing.filter(m => m !== modelData.id)
                                                                                     : lookupModel.missing.concat([modelData.id])
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    SideHeading { text: qsTr("Platform") }
                                    BulkOption {
                                        Layout.fillWidth: true
                                        radio: true
                                        text: qsTr("All platforms")
                                        checked: lookupModel.platform === ""
                                        selected: checked
                                        count: String(lookupModel.platforms.reduce((sum, p) => sum + p.count, 0))
                                        onClicked: lookupModel.platform = ""
                                    }
                                    Repeater {
                                        model: lookupModel.platforms
                                        BulkOption {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            radio: true
                                            text: modelData.name
                                            checked: lookupModel.platform === modelData.id
                                            selected: checked
                                            count: String(modelData.count)
                                            dimmed: modelData.count === 0
                                            onClicked: lookupModel.platform = modelData.id
                                        }
                                    }
                                }

                                ColumnLayout {
                                    visible: lookupModel.allTags.length > 0
                                    Layout.fillWidth: true
                                    spacing: 2

                                    SideHeading {
                                        text: qsTr("Tags")
                                        note: lookupModel.tags.length > 0 ? qsTr("any of %1").arg(lookupModel.tags.length) : ""
                                    }
                                    AbstractButton {
                                        id: tagsButton
                                        Layout.fillWidth: true
                                        Layout.leftMargin: Theme.layout.spacing / 2
                                        Layout.rightMargin: Theme.layout.spacing / 2
                                        Layout.bottomMargin: Theme.layout.spacing / 2
                                        hoverEnabled: true
                                        topPadding: Theme.layout.spacing * 0.75 + Theme.settingsWindow.inputBorderWidth
                                        bottomPadding: topPadding
                                        leftPadding: Theme.layout.spacing
                                        rightPadding: Theme.layout.spacing
                                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                                        onClicked: tagsPopup.openFor(tagsButton, "")
                                        property real dip: pressed ? Theme.motion.pressScale : 1
                                        Behavior on dip { enabled: Motion.on; MotionNumber {} }
                                        Binding { target: tagsButton.background; property: "scale"; value: tagsButton.dip }
                                        Binding { target: tagsButton.contentItem; property: "scale"; value: tagsButton.dip }

                                        background: Rectangle {
                                            radius: Theme.settingsWindow.inputRadius
                                            color: tagsButton.hovered ? Theme.settingsWindow.buttonHoverBackground : Theme.settingsWindow.buttonBackground
                                            border.color: Theme.settingsWindow.inputBorderFocus
                                            border.width: tagsPopup.opened || tagsButton.visualFocus ? Theme.settingsWindow.inputBorderWidth : 0
                                        }
                                        contentItem: RowLayout {
                                            spacing: Theme.layout.spacing
                                            Icon {
                                                path: Icons.tagLine
                                                size: Theme.settingsWindow.iconSize
                                                color: tagsButton.hovered ? Theme.gameEditor.lookupText : Theme.gameEditor.hintColor
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                text: lookupModel.tags.length > 0 ? qsTr("Change tags…") : qsTr("Filter by tag…")
                                                color: tagsButton.hovered ? Theme.gameEditor.lookupText : Theme.gameEditor.hintColor
                                                elide: Text.ElideRight
                                            }
                                            Muted { text: String(lookupModel.allTags.length) }
                                        }
                                    }
                                    Repeater {
                                        model: lookupModel.tags
                                        BulkOption {
                                            required property string modelData
                                            readonly property int games: lookupModel.tagCounts[modelData] ?? 0
                                            Layout.fillWidth: true
                                            text: modelData
                                            checked: true
                                            swatch: tagsPopup.colorOf(modelData)
                                            count: String(games)
                                            dimmed: games === 0
                                            onClicked: tagsPopup.toggle(modelData)
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 1
                            color: Theme.settingsWindow.inputBorder
                        }
                        TextLink {
                            text: qsTr("Clear filters")
                            onClicked: {
                                search.text = ""
                                lookupModel.clearFilters()
                            }
                        }
                    }

                    ColumnLayout {
                        // The columns' widths come from this one's, so it can't take its own from them.
                        Layout.preferredWidth: 1
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: Theme.layout.spacing

                        SettingsSearchField {
                            id: search
                            Layout.fillWidth: true
                            Layout.bottomMargin: Theme.layout.spacing
                            Layout.rightMargin: scrollGap
                            placeholderText: qsTr("Search titles")
                            onTextChanged: lookupModel.search = text
                        }

                        ColumnLayout {
                            visible: gameList.count > 0
                            Layout.fillWidth: true
                            Layout.rightMargin: scrollGap
                            spacing: Theme.layout.spacing / 2

                            RowLayout {
                                id: header
                                Layout.fillWidth: true
                                Layout.leftMargin: Theme.gameEditor.bulkRowPadding
                                Layout.rightMargin: Theme.gameEditor.bulkRowPadding
                                spacing: Theme.gameEditor.bulkRowPadding

                                Check {
                                    readonly property bool all: lookupModel.checkedCount === lookupModel.gameModel.count
                                    checked: lookupModel.checkedCount > 0 && all
                                    partial: lookupModel.checkedCount > 0 && !all
                                    onToggled: lookupModel.setAllChecked(!checked)
                                }
                                Item { implicitWidth: Theme.gameEditor.bulkIconSize }
                                Muted {
                                    Layout.preferredWidth: root.gameColumn(3)
                                    text: qsTr("Game")
                                    elide: Text.ElideRight
                                }
                                Muted {
                                    Layout.preferredWidth: root.gameColumn(1.2)
                                    text: qsTr("Platform")
                                    elide: Text.ElideRight
                                }
                                Muted {
                                    Layout.fillWidth: true // what's left, so the header never widens the window
                                    text: qsTr("Missing")
                                    elide: Text.ElideRight
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 1
                                color: Theme.settingsWindow.inputBorder
                            }
                        }

                        ListView {
                            id: gameList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: lookupModel.gameModel
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: SettingsScrollBar { thickness: Theme.gameEditor.bulkScrollbarWidth; trackless: true }

                            delegate: Rectangle {
                                id: gameRow
                                required property var item
                                readonly property var modelData: item
                                width: gameList.width - scrollGap
                                implicitHeight: rowContent.implicitHeight + 2 * Theme.gameEditor.bulkRowPadding
                                radius: Theme.settingsWindow.rowRadius
                                color: rowHover.hovered ? Theme.settingsWindow.rowHover : "transparent"
                                HoverHandler { id: rowHover }
                                TapHandler { onTapped: lookupModel.setChecked(gameRow.modelData.id, !gameRow.modelData.checked) }

                                RowLayout {
                                    id: rowContent
                                    anchors.fill: parent
                                    anchors.margins: Theme.gameEditor.bulkRowPadding
                                    spacing: Theme.gameEditor.bulkRowPadding

                                    Check {
                                        checked: gameRow.modelData.checked
                                        onToggled: lookupModel.setChecked(gameRow.modelData.id, !checked)
                                    }
                                    GameIcon {
                                        size: Theme.gameEditor.bulkIconSize
                                        source: gameRow.modelData.icon
                                        title: gameRow.modelData.title
                                    }
                                    Label {
                                        Layout.preferredWidth: root.gameColumn(3)
                                        text: gameRow.modelData.title
                                        color: Theme.gameEditor.lookupText
                                        elide: Text.ElideRight
                                    }
                                    Muted {
                                        Layout.preferredWidth: root.gameColumn(1.2)
                                        text: gameRow.modelData.platformName
                                        elide: Text.ElideRight
                                    }
                                    Flow {
                                        id: chipFlow
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: root.gameColumn(2.4)
                                        spacing: Theme.layout.spacing / 2

                                        Repeater {
                                            model: root.missingChips(gameRow.modelData.missing)
                                            Rectangle {
                                                required property var modelData
                                                readonly property bool unwanted: !lookupModel.includeComplete
                                                                                 && !lookupModel.missing.includes(modelData.category)
                                                width: Math.min(implicitWidth, chipFlow.width)
                                                implicitWidth: chipLabel.implicitWidth + 2 * Theme.layout.spacing
                                                implicitHeight: chipLabel.implicitHeight + Theme.layout.spacing
                                                radius: Theme.radius.tiny
                                                color: "transparent"
                                                border.color: Theme.settingsWindow.inputBorder
                                                border.width: Theme.settingsWindow.inputBorderWidth
                                                opacity: unwanted ? Theme.gameEditor.readOnlyOpacity : 1
                                                Muted {
                                                    id: chipLabel
                                                    anchors.centerIn: parent
                                                    width: Math.min(implicitWidth, parent.width - 2 * Theme.layout.spacing)
                                                    text: parent.modelData.text
                                                    elide: Text.ElideRight
                                                    font.pixelSize: Theme.font.size - 1
                                                }
                                            }
                                        }
                                        Muted {
                                            visible: gameRow.modelData.missing.length === 0
                                            text: "—"
                                        }
                                    }
                                }
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: gameList.count === 0
                                color: Theme.gameEditor.hintColor
                                text: lookupModel.missingCount === 0 && !lookupModel.replaceExisting && !lookupModel.includeComplete ? qsTr("Every game has a description, a year, a rating and its art.")
                                                              : qsTr("No games match the filters.")
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: -Theme.settingsWindow.contentPadding
                    Layout.rightMargin: -Theme.settingsWindow.contentPadding
                    implicitHeight: 1
                    color: Theme.settingsWindow.inputBorder
                }

                RowLayout {
                    Layout.fillWidth: true
                    // As far below the line as above it, not a whole content padding.
                    Layout.bottomMargin: -(Theme.settingsWindow.contentPadding - Theme.gameEditor.rowSpacing)
                    spacing: Theme.layout.spacing

                    Muted {
                        text: qsTr("%1 of %2 checked").arg(lookupModel.checkedCount).arg(lookupModel.gameModel.count)
                    }
                    Label {
                        id: appliedLabel
                        Layout.fillWidth: true
                        color: Theme.gameEditor.hintColor
                        elide: Text.ElideRight
                    }
                    SettingsCheckBox {
                        bordered: false
                        text: qsTr("Replace existing values")
                        checked: lookupModel.replaceExisting
                        onToggled: lookupModel.replaceExisting = checked
                        ToolTip.visible: hovered
                        ToolTip.delay: Theme.tooltip.delay
                        ToolTip.text: qsTr("Also offer IGDB's description, year and rating where they differ from the game's own, unchecked")
                    }
                    SettingsCheckBox {
                        bordered: false
                        text: qsTr("Suggest tags")
                        checked: lookupModel.suggestTags
                        onToggled: lookupModel.suggestTags = checked
                        ToolTip.visible: hovered
                        ToolTip.delay: Theme.tooltip.delay
                        ToolTip.text: qsTr("Offer your tags that fit what IGDB says about each game")
                    }
                    Rectangle {
                        Layout.preferredWidth: 1
                        Layout.preferredHeight: Theme.settingsWindow.toolbarHeight / 2
                        color: Theme.settingsWindow.inputBorder
                    }
                    SettingsButton {
                        visible: lookupModel.resultModel.count > 0
                        implicitHeight: Theme.settingsWindow.buttonHeight
                        text: qsTr("Back to the results")
                        onClicked: root.reviewing = true
                    }
                    SettingsButton {
                        primary: true
                        implicitHeight: Theme.settingsWindow.buttonHeight
                        text: qsTr("Look up %1").arg(lookupModel.checkedCount)
                        enabled: lookupModel.checkedCount > 0 && Metadata.hasIgdbKeys
                        onClicked: root.lookUp()
                    }
                }
            }

            ScreenSwap {
                shown: root.reviewing
                side: 1
                spacing: Theme.gameEditor.rowSpacing

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.gameEditor.columnSpacing / 2

                    Flickable {
                        id: reviewSidebar
                        Layout.preferredWidth: Theme.gameEditor.bulkSidebarWidth
                        Layout.minimumWidth: Theme.gameEditor.bulkSidebarWidth
                        Layout.maximumWidth: Theme.gameEditor.bulkSidebarWidth
                        Layout.fillHeight: true
                        contentHeight: reviewColumn.implicitHeight
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: SettingsScrollBar { thickness: Theme.gameEditor.bulkScrollbarWidth; trackless: true }

                        ColumnLayout {
                            id: reviewColumn
                            width: reviewSidebar.width - scrollGap
                            spacing: Theme.gameEditor.rowSpacing

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: Theme.layout.spacing
                                Layout.rightMargin: Theme.layout.spacing
                                spacing: Theme.layout.spacing

                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: 6
                                    radius: height / 2
                                    color: Theme.gameEditor.bulkProgressTrack
                                    clip: true
                                    Surface {
                                        width: lookupModel.total > 0 ? parent.width * Math.min(1, lookupModel.done / lookupModel.total)
                                                                     : parent.width
                                        Behavior on width { enabled: Motion.on; MotionNumber {} }
                                        height: parent.height
                                        radius: height / 2
                                        fill: Theme.palette.accent
                                        fillEnd: Theme.palette.accentEnd
                                        direction: "right"
                                    }
                                }
                                Muted {
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                    font.features: { "tnum": 1 }
                                    text: lookupModel.applying ? qsTr("Saving %1 of %2").arg(Math.min(lookupModel.done + 1, lookupModel.total)).arg(lookupModel.total)
                                        : !lookupModel.running ? qsTr("%1 of %2 looked up").arg(lookupModel.done).arg(lookupModel.total)
                                        : lookupModel.secondsLeft < 0 ? qsTr("Looking up %1 of %2").arg(Math.min(lookupModel.done + 1, lookupModel.total)).arg(lookupModel.total)
                                        : qsTr("Looking up %1 of %2 · about %3 s left").arg(Math.min(lookupModel.done + 1, lookupModel.total)).arg(lookupModel.total)
                                                                                      .arg(lookupModel.secondsLeft)
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                SideHeading { text: qsTr("Show") }
                                Repeater {
                                    model: [{ id: "", count: "all", name: qsTr("All results") },
                                            { id: "matched", name: qsTr("Matched") },
                                            { id: "pick", name: qsTr("Needs a pick") },
                                            { id: "notFound", name: qsTr("Not found") },
                                            { id: "error", name: qsTr("Couldn't look up") },
                                            { id: "notOnIgdb", name: qsTr("Not on IGDB") }]
                                    BulkOption {
                                        required property var modelData
                                        readonly property int results: lookupModel.stateCounts[modelData.count ?? modelData.id] ?? 0
                                        Layout.fillWidth: true
                                        showBox: false
                                        showDot: true
                                        dot: modelData.id === "" ? "transparent" : root.stateColor(modelData.id)
                                        text: modelData.name
                                        checked: lookupModel.resultFilter === modelData.id
                                        selected: checked
                                        count: String(results)
                                        dimmed: results === 0
                                        onClicked: lookupModel.resultFilter = modelData.id
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                SideHeading { text: qsTr("For every match") }
                                CategoryGroup {
                                    title: qsTr("Replace existing")
                                    tip: qsTr("IGDB's values in place of the games' own")
                                    items: [{ id: "description", name: qsTr("Description"), tip: qsTr("Replace each game's description with IGDB's") },
                                            { id: "year", name: qsTr("Year"), tip: qsTr("Replace each game's year with IGDB's") },
                                            { id: "rating", name: qsTr("Rating"), tip: qsTr("Replace each game's rating with IGDB's") }]
                                }
                                CategoryGroup {
                                    title: qsTr("Add")
                                    tip: qsTr("What IGDB and SteamGridDB offer on top of the games' own")
                                    items: [{ id: "tags", name: qsTr("Tags"), tip: qsTr("Add the tags IGDB suggests") },
                                            { id: "art", name: qsTr("Art"), tip: qsTr("Use SteamGridDB's top image of each kind found") }]
                                }
                                Muted {
                                    visible: root.categoryTally(["description", "year", "rating", "tags", "art"]).total === 0
                                    Layout.fillWidth: true
                                    Layout.leftMargin: Theme.layout.spacing
                                    Layout.rightMargin: Theme.layout.spacing
                                    wrapMode: Text.WordWrap
                                    text: qsTr("Nothing offered to check.")
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Theme.layout.spacing

                                SideHeading { text: qsTr("View") }
                                SettingsButton {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: Theme.layout.spacing / 2
                                    Layout.rightMargin: Theme.layout.spacing / 2
                                    implicitHeight: root.barButtonHeight
                                    text: qsTr("Choose art for all")
                                    enabled: !lookupModel.applying && lookupModel.hasArt
                                    onClicked: root.browseArt("", "")
                                }
                                SettingsButton {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: Theme.layout.spacing / 2
                                    Layout.rightMargin: Theme.layout.spacing / 2
                                    implicitHeight: root.barButtonHeight
                                    enabled: lookupModel.results.length > 0
                                    text: root.allCollapsed ? qsTr("Expand all") : qsTr("Collapse all")
                                    tip: root.allCollapsed ? qsTr("Show the details of every result") : qsTr("Hide the details of every result, leaving one line each")
                                    onClicked: root.setAllCollapsed(!root.allCollapsed)
                                }
                            }
                        }
                    }

                    ListView {
                        id: resultList
                        Layout.preferredWidth: 1
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: lookupModel.resultModel
                        spacing: Theme.layout.spacing / 2
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: SettingsScrollBar { thickness: Theme.gameEditor.bulkScrollbarWidth; trackless: true }

                        delegate: Item {
                            id: result
                            required property var item
                            readonly property var r: item
                            // Kept by game, not here: a list's rows are made and dropped as it scrolls.
                            readonly property bool open: !root.collapsed[r.id]
                            readonly property bool hasTray: open && r.state === "matched"
                            readonly property int bw: Theme.gameEditor.bulkCardBorderWidth
                            width: resultList.width - scrollGap
                            property real trayProgress: hasTray ? 1 : 0
                            Behavior on trayProgress { enabled: Motion.on; MotionNumber { speed: "normal" } }
                            readonly property real trayShown: Math.max(0, Math.min(1, trayProgress))
                            implicitHeight: rowCard.height + trayClip.height

                            // Cuts off the tray's top edge, so the row's bottom edge is the only line between them:
                            // the outline is translucent, and two drawn over each other show brighter.
                            Item {
                                id: trayClip
                                visible: result.trayShown > 0
                                y: rowCard.height
                                width: parent.width
                                height: (tray.height - result.bw) * result.trayShown
                                opacity: result.trayShown
                                clip: true

                                Surface {
                                    id: tray
                                    y: -result.bw
                                    width: parent.width
                                    height: trayContent.implicitHeight + 2 * Theme.gameEditor.bulkRowPadding + result.bw * 2
                                    radius: Theme.gameEditor.cardRadius
                                    topLeftRadius: 0
                                    topRightRadius: 0
                                    fill: Qt.darker(Theme.gameEditor.bulkTrayBackground, Theme.gameEditor.bulkTrayDarkness)
                                    fillEnd: Qt.darker(Theme.gameEditor.bulkTrayBackgroundEnd, Theme.gameEditor.bulkTrayDarkness)
                                    direction: Theme.gameEditor.gradientDirection
                                    border.color: Theme.gameEditor.cardBorder
                                    border.width: result.bw

                                    RowLayout {
                                        id: trayContent
                                        x: Theme.gameEditor.bulkRowPadding + result.bw
                                        y: Theme.gameEditor.bulkRowPadding + result.bw * 2
                                        width: parent.width - 2 * x
                                        spacing: Theme.layout.spacing

                                        ColumnLayout {
                                            id: matchColumn
                                            readonly property int matchWidth: Math.min(Theme.gameEditor.bulkMatchCoverWidth,
                                                                                       Math.max(88, Math.round(trayContent.width * 0.24)))
                                            Layout.alignment: Qt.AlignTop
                                            // Fixed: the name wrapping inside would otherwise make it share the room with the sections.
                                            Layout.fillWidth: false
                                            Layout.preferredWidth: matchColumn.matchWidth
                                            Layout.maximumWidth: matchColumn.matchWidth
                                            Layout.leftMargin: Theme.gameEditor.bulkRowPadding * 1.5
                                            Layout.rightMargin: Theme.gameEditor.bulkRowPadding * 2.5
                                            Layout.topMargin: sectionHeadingFont.height + root.sectionHeadingGap
                                            spacing: Theme.layout.spacing / 2

                                            ArtThumb {
                                                Layout.bottomMargin: Theme.layout.spacing / 2
                                                implicitHeight: Math.round(matchColumn.matchWidth * 3 / 2)
                                                cornerRadius: Theme.radius.small
                                                source: result.r.match?.coverLarge ?? result.r.match?.cover ?? ""
                                            }
                                            Muted { text: qsTr("Matched"); font.pixelSize: Theme.font.size }
                                            Label {
                                                Layout.fillWidth: true
                                                text: result.r.match?.name ?? ""
                                                color: Theme.gameEditor.lookupText
                                                font.weight: Font.DemiBold
                                                font.pixelSize: Theme.font.size + 2
                                                wrapMode: Text.WordWrap
                                            }
                                            Muted {
                                                visible: (result.r.match?.year ?? 0) > 0
                                                text: String(result.r.match?.year ?? "")
                                                font.pixelSize: Theme.font.size
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.alignment: Qt.AlignTop
                                            spacing: Theme.layout.spacing * 1.5

                                            SectionCard {
                                                visible: result.r.fields.length > 0
                                                label: qsTr("Info")
                                                Flow {
                                                    Layout.fillWidth: true
                                                    spacing: 2
                                                    Repeater {
                                                        model: result.r.fields.length
                                                        ResultChip {
                                                            id: fieldChip
                                                            required property int index
                                                            // By index, not as the model, so a checked chip is kept and its mark pops.
                                                            readonly property var modelData: result.r.fields[index] ?? ({})
                                                            onClicked: lookupModel.setFieldChecked(result.r.id, modelData.key, !modelData.checked)
                                                            ToolTip.visible: hovered
                                                            ToolTip.delay: Theme.tooltip.delay
                                                            ToolTip.text: modelData.replaces && modelData.current !== modelData.value ? qsTr("IGDB: %1\n\nNow: %2").arg(modelData.value).arg(modelData.current)
                                                                                             : String(modelData.value)
                                                            contentItem: RowLayout {
                                                                spacing: Theme.layout.spacing
                                                                Check { checked: fieldChip.modelData.checked; onToggled: fieldChip.clicked() }
                                                                RowLayout {
                                                                    spacing: Theme.layout.spacing / 2
                                                                    opacity: fieldChip.modelData.checked ? 1 : 0.5
                                                                    ChipText {
                                                                        text: fieldChip.modelData.key === "year" ? qsTr("Year %1").arg(fieldChip.modelData.value)
                                                                            : fieldChip.modelData.key === "rating" ? qsTr("Rating %1/5").arg(fieldChip.modelData.value)
                                                                            : fieldChip.modelData.replaces ? qsTr("Replace description") : fieldChip.modelData.name
                                                                    }
                                                                    ChipText {
                                                                        visible: fieldChip.modelData.replaces && fieldChip.modelData.key !== "description"
                                                                        text: fieldChip.modelData.current === fieldChip.modelData.value ? qsTr("same, mark as from IGDB")
                                                                                                                                        : qsTr("was %1").arg(fieldChip.modelData.current)
                                                                        color: Theme.gameEditor.hintColor
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            SectionCard {
                                                visible: result.r.tags.length > 0
                                                label: qsTr("Tags")
                                                Flow {
                                                    Layout.fillWidth: true
                                                    spacing: 2
                                                    Repeater {
                                                        model: result.r.tags.length
                                                        ResultChip {
                                                            id: tagChip
                                                            required property int index
                                                            readonly property var modelData: result.r.tags[index] ?? ({})
                                                            onClicked: lookupModel.setTagChecked(result.r.id, modelData.name, !modelData.checked)
                                                            ToolTip.visible: hovered
                                                            ToolTip.delay: Theme.tooltip.delay
                                                            ToolTip.text: qsTr("Suggested by IGDB: add %1 to this game").arg(modelData.name)
                                                            contentItem: RowLayout {
                                                                spacing: Theme.layout.spacing
                                                                Check { checked: tagChip.modelData.checked; onToggled: tagChip.clicked() }
                                                                TagChip {
                                                                    name: tagChip.modelData.name
                                                                    implicitHeight: Theme.tags.fontSize + 2 * Theme.layout.spacing
                                                                    colorIndex: Library.tags.find(t => t.name === tagChip.modelData.name)?.color ?? 0
                                                                    opacity: tagChip.modelData.checked ? 1 : 0.4
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            SectionCard {
                                                visible: result.r.art.length > 0
                                                label: qsTr("Art")
                                                padding: Theme.layout.spacing * 2
                                                GridLayout {
                                                    readonly property var art: result.r.art
                                                    // Shrinks the pictures when the card is too narrow for them (a window near its minimum
                                                    // width); the sum is the grid's width at hero height h, solved for h.
                                                    readonly property real fixedWidth: 2 * columnSpacing
                                                        + 2 / 3 * (rowSpacing + root.artHeaderHeight + Theme.layout.spacing)
                                                    readonly property int heroHeight: Math.max(24, Math.min(Theme.gameEditor.bulkArtHeight,
                                                        Math.floor((parent.width - fixedWidth) / (3 + 2 / 3 * 1.82))))
                                                    readonly property int smallHeight: Math.round(heroHeight * 0.82)
                                                    function entry(kind) { return art.find(a => a.kind === kind) }
                                                    Layout.fillWidth: true
                                                    columns: 2
                                                    columnSpacing: Theme.layout.spacing * 2.5
                                                    rowSpacing: Theme.layout.spacing * 2
                                                    ArtCell { kind: "hero"; gameId: result.r.id; entry: parent.entry("hero"); previewHeight: parent.heroHeight }
                                                    ArtCell {
                                                        kind: "cover"
                                                        gameId: result.r.id
                                                        entry: parent.entry("cover")
                                                        Layout.rowSpan: 2
                                                        Layout.alignment: Qt.AlignTop
                                                        previewHeight: parent.heroHeight + parent.rowSpacing + root.artHeaderHeight
                                                                       + Theme.layout.spacing + parent.smallHeight
                                                    }
                                                    RowLayout {
                                                        spacing: Theme.layout.spacing * 2.5
                                                        ArtCell { kind: "logo"; gameId: result.r.id; entry: parent.parent.entry("logo"); previewHeight: parent.parent.smallHeight }
                                                        ArtCell { kind: "icon"; gameId: result.r.id; entry: parent.parent.entry("icon"); previewHeight: parent.parent.smallHeight }
                                                    }
                                                }
                                            }

                                            Muted {
                                                visible: result.r.fields.length === 0 && result.r.tags.length === 0 && result.r.art.length === 0
                                                text: qsTr("Nothing to fill in.")
                                            }
                                        }
                                    }
                                }
                            }

                            Surface {
                                id: rowCard
                                width: parent.width
                                height: resultContent.implicitHeight + 2 * Theme.gameEditor.bulkRowPadding
                                radius: Theme.gameEditor.cardRadius
                                bottomLeftRadius: radius * (1 - result.trayShown)
                                bottomRightRadius: radius * (1 - result.trayShown)
                                fill: Theme.gameEditor.cardBackground
                                fillEnd: Theme.gameEditor.cardBackgroundEnd
                                direction: Theme.gameEditor.gradientDirection
                                border.color: Theme.gameEditor.cardBorder
                                border.width: result.bw

                                TapHandler {
                                    enabled: result.r.state !== "running"
                                    onTapped: root.toggleCollapsed(result.r.id)
                                }
                                HoverHandler {
                                    id: resultHover
                                    enabled: result.r.state !== "running"
                                    cursorShape: Qt.PointingHandCursor
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    radius: parent.radius
                                    bottomLeftRadius: parent.bottomLeftRadius
                                    bottomRightRadius: parent.bottomRightRadius
                                    color: resultHover.hovered ? Theme.settingsWindow.rowHover : "transparent"
                                }

                                ColumnLayout {
                                    id: resultContent
                                    anchors.fill: parent
                                    anchors.margins: Theme.gameEditor.bulkRowPadding
                                    spacing: Theme.layout.spacing

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.gameEditor.bulkRowPadding

                                        GameIcon {
                                            size: Theme.gameEditor.bulkIconSize
                                            source: result.r.icon
                                            title: result.r.title
                                        }
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2
                                            Label {
                                                Layout.fillWidth: true
                                                text: result.r.title
                                                color: Theme.gameEditor.lookupText
                                                font.weight: Font.DemiBold
                                                font.pixelSize: Theme.font.size + 1
                                                elide: Text.ElideRight
                                            }
                                            Muted {
                                                Layout.fillWidth: true
                                                font.pixelSize: Theme.font.size - 1
                                                text: result.r.searched !== result.r.title ? qsTr("%1 · searched as %2").arg(result.r.platformName).arg(result.r.searched)
                                                                                          : result.r.platformName
                                                elide: Text.ElideRight
                                            }
                                        }
                                        Badge {
                                            kind: result.r.state
                                            text: ({ running: qsTr("Looking up…"), matched: qsTr("Matched"), pick: qsTr("Which game?"),
                                                     notFound: qsTr("Not found"), error: qsTr("Failed"),
                                                     notOnIgdb: qsTr("Not on IGDB") })[result.r.state] ?? ""
                                        }
                                        AbstractButton {
                                            id: chevron
                                            Layout.alignment: Qt.AlignVCenter
                                            padding: 0
                                            visible: result.r.state !== "running"
                                            implicitWidth: Theme.settingsWindow.iconSize + Theme.layout.spacing * 2
                                            implicitHeight: implicitWidth
                                            hoverEnabled: true
                                            onClicked: root.toggleCollapsed(result.r.id)
                                            scale: chevron.pressed ? Theme.motion.pressScale : 1
                                            Behavior on scale { enabled: Motion.on; MotionNumber {} }
                                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                                            ToolTip.visible: hovered
                                            ToolTip.delay: Theme.tooltip.delay
                                            ToolTip.text: result.open ? qsTr("Hide the details") : qsTr("Show the details")
                                            background: Rectangle {
                                                radius: Theme.radius.small
                                                color: chevron.hovered ? Theme.settingsWindow.rowHover : "transparent"
                                            }
                                            contentItem: Item {
                                                Icon {
                                                    anchors.centerIn: parent
                                                    path: Icons.chevronRight
                                                    size: Theme.settingsWindow.iconSize
                                                    color: Theme.gameEditor.hintColor
                                                    rotation: result.open ? 90 : 0
                                                    Behavior on rotation { enabled: Motion.on; MotionNumber {} }
                                                }
                                            }
                                        }
                                    }

                                    // The panels fold shut with the chevron. Hidden outright when there is none, so an
                                    // empty one adds no gap; its top margin cancels the column's spacing so that gap
                                    // grows with it.
                                    Unfold {
                                        readonly property bool hasPanel: ["pick", "notFound", "error", "notOnIgdb"].includes(result.r.state)
                                                                         || result.r.applyError !== ""
                                        open: result.open
                                        visible: hasPanel && (open || progress > 0)
                                        Layout.fillWidth: true
                                        Layout.topMargin: -Theme.layout.spacing
                                        topPadding: Theme.layout.spacing
                                        spacing: Theme.layout.spacing

                                        Flow {
                                            visible: result.r.state === "pick"
                                            Layout.fillWidth: true
                                            Layout.leftMargin: root.detailIndent
                                            spacing: Theme.layout.spacing
                                            Repeater {
                                                model: result.r.state === "pick" ? result.r.candidates.slice(0, 5) : []
                                                Rectangle {
                                                    id: candidate
                                                    required property var modelData
                                                    required property int index
                                                    width: candidateRow.implicitWidth + 2 * Theme.layout.spacing
                                                    height: Theme.gameEditor.bulkThumbnailHeight + Theme.layout.spacing
                                                    radius: Theme.settingsWindow.inputRadius
                                                    color: candidateMouse.containsMouse ? Theme.gameEditor.pickHover : "transparent"
                                                    border.color: Theme.settingsWindow.inputBorder
                                                    border.width: Theme.settingsWindow.inputBorderWidth
                                                    RowLayout {
                                                        id: candidateRow
                                                        anchors.centerIn: parent
                                                        spacing: Theme.layout.spacing
                                                        ArtThumb { source: candidate.modelData.cover }
                                                        ColumnLayout {
                                                            spacing: 2
                                                            Label {
                                                                text: candidate.modelData.name
                                                                color: Theme.gameEditor.lookupText
                                                                Layout.maximumWidth: 200
                                                                elide: Text.ElideRight
                                                            }
                                                            Muted {
                                                                text: [candidate.modelData.year > 0 ? String(candidate.modelData.year) : ""]
                                                                      .concat(candidate.modelData.platforms).filter(s => s !== "").join(" · ")
                                                                Layout.maximumWidth: 200
                                                                elide: Text.ElideRight
                                                            }
                                                        }
                                                    }
                                                    MouseArea {
                                                        id: candidateMouse
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: lookupModel.pick(result.r.id, candidate.index)
                                                    }
                                                }
                                            }
                                        }

                                        RowLayout {
                                            visible: result.r.state === "pick" || result.r.state === "notFound"
                                            Layout.fillWidth: true
                                            Layout.leftMargin: root.detailIndent
                                            spacing: Theme.layout.spacing

                                            Muted {
                                                id: why
                                                Layout.fillWidth: true
                                                // Not its own implicitWidth: an eliding Label's follows the width it was given.
                                                Layout.maximumWidth: Math.ceil(whyFull.implicitWidth) + 2
                                                elide: Text.ElideRight
                                                Muted { id: whyFull; visible: false; text: why.text }
                                                text: result.r.state === "pick" ? qsTr("None of these?")
                                                    : qsTr("IGDB has nothing called \"%1\" on %2.").arg(result.r.searched).arg(result.r.platformName)
                                            }
                                            SettingsButton {
                                                implicitHeight: root.barButtonHeight
                                                text: qsTr("Detailed lookup")
                                                tip: qsTr("Search another name or platform, and see every result")
                                                onClicked: root.openDetailedLookup(result.r)
                                            }
                                            TextLink {
                                                text: qsTr("Not on IGDB")
                                                onClicked: lookupModel.markNotOnIgdb(result.r.id)
                                            }
                                        }

                                        RowLayout {
                                            visible: result.r.state === "error"
                                            Layout.fillWidth: true
                                            Layout.leftMargin: root.detailIndent
                                            spacing: Theme.layout.spacing
                                            Label {
                                                Layout.fillWidth: true
                                                text: result.r.error
                                                color: Theme.gameEditor.errorColor
                                                wrapMode: Text.WordWrap
                                            }
                                            SettingsButton {
                                                implicitHeight: root.barButtonHeight
                                                text: qsTr("Retry")
                                                onClicked: lookupModel.retry(result.r.id, result.r.searched)
                                            }
                                        }

                                        RowLayout {
                                            visible: result.r.state === "notOnIgdb"
                                            Layout.fillWidth: true
                                            Layout.leftMargin: root.detailIndent
                                            spacing: Theme.layout.spacing
                                            Muted {
                                                Layout.fillWidth: true
                                                wrapMode: Text.WordWrap
                                                text: qsTr("Will be marked as not on IGDB, so it's skipped next time.")
                                            }
                                            TextLink {
                                                text: qsTr("Undo")
                                                onClicked: lookupModel.unmarkNotOnIgdb(result.r.id)
                                            }
                                        }

                                        Label {
                                            visible: result.r.applyError !== ""
                                            Layout.fillWidth: true
                                            Layout.leftMargin: root.detailIndent
                                            text: result.r.applyError
                                            color: Theme.gameEditor.errorColor
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }
                        }

                        Label {
                            anchors.centerIn: parent
                            visible: resultList.count === 0
                            color: Theme.gameEditor.hintColor
                            text: lookupModel.running ? qsTr("No results here yet.") : qsTr("No results here.")
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: -Theme.settingsWindow.contentPadding
                    Layout.rightMargin: -Theme.settingsWindow.contentPadding
                    implicitHeight: 1
                    color: Theme.settingsWindow.inputBorder
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: -(Theme.settingsWindow.contentPadding - Theme.gameEditor.rowSpacing)
                    spacing: Theme.layout.spacing

                    Muted {
                        readonly property int needYou: (lookupModel.stateCounts.pick ?? 0) + (lookupModel.stateCounts.notFound ?? 0)
                                                       + (lookupModel.stateCounts.error ?? 0)
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: needYou > 0 ? qsTr("%1 need you · nothing is written until Apply").arg(needYou)
                                          : qsTr("Nothing is written until Apply")
                    }
                    SettingsButton {
                        visible: lookupModel.running || lookupModel.applying
                        implicitHeight: root.barButtonHeight
                        text: qsTr("Stop")
                        onClicked: lookupModel.stop()
                    }
                    SettingsButton {
                        implicitHeight: root.barButtonHeight
                        text: qsTr("Back to the list")
                        enabled: !lookupModel.running && !lookupModel.applying
                        onClicked: root.reviewing = false
                    }
                    SettingsButton {
                        primary: true
                        implicitHeight: root.barButtonHeight
                        text: qsTr("Apply %1").arg(lookupModel.applyCount)
                        enabled: lookupModel.applyCount > 0 && !lookupModel.running && !lookupModel.applying
                        onClicked: lookupModel.apply()
                    }
                }
            }
        }
    }

    // Dragging the top strip, padding above the title included, moves the window, since it has no
    // title bar (see SettingsWindow for TakeOverForbidden).
    Item {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.settingsWindow.contentPadding + windowControls.height + Theme.layout.spacing / 4
        DragHandler {
            target: null
            grabPermissions: PointerHandler.TakeOverForbidden
            onActiveChanged: if (active) root.startSystemMove()
        }
    }

    WindowControls {
        id: windowControls
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Theme.settingsWindow.contentPadding
        anchors.rightMargin: Theme.settingsWindow.contentPadding
    }

    // An outline around the window, over the content; none while it fills the screen. A tiling
    // compositor calls a floating window maximized, so being maximized isn't enough.
    Rectangle {
        anchors.fill: parent
        visible: border.width > 0 && root.visibility !== Window.FullScreen
                 && !(root.visibility === Window.Maximized && root.width > 0 && root.height > 0
                      && WindowState.fillsScreen(root))
        color: "transparent"
        border.color: Theme.gameEditor.bulkWindowBorder
        border.width: Theme.gameEditor.bulkWindowBorderWidth
        radius: Theme.settingsWindow.windowBorderFollowCompositor && WindowState.compositorRounding() >= 0
                ? WindowState.compositorRounding() : Theme.gameEditor.bulkWindowBorderRadius
    }

    WindowFrame {}
}
