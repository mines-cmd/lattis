import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
ApplicationWindow {
    id: root

    objectName: "settingsWindow"

    onClosing: close => {
        close.accepted = false
        visible = false
        if (!Theme.settingsWindow.rememberSize)
            sizeForOpening()
    }

    minimumWidth: 640
    minimumHeight: 420

    // Sized only while hidden: Hyprland mishandles a client resizing its own mapped window.
    function sizeForOpening() {
        const t = Theme.settingsWindow
        width = t.rememberSize ? WindowState.width("settings", t.width) : t.width
        height = t.rememberSize ? WindowState.height("settings", t.height) : t.height
    }
    Connections {
        target: Theme.settingsWindow
        function onWidthChanged() { if (!root.visible) root.sizeForOpening() }
        function onHeightChanged() { if (!root.visible) root.sizeForOpening() }
        function onRememberSizeChanged() { if (!root.visible) root.sizeForOpening() }
    }
    Component.onCompleted: {
        sizeForOpening()
        if (Theme.settingsWindow.rememberSize && WindowState.maximized("settings"))
            visibility = Window.Maximized
        WindowState.track(this, "settings")
        shownCategory = currentCategory
    }
    onVisibleChanged: if (visible) Theme.beginEditSession()
    title: qsTr("Settings")
    color: Theme.settingsWindow.background
    flags: Qt.Window | Qt.FramelessWindowHint

    font.pixelSize: Theme.font.size
    font.family: Theme.font.family !== "" ? Theme.font.family : Qt.application.font.family
    // Like the main window's: a window of its own doesn't inherit it, and Basic's tooltips are white.
    palette.toolTipBase: Theme.tooltip.background
    palette.toolTipText: Theme.tooltip.text
    palette.dark: Theme.tooltip.border

    readonly property var categories: [
        { key: "general", name: qsTr("General"), icon: Icons.gear, enabled: true, page: general },
        { key: "game_page", name: qsTr("Game page"), icon: Icons.layoutMasonryLine, enabled: true, page: gamePage },
        { key: "library", name: qsTr("Library"), icon: Icons.folder, enabled: true, page: library },
        { key: "steam", name: qsTr("Steam"), icon: Icons.steam, enabled: true, page: steam },
        { key: "platforms", name: qsTr("Platforms"), icon: Icons.computer, enabled: true,
          page: platforms },
        { key: "emulators", name: qsTr("Emulators"), icon: Icons.gamepad, enabled: true,
          page: emulators },
        { key: "metadata", name: qsTr("Metadata"), icon: Icons.search, enabled: true, page: metadata },
        { key: "appearance", name: qsTr("Appearance"), icon: Icons.brush, enabled: true, page: appearance },
        { key: "customize", name: qsTr("Customize"), icon: Icons.paletteLine, enabled: true,
          sections: Theme.customizeSchema.sections, page: customizePage },
        { key: "keyboard", name: qsTr("Keyboard"), icon: Icons.keyboard, enabled: true,
          sections: Actions.model.sections, page: keyboard },
    ]
    property int currentCategory: categories.findIndex(c => c.enabled)
    // The main window's selected game, for the Game page's examples.
    property var game: ({})
    // Switching to a category without sections skips the animation, so the links closing above don't slide it.
    property bool animateLinks: false
    readonly property var currentPage: categories[currentCategory].page?.item ?? null

    // The page switch moves along the sidebar's axis: going down, the old page slides up and
    // fades out while the new one rises into its place from below; going up, the reverse. The
    // two fades run together so their opacities always add up to 1: staggered, there were frames
    // with both pages nearly gone, which read as a blink. Driven from here rather than from each
    // loader's `current`, so the direction is known before either page reacts.
    property int shownCategory: -1
    readonly property int pageSlide: Motion.duration(Theme.motion.normal)
    readonly property int pageFade: Math.round(pageSlide / 2)

    onCurrentCategoryChanged: {
        const direction = currentCategory > shownCategory ? 1 : -1
        categories[shownCategory]?.page.leave(direction)
        categories[currentCategory].page.enter(direction)
        shownCategory = currentCategory
    }

    function showCategory(key, item) {
        const half = key === "colors" || key === "style" ? key : ""
        const i = categories.findIndex(c => c.key === (half !== "" ? "customize" : key))
        if (i < 0)
            return
        currentCategory = i
        const page = categories[i].page.item
        if (half !== "")
            page.reveal(item ?? "", half)
        else if (item !== undefined && page.reveal !== undefined)
            page.reveal(item)
    }

    component PageLoader: Loader {
        id: pageLoader

        property bool visited: false
        readonly property bool current: root.categories[root.currentCategory].page === pageLoader

        function load() {
            visited = true
            return item
        }

        property real shift: 0

        function enter(direction) {
            leaveSlide.stop()
            enterSlide.stop()
            opacity = 0
            shift = direction * Theme.settingsWindow.pageOffset
            enterSlide.start()
        }

        function leave(direction) {
            enterSlide.stop()
            leaveShift.to = -direction * Theme.settingsWindow.pageOffset
            leaveSlide.start()
        }

        anchors.fill: parent
        active: current || visited
        opacity: 0
        visible: opacity > 0
        enabled: current
        transform: Translate { y: pageLoader.shift }
        onLoaded: visited = true
        Component.onCompleted: opacity = current ? 1 : 0

        // The fades are linear: the theme's curves land a third of a fade in the first frame.
        ParallelAnimation {
            id: leaveSlide
            NumberAnimation {
                target: pageLoader
                property: "opacity"
                to: 0
                duration: root.pageFade
            }
            NumberAnimation {
                id: leaveShift
                target: pageLoader
                property: "shift"
                duration: root.pageSlide
                easing.type: Motion.easingType
                easing.bezierCurve: Motion.easingCurve
            }
        }

        ParallelAnimation {
            id: enterSlide
            NumberAnimation {
                target: pageLoader
                property: "opacity"
                to: 1
                duration: root.pageFade
            }
            NumberAnimation {
                target: pageLoader
                property: "shift"
                to: 0
                duration: root.pageSlide
                easing.type: Motion.easingType
                easing.bezierCurve: Motion.easingCurve
            }
        }
    }

    Timer {
        interval: 120
        repeat: true
        running: root.visible
        onTriggered: {
            const next = root.categories.find(c => !c.page.visited)
            if (next)
                next.page.load()
            else
                stop()
        }
    }

    signal lookUpMissing()
    signal suggestTagsFromIgdb()
    signal runSetupGuide()

    function run(name) {
        switch (name) {
        case "settings_search": currentPage?.focusSearch(); break
        case "close_settings": close(); break
        case "previous_section": currentPage?.jumpBy(-1); break
        case "next_section": currentPage?.jumpBy(1); break
        case "new_colors":
            showCategory("customize")
            customizePage.item.newItem("colors")
            break
        case "new_style":
            showCategory("customize")
            customizePage.item.newItem("style")
            break
        case "open_colors_file": customizePage.load().openFile("colors"); break
        case "open_style_file": customizePage.load().openFile("style"); break
        case "show_shortcuts": showCategory("keyboard"); break
        case "back_up_library": showCategory("library", "backup-library"); break
        case "back_up_saves": showCategory("library", "backup-saves"); break
        case "import_backup": showCategory("library", "import"); break
        case "open_data_folder": GameRunner.openDataFolder(); break
        case "undo": settingsToast.undo(); break
        case "quit": Qt.quit(); break
        }
    }

    ActionShortcuts { scope: "settings" }

    Connections {
        target: Actions
        function onTriggered(name, target) { if (target === root) root.run(name) }
    }

    Surface {
        anchors.fill: parent
        fill: Theme.settingsWindow.background
        fillEnd: Theme.settingsWindow.backgroundEnd
        direction: Theme.controls.gradientDirection
    }

    RowLayout {
        id: layout
        anchors.fill: parent
        spacing: 0

        focus: true
        Keys.onEscapePressed: {
            if (root.activeFocusItem !== layout)
                layout.forceActiveFocus()
            else if (!(root.currentPage?.clearSearch() ?? false))
                root.close()
        }

        function isEmptySpace(x, y) {
            let item = layout
            let p = Qt.point(x, y)
            for (;;) {
                const child = item.childAt(p.x, p.y)
                if (child === null)
                    return true
                if (child.enabled && (child instanceof Control || child instanceof MouseArea
                                      || child instanceof TextInput || child instanceof TextEdit))
                    return false
                p = child.mapFromItem(item, p.x, p.y)
                item = child
            }
        }

        TapHandler {
            onTapped: eventPoint => {
                if (root.activeFocusItem !== layout
                        && layout.isEmptySpace(eventPoint.position.x, eventPoint.position.y))
                    layout.forceActiveFocus()
            }
        }

        Item {
            readonly property bool container: Theme.settingsWindow.sidebarContainer
            readonly property int inset: container ? Theme.settingsWindow.sidebarContainerMargin : 0

            Layout.fillHeight: true
            Layout.preferredWidth: Theme.settingsWindow.sidebarWidth

            Surface {
                id: sidebarPanel
                anchors.fill: parent
                anchors.margins: parent.inset
                radius: parent.container ? Theme.settingsWindow.sidebarContainerRadius : 0
                fill: Theme.settingsWindow.sidebarBackground
                fillEnd: Theme.settingsWindow.sidebarBackgroundEnd
                direction: Theme.controls.gradientDirection
                border.color: Theme.settingsWindow.sidebarBorder
                border.width: parent.container ? Theme.settingsWindow.sidebarBorderWidth : 0
            }

            Rectangle {
                visible: !parent.container
                anchors.right: parent.right
                width: Theme.settingsWindow.sidebarBorderWidth
                height: parent.height
                color: Theme.settingsWindow.sidebarBorder
            }

            Flickable {
                id: sidebar
                anchors.fill: sidebarPanel
                anchors.margins: sidebarPanel.border.width
                anchors.rightMargin: parent.container ? sidebarPanel.border.width : Theme.settingsWindow.sidebarBorderWidth
                contentHeight: sidebarColumn.implicitHeight + 20
                interactive: contentHeight > height
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ScrollIndicator.vertical: ScrollIndicator {}

                function ensureVisible(item) {
                    const top = item.mapToItem(sidebar.contentItem, 0, 0).y - 10
                    const bottom = top + item.height + 20
                    let y = contentY
                    if (top < y)
                        y = top
                    else if (bottom > y + height)
                        y = bottom - height
                    contentY = Math.max(0, Math.min(contentHeight - height, y))
                }

                ColumnLayout {
                    id: sidebarColumn
                    x: 10
                    y: 10
                    width: sidebar.width - 20
                    spacing: 4

                    Repeater {
                        model: root.categories

                        ColumnLayout {
                            id: category
                            required property var modelData
                            required property int index
                            readonly property bool selected: root.currentCategory === index
                            readonly property var sections: modelData.sections ?? []
                            property bool expanded: true

                            Layout.fillWidth: true
                            spacing: 0

                            Surface {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Theme.settingsWindow.sidebarItemHeight
                                radius: Theme.settingsWindow.sidebarItemRadius
                                fill: category.selected ? Theme.settingsWindow.sidebarSelectedBackground
                                    : mouse.containsMouse && category.modelData.enabled ? Theme.controls.hover
                                    : Qt.alpha(Theme.controls.hover, 0)
                                fillEnd: category.selected ? Theme.settingsWindow.sidebarSelectedBackgroundEnd
                                       : mouse.containsMouse && category.modelData.enabled ? Theme.controls.hover
                                       : Qt.alpha(Theme.controls.hover, 0)
                                direction: Theme.controls.gradientDirection
                                opacity: category.modelData.enabled ? 1 : 0.45

                                Behavior on fill { enabled: Motion.fades; MotionColor {} }
                                Behavior on fillEnd { enabled: Motion.fades; MotionColor {} }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    spacing: 10

                                    Icon {
                                        path: category.modelData.icon
                                        size: 18
                                        color: category.selected ? Theme.settingsWindow.sidebarSelectedText
                                             : Theme.settingsWindow.sidebarText

                                        Behavior on color { enabled: Motion.fades; MotionColor {} }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: category.modelData.name
                                        color: category.selected ? Theme.settingsWindow.sidebarSelectedText
                                             : category.modelData.enabled ? Theme.settingsWindow.sidebarText
                                             : Theme.settingsWindow.sidebarTextMuted
                                        elide: Text.ElideRight
                                        font.weight: category.selected ? Theme.font.onAccentWeight : Font.Normal

                                        Behavior on color { enabled: Motion.fades; MotionColor {} }
                                    }

                                    Icon {
                                        visible: category.sections.length > 0
                                        path: Icons.chevron
                                        size: Theme.controls.iconSize
                                        color: category.selected ? Theme.settingsWindow.sidebarSelectedText
                                             : Theme.settingsWindow.sidebarText
                                        rotation: category.selected && category.expanded ? 90 : 0

                                        Behavior on color { enabled: Motion.fades; MotionColor {} }
                                        Behavior on rotation { enabled: Motion.on; MotionNumber {} }
                                    }
                                }

                                MouseArea {
                                    id: mouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: category.modelData.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    enabled: category.modelData.enabled
                                    onClicked: {
                                        if (category.selected) {
                                            root.animateLinks = true
                                            category.expanded = !category.expanded
                                        } else {
                                            root.animateLinks = category.sections.length > 0
                                            root.currentCategory = category.index
                                            category.expanded = true
                                        }
                                    }
                                }

                                ToolTip.visible: !category.modelData.enabled && mouse.containsMouse
                                ToolTip.text: qsTr("Not implemented yet")
                                ToolTip.delay: Theme.tooltip.delay
                            }

                            Item {
                                id: links
                                readonly property real reveal: category.selected && category.expanded ? 1 : 0
                                property real shown: reveal

                                Behavior on shown { enabled: Motion.on && root.animateLinks; MotionNumber { speed: "normal" } }

                                visible: category.sections.length > 0
                                Layout.fillWidth: true
                                Layout.preferredHeight: linkColumn.implicitHeight * shown
                                // The gap under the header is part of the animated height so it doesn't pop in.
                                Layout.topMargin: 4 * shown
                                clip: true

                                ColumnLayout {
                                    id: linkColumn
                                    width: parent.width
                                    spacing: 2
                                    opacity: links.shown

                                    Repeater {
                                        model: category.sections

                                        ColumnLayout {
                                            id: entry
                                            required property var modelData
                                            required property int index
                                            readonly property var subsections: modelData.subsections ?? []
                                            readonly property bool sectionCurrent: category.modelData.page.item?.currentSection === modelData.name
                                            readonly property string currentSubsection: sectionCurrent ? category.modelData.page.item?.currentSubsection ?? "" : ""

                                            Layout.fillWidth: true
                                            spacing: 2

                                            Rectangle {
                                                id: link
                                                readonly property bool current: entry.sectionCurrent

                                                onCurrentChanged: if (current && links.shown === 1) sidebar.ensureVisible(link)

                                                Connections {
                                                    target: links
                                                    function onShownChanged() {
                                                        if (link.current && links.shown === 1)
                                                            sidebar.ensureVisible(link)
                                                    }
                                                }

                                                Layout.fillWidth: true
                                                Layout.leftMargin: Theme.settingsWindow.sidebarSectionIndent
                                                Layout.preferredHeight: Theme.settingsWindow.sidebarSectionHeight
                                                radius: Theme.settingsWindow.sidebarItemRadius
                                                color: current ? Theme.settingsWindow.sidebarSectionSelectedBackground
                                                     : linkMouse.containsMouse ? Theme.controls.hover
                                                     : "transparent"

                                                Rectangle {
                                                    visible: link.current && entry.subsections.length === 0
                                                    x: 0
                                                    width: 3
                                                    height: parent.height - 12
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    radius: width / 2
                                                    color: Theme.settingsWindow.sidebarSectionMarker
                                                }

                                                Label {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 10
                                                    anchors.rightMargin: entry.subsections.length > 0 ? 10 + chevron.width + 4 : 10
                                                    verticalAlignment: Text.AlignVCenter
                                                    text: entry.modelData.title
                                                    color: link.current || linkMouse.containsMouse
                                                           ? Theme.settingsWindow.sidebarSectionSelectedText
                                                           : Theme.settingsWindow.sidebarSectionText
                                                    font.pixelSize: Theme.settingsWindow.sidebarSectionFontSize
                                                    elide: Text.ElideRight
                                                }

                                                Icon {
                                                    id: chevron
                                                    visible: entry.subsections.length > 0
                                                    anchors.right: parent.right
                                                    anchors.rightMargin: 10
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    path: Icons.chevron
                                                    size: Theme.controls.iconSize
                                                    color: link.current || linkMouse.containsMouse
                                                           ? Theme.settingsWindow.sidebarSectionSelectedText
                                                           : Theme.settingsWindow.sidebarSectionText
                                                    rotation: 90 * subLinks.shown

                                                    Behavior on color { enabled: Motion.fades; MotionColor {} }
                                                }

                                                MouseArea {
                                                    id: linkMouse
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: category.modelData.page.item.jumpTo(entry.modelData.name)
                                                }
                                            }

                                            Item {
                                                id: subLinks
                                                readonly property real reveal: entry.sectionCurrent && entry.subsections.length > 0 ? 1 : 0
                                                property real shown: reveal

                                                Behavior on shown { enabled: Motion.on; MotionNumber { speed: "normal" } }

                                                visible: shown > 0
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: subColumn.implicitHeight * shown
                                                clip: true

                                                ColumnLayout {
                                                    id: subColumn
                                                    width: parent.width
                                                    spacing: 2
                                                    opacity: subLinks.shown

                                                    Repeater {
                                                        model: entry.subsections

                                                        Rectangle {
                                                            id: subLink
                                                            required property var modelData
                                                            readonly property bool current: entry.currentSubsection === modelData.title

                                                            onCurrentChanged: if (current && links.shown === 1) sidebar.ensureVisible(subLink)

                                                            Layout.fillWidth: true
                                                            Layout.leftMargin: Theme.settingsWindow.sidebarSectionIndent
                                                                               + Theme.settingsWindow.sidebarSubsectionIndent
                                                            Layout.preferredHeight: Theme.settingsWindow.sidebarSectionHeight
                                                            radius: Theme.settingsWindow.sidebarItemRadius
                                                            color: current ? Theme.settingsWindow.sidebarSectionSelectedBackground
                                                                 : subMouse.containsMouse ? Theme.controls.hover
                                                                 : "transparent"

                                                            Rectangle {
                                                                visible: subLink.current
                                                                x: 0
                                                                width: 3
                                                                height: parent.height - 12
                                                                anchors.verticalCenter: parent.verticalCenter
                                                                radius: width / 2
                                                                color: Theme.settingsWindow.sidebarSectionMarker
                                                            }

                                                            Label {
                                                                anchors.fill: parent
                                                                anchors.leftMargin: 10
                                                                anchors.rightMargin: 10
                                                                verticalAlignment: Text.AlignVCenter
                                                                text: subLink.modelData.title
                                                                color: subLink.current || subMouse.containsMouse
                                                                       ? Theme.settingsWindow.sidebarSectionSelectedText
                                                                       : Theme.settingsWindow.sidebarSectionText
                                                                font.pixelSize: Theme.settingsWindow.sidebarSectionFontSize
                                                                elide: Text.ElideRight
                                                            }

                                                            MouseArea {
                                                                id: subMouse
                                                                anchors.fill: parent
                                                                hoverEnabled: true
                                                                cursorShape: Qt.PointingHandCursor
                                                                onClicked: category.modelData.page.item.jumpToSubsection(
                                                                               entry.modelData.name, subLink.modelData.title)
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
                }
            }
        }

        ColumnLayout {
            readonly property int inset: Theme.settingsWindow.sidebarContainer
                                         ? Theme.settingsWindow.sidebarContainerMargin : 0

            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.topMargin: parent.inset
                Layout.rightMargin: parent.inset
                Layout.leftMargin: Theme.settingsWindow.contentPadding
                spacing: 10

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    DragHandler {
                        target: null
                        grabPermissions: PointerHandler.CanTakeOverFromAnything
                        onActiveChanged: if (active) root.startSystemMove()
                    }

                    Repeater {
                        model: root.categories

                        Label {
                            required property var modelData

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.topMargin: Theme.settingsWindow.pageDescriptionTopMargin - parent.parent.parent.inset
                            opacity: modelData.page.opacity
                            visible: opacity > 0
                            text: modelData.page.item?.description ?? ""
                            color: Theme.controls.descriptionColor
                            font.pixelSize: Theme.controls.descriptionFontSize
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }
                }

                WindowControls {
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: Theme.settingsWindow.controlsMargin
                    Layout.rightMargin: Theme.settingsWindow.controlsMargin
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                // A page sliding in would otherwise draw over the title bar or the window's edge.
                clip: true

                PageLoader {
                    id: general
                    source: "GeneralPage.qml"
                    Connections {
                        target: general.item
                        function onRunSetupGuide() { root.runSetupGuide() }
                        function onMessage(text) { root.showMessage(text) }
                    }
                }

                PageLoader {
                    id: gamePage
                    source: "GamePage.qml"
                }

                PageLoader {
                    id: appearance
                    source: "AppearancePage.qml"
                    Connections {
                        target: appearance.item
                        function onMessage(text, link) { root.showMessage(text, link) }
                    }
                }

                PageLoader {
                    id: customizePage
                    source: "CustomizePage.qml"
                    Connections {
                        target: customizePage.item
                        function onMessage(text, link) { root.showMessage(text, link) }
                    }
                }

                PageLoader {
                    id: keyboard
                    source: "KeyboardPage.qml"
                    Connections {
                        target: keyboard.item
                        function onMessage(text) { root.showMessage(text) }
                    }
                }

                PageLoader {
                    id: library
                    source: "LibraryPage.qml"
                    Connections {
                        target: library.item
                        function onSuggestTagsFromIgdb() { root.suggestTagsFromIgdb() }
                        function onShowColors(section) { root.showCategory("colors", section) }
                    }
                }

                PageLoader {
                    id: platforms
                    source: "PlatformsPage.qml"
                    Connections {
                        target: platforms.item
                        function onMessage(text) { root.showMessage(text) }
                    }
                }

                PageLoader {
                    id: emulators
                    source: "EmulatorsPage.qml"
                    Connections {
                        target: emulators.item
                        function onMessage(text) { root.showMessage(text) }
                    }
                }

                PageLoader {
                    id: metadata
                    source: "MetadataPage.qml"
                    Connections {
                        target: metadata.item
                        function onLookUpMissing() { root.lookUpMissing() }
                    }
                }

                PageLoader {
                    id: steam
                    source: "SteamPage.qml"
                    Connections {
                        target: steam.item
                        function onMessage(text) { root.showMessage(text) }
                    }
                }
            }
        }
    }

    property bool changing: false

    function change(fn) {
        changing = true
        try {
            return fn()
        } finally {
            changing = false
        }
    }

    // On the Game page `id` is <place>.<name>.
    function dragPlatformForScreenshot(id, offset) {
        (currentPage === gamePage.item ? gamePage.item : platforms.item)?.dragForScreenshot(id, offset)
    }

    function showMessage(text, link) { settingsToast.show(text, link) }

    Connections {
        target: Library
        function onUndoOffered(token, text) {
            if (root.changing)
                settingsToast.showUndo(text, token)
        }
    }

    Toast {
        id: settingsToast
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
    }

    // An outline around the window, over the content; none while it fills the screen. A tiling
    // compositor calls a floating window maximized, so being maximized isn't enough.
    Rectangle {
        anchors.fill: parent
        visible: border.width > 0 && root.visibility !== Window.FullScreen
                 && !(root.visibility === Window.Maximized && root.width > 0 && root.height > 0
                      && WindowState.fillsScreen(root))
        color: "transparent"
        border.color: Theme.settingsWindow.windowBorder
        border.width: Theme.settingsWindow.windowBorderWidth
        radius: Theme.settingsWindow.windowBorderFollowCompositor && WindowState.compositorRounding() >= 0
                ? WindowState.compositorRounding() : Theme.settingsWindow.windowBorderRadius
    }

    WindowFrame {}
}
