import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
FocusScope {
    id: root

    required property var page
    // Floating over the grid in a narrow window instead of standing beside it.
    property bool panel: false

    property string gameQuery: ""
    property var openYears: ({})

    readonly property var shownGames: {
        const words = fold(gameQuery).split(" ").filter(w => w !== "")
        if (words.length === 0)
            return page.gameList
        return page.gameList.filter(g => {
            const name = fold(g.title)
            return words.every(w => name.includes(w))
        })
    }

    function fold(text: string): string {
        return text.normalize("NFD").replace(/[̀-ͯ]/g, "").toLowerCase()
    }

    function count(section: string, key: string): int {
        return page.counts[section]?.[key] ?? 0
    }

    function yearState(year: var): int {
        const picked = year.months.reduce((n, m) => n + (page.monthSel[m.key] ? 1 : 0), 0)
        return picked === 0 ? 0 : picked === year.months.length ? 2 : 1
    }

    function yearCount(year: var): int {
        return year.months.reduce((n, m) => n + count("month", m.key), 0)
    }

    function openYear(year: int, open: bool) {
        const next = Object.assign({}, openYears)
        next[year] = open
        openYears = next
    }

    // The newest year starts open; later batches add years without closing what was opened.
    Connections {
        target: root.page
        function onYearListChanged() {
            if (Object.keys(root.openYears).length === 0 && root.page.yearList.length > 0)
                root.openYear(root.page.yearList[0].year, true)
        }
        function onStarted() {
            root.openYears = ({})
            root.gameQuery = ""
        }
    }

    Surface {
        anchors.fill: parent
        radius: Theme.gallery.sidebarRadius
        fill: Theme.gallery.sidebarBackground
        fillEnd: Theme.gallery.sidebarBackgroundEnd
        direction: Theme.gallery.sidebarGradientDirection
        border.color: Theme.gallery.sidebarBorder
        border.width: Theme.gallery.sidebarBorderWidth
    }

    component ClearLink: AbstractButton {
        id: link
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        padding: 2
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        background: Rectangle {
            radius: Theme.gallery.entryRadius
            color: "transparent"
            border.color: Theme.filters.focusRing
            border.width: link.visualFocus ? Theme.filters.focusRingWidth : 0
        }
        contentItem: Label {
            text: link.text
            color: Theme.gallery.clearColor
            opacity: link.hovered ? 1 : 0.8
            font.pixelSize: Theme.gallery.sectionSize
            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true; speed: "fast" } }
        }
    }

    component Entry: AbstractButton {
        id: entry

        // 0 = off, 1 = some of its parts, 2 = on
        property int mark: 0
        property string iconPath
        property int amount: -1
        property int indent: 0
        readonly property bool empty: amount === 0 && mark === 0

        Layout.fillWidth: true
        implicitHeight: Theme.gallery.entryHeight
        leftPadding: Theme.layout.spacing + indent
        rightPadding: Theme.layout.spacing
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        opacity: empty ? Theme.gallery.entryEmptyOpacity : 1
        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }

        HoverHandler { cursorShape: Qt.PointingHandCursor }
        ToolTip.visible: hovered && entryName.truncated
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: text

        background: Rectangle {
            radius: Theme.gallery.entryRadius
            color: entry.hovered ? Theme.gallery.entryHover : "transparent"
            border.color: Theme.filters.focusRing
            border.width: entry.visualFocus ? Theme.filters.focusRingWidth : 0
            Behavior on color { enabled: Motion.fades; MotionColor {} }
        }
        contentItem: RowLayout {
            spacing: Theme.layout.spacing

            Rectangle {
                implicitWidth: Theme.controls.checkboxSize
                implicitHeight: Theme.controls.checkboxSize
                radius: Theme.controls.checkboxRadius
                color: entry.mark > 0 ? Theme.controls.checkboxOn : Theme.controls.inputBackground
                border.color: Theme.controls.inputBorder
                border.width: entry.mark > 0 ? 0 : Math.max(1, Theme.controls.inputBorderWidth)
                Behavior on color { enabled: Motion.fades; MotionColor {} }

                MarkPop {
                    anchors.fill: parent
                    shown: entry.mark === 2
                    hiddenScale: 0
                    overshoot: Theme.motion.markOvershoot * 1.3
                    Icon {
                        anchors.centerIn: parent
                        path: Icons.check
                        size: parent.width - 4
                        color: Theme.controls.checkboxMark
                    }
                }
                Rectangle {
                    anchors.centerIn: parent
                    visible: entry.mark === 1
                    width: parent.width - 8
                    height: 2
                    color: Theme.controls.checkboxMark
                }
            }
            Icon {
                visible: entry.iconPath !== ""
                path: entry.iconPath
                size: Theme.gallery.entrySize + 2
                color: Theme.gallery.entryColor
            }
            Label {
                id: entryName
                Layout.fillWidth: true
                text: entry.text
                elide: Text.ElideRight
                color: Theme.gallery.entryColor
                font.pixelSize: Theme.gallery.entrySize
            }
            Label {
                visible: entry.amount >= 0
                text: Number(entry.amount).toLocaleString(Qt.locale(), "f", 0)
                color: Theme.gallery.entryCountColor
                font.pixelSize: Theme.gallery.entrySize - 1
            }
        }
    }

    component FilterSection: ColumnLayout {
        id: section

        property string name
        property string title
        property bool active: false
        default property alias body: unfold.content
        readonly property bool folded: General.galleryFoldedSections.includes(name)
        readonly property real headerHeight: headerRow.implicitHeight
        readonly property alias bodySpacing: unfold.spacing

        Layout.fillWidth: true
        spacing: Theme.layout.spacing / 2

        RowLayout {
            id: headerRow
            Layout.fillWidth: true

            AbstractButton {
                id: foldButton
                Layout.fillWidth: true
                implicitHeight: Theme.gallery.entryHeight - 6
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                onClicked: root.page.message(General.setGallerySectionFolded(section.name, !section.folded))

                HoverHandler { cursorShape: Qt.PointingHandCursor }
                ToolTip.visible: hovered
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: section.folded ? qsTr("Expand") : qsTr("Collapse")

                background: Rectangle {
                    radius: Theme.gallery.entryRadius
                    color: "transparent"
                    border.color: Theme.filters.focusRing
                    border.width: foldButton.visualFocus ? Theme.filters.focusRingWidth : 0
                }
                contentItem: Row {
                    spacing: Theme.layout.spacing / 2

                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        path: Icons.chevronRight
                        size: Theme.gallery.sectionSize + 2
                        color: Theme.gallery.sectionColor
                        rotation: section.folded ? 0 : 90
                        Behavior on rotation { enabled: Motion.on; MotionNumber {} }
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: Theme.gallery.sectionUppercase ? section.title.toUpperCase() : section.title
                        color: Theme.gallery.sectionColor
                        font.weight: Theme.font.strongWeight
                        font.pixelSize: Theme.gallery.sectionSize
                        font.letterSpacing: Theme.gallery.sectionUppercase ? 0.8 : 0
                    }
                }
            }
            ClearLink {
                visible: section.active
                text: qsTr("Clear")
                onClicked: root.page.clear(section.name)
            }
        }
        Unfold {
            id: unfold
            Layout.fillWidth: true
            open: !section.folded
            spacing: 0
        }
    }

    ColumnLayout {
        id: top
        x: Theme.gallery.sidebarPadding
        y: Theme.gallery.sidebarPadding
        width: parent.width - 2 * Theme.gallery.sidebarPadding
        spacing: Theme.gallery.sidebarSpacing

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: qsTr("Filters")
                color: Theme.palette.text
                font.pixelSize: Theme.gallery.sidebarTitleSize
                font.weight: Theme.font.emphasisWeight
            }
            ClearLink {
                visible: root.page.filtersActive
                text: qsTr("Clear all")
                onClicked: root.page.clearAll()
            }
        }
    }

    Flickable {
        id: flick
        x: Theme.gallery.sidebarPadding
        y: top.y + top.height + Theme.gallery.sidebarSpacing
        width: parent.width - 2 * Theme.gallery.sidebarPadding
        height: parent.height - y - Theme.gallery.sidebarPadding
        contentWidth: width
        contentHeight: sections.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: SettingsScrollBar { trackless: true; thickness: Theme.gallery.sidebarScrollbarWidth }

        ColumnLayout {
            id: sections
            width: flick.width - (flick.contentHeight > flick.height ? Theme.layout.spacing * 1.5 : 0)
            spacing: Theme.gallery.sidebarSpacing

            FilterSection {
                id: typeSection
                name: "type"
                title: qsTr("Type")
                active: Object.keys(root.page.typeSel).length > 0

                Repeater {
                    model: [{ id: "screenshot", name: qsTr("Screenshots") }, { id: "clip", name: qsTr("Clips") }]
                    Entry {
                        required property var modelData
                        text: modelData.name
                        mark: root.page.typeSel[modelData.id] ? 2 : 0
                        amount: root.count("type", modelData.id)
                        onClicked: root.page.toggle("type", modelData.id)
                    }
                }
            }

            FilterSection {
                id: platformSection
                name: "platform"
                title: qsTr("Platform")
                active: Object.keys(root.page.platformSel).length > 0

                Repeater {
                    model: root.page.platformList
                    Entry {
                        required property var modelData
                        text: modelData.name
                        iconPath: Icons.forPlatform(modelData.icon)
                        mark: root.page.platformSel[modelData.id] ? 2 : 0
                        amount: root.count("platform", modelData.id)
                        onClicked: root.page.toggle("platform", modelData.id)
                    }
                }
            }

            FilterSection {
                id: gameSection
                name: "game"
                title: qsTr("Game")
                active: Object.keys(root.page.gameSel).length > 0

                // The sidebar's height less everything but the game list, summed from the parts rather
                // than from `sections`, whose height includes the list's own.
                readonly property real listRoom: flick.height - typeSection.implicitHeight - platformSection.implicitHeight
                                                 - dateSection.implicitHeight - 3 * sections.spacing
                                                 - headerHeight - spacing - gameSearch.implicitHeight
                                                 - gameSearch.Layout.bottomMargin - bodySpacing

                SettingsSearchField {
                    id: gameSearch
                    Layout.fillWidth: true
                    implicitHeight: Theme.gallery.searchHeight
                    Layout.bottomMargin: Theme.layout.spacing / 2
                    placeholderText: qsTr("Search games")
                    text: root.gameQuery
                    onTextChanged: root.gameQuery = text
                    Accessible.name: qsTr("Search games")
                }
                ListView {
                    id: gameListView
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(contentHeight, Math.max(Theme.gallery.listHeight, gameSection.listRoom))
                    clip: true
                    model: root.shownGames
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: SettingsScrollBar { trackless: true; thickness: Theme.gallery.sidebarScrollbarWidth }

                    delegate: Entry {
                        required property var modelData
                        width: gameListView.width - (gameListView.contentHeight > gameListView.height ? Theme.layout.spacing * 1.5 : 0)
                        text: modelData.title
                        mark: root.page.gameSel[modelData.id] ? 2 : 0
                        amount: root.count("game", modelData.id)
                        onClicked: root.page.toggle("game", modelData.id)
                    }
                }
                Label {
                    visible: root.shownGames.length === 0
                    Layout.fillWidth: true
                    leftPadding: Theme.layout.spacing
                    text: qsTr("No game by that name has media")
                    wrapMode: Text.WordWrap
                    color: Theme.gallery.entryCountColor
                    font.pixelSize: Theme.gallery.entrySize
                }
            }

            FilterSection {
                id: dateSection
                name: "date"
                title: qsTr("Date")
                active: Object.keys(root.page.monthSel).length > 0

                Flickable {
                    id: dateFlick
                    Layout.fillWidth: true
                    // Room left once the game section keeps its minimum; summed from parts, not from
                    // gameSection.implicitHeight, which depends on this list's height.
                    readonly property real room: flick.height - typeSection.implicitHeight - platformSection.implicitHeight
                                                 - 3 * sections.spacing - dateSection.headerHeight - dateSection.spacing
                                                 - dateSection.bodySpacing - gameSection.headerHeight - gameSection.spacing
                                                 - gameSearch.implicitHeight - gameSearch.Layout.bottomMargin
                                                 - gameSection.bodySpacing - Theme.gallery.listHeight
                    Layout.preferredHeight: Math.min(contentHeight, Math.max(Theme.gallery.listHeight, room))
                    contentWidth: width
                    contentHeight: years.implicitHeight
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: SettingsScrollBar { trackless: true; thickness: Theme.gallery.sidebarScrollbarWidth }

                    ColumnLayout {
                        id: years
                        width: dateFlick.width - (dateFlick.contentHeight > dateFlick.height ? Theme.layout.spacing * 1.5 : 0)
                        spacing: 0

                        Repeater {
                            model: root.page.yearList

                            ColumnLayout {
                                id: yearBlock
                                required property var modelData
                                readonly property bool open: root.openYears[modelData.year] === true

                                Layout.fillWidth: true
                                spacing: 0

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 0

                                    AbstractButton {
                                        id: yearFold
                                        implicitWidth: Theme.gallery.entryHeight
                                        implicitHeight: Theme.gallery.entryHeight
                                        focusPolicy: Qt.StrongFocus
                                        onClicked: root.openYear(yearBlock.modelData.year, !yearBlock.open)
                                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                                        ToolTip.visible: hovered
                                        ToolTip.delay: Theme.tooltip.delay
                                        ToolTip.text: yearBlock.open ? qsTr("Collapse") : qsTr("Expand")
                                        background: Rectangle {
                                            radius: Theme.gallery.entryRadius
                                            color: "transparent"
                                            border.color: Theme.filters.focusRing
                                            border.width: yearFold.visualFocus ? Theme.filters.focusRingWidth : 0
                                        }
                                        contentItem: Item {
                                            Icon {
                                                anchors.centerIn: parent
                                                path: Icons.chevronRight
                                                size: Theme.gallery.entrySize + 2
                                                color: Theme.gallery.sectionColor
                                                rotation: yearBlock.open ? 90 : 0
                                                Behavior on rotation { enabled: Motion.on; MotionNumber {} }
                                            }
                                        }
                                    }
                                    Entry {
                                        text: yearBlock.modelData.year
                                        mark: root.yearState(yearBlock.modelData)
                                        amount: root.yearCount(yearBlock.modelData)
                                        onClicked: root.page.toggleYear(yearBlock.modelData)
                                    }
                                }
                                Unfold {
                                    Layout.fillWidth: true
                                    open: yearBlock.open
                                    spacing: 0

                                    Repeater {
                                        model: yearBlock.modelData.months

                                        Entry {
                                            required property var modelData
                                            indent: Theme.gallery.entryHeight + Theme.gallery.monthIndent
                                            text: modelData.name
                                            mark: root.page.monthSel[modelData.key] ? 2 : 0
                                            amount: root.count("month", modelData.key)
                                            onClicked: root.page.toggle("month", modelData.key)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
