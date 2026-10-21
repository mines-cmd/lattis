import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ListView {
    id: root

    required property var schema
    property string emptyText: qsTr("No settings match.")

    property string currentSection: schema.sections.length > 0 ? schema.sections[0].name : ""
    property string currentSubsection: ""
    readonly property alias sectionJump: sectionJump

    signal message(string text, var link)
    signal followersRequested(string section, string key)
    signal goToRequested(string section, string key)

    function jumpTo(section) {
        const row = schema.firstRow(section)
        if (row < 0)
            return
        currentSection = section
        currentSubsection = schema.sections.find(s => s.name === section)?.subsections[0]?.title ?? ""
        sectionJump.go(row, Theme.settingsWindow.sectionSpacing)
    }

    function jumpToSubsection(section, subsection) {
        const row = schema.firstRow(section, subsection)
        if (row < 0)
            return
        currentSection = section
        currentSubsection = subsection
        sectionJump.go(row, Theme.settingsWindow.subsectionSpacing)
    }

    function jumpBy(step) {
        const sections = schema.sections
        const i = sections.findIndex(s => s.name === currentSection)
        const next = sections[i + step]
        if (next !== undefined)
            jumpTo(next.name)
    }

    function updateCurrentSection() {
        if (sectionJump.busy || scrollBar.probing)
            return
        const item = itemAt(0, contentY + 1)
        if (item) {
            currentSection = item.section
            currentSubsection = item.subsection
        }
    }

    clip: true
    spacing: 0
    model: schema
    boundsBehavior: Flickable.StopAtBounds

    readonly property alias scrollBar: scrollBar

    // Rows differ in height and only those near the view exist, so contentHeight is an estimate
    // that changes as rows are made. An attached bar drags in pixels of that estimate, so the page
    // and the current section jump while dragging; this bar drags by row index instead.
    SettingsScrollBar {
        id: scrollBar

        property real lastTop: 0
        // Keeps the probe's trip to the end from changing the current section, which the sidebar follows.
        property bool probing: false

        parent: root
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        orientation: Qt.Vertical
        size: root.visibleArea.heightRatio

        function rowAt(y) {
            const i = root.indexAt(0, y)
            const item = root.itemAtIndex(i)
            return item ? i + (y - item.y) / item.height : i
        }

        function follow() {
            if (!pressed)
                position = root.visibleArea.yPosition
        }

        function showRow(t) {
            const i = Math.floor(t)
            root.positionViewAtIndex(i, ListView.Beginning)
            const item = root.itemAtIndex(i)
            if (item)
                root.contentY = Math.min(root.contentY + (t - i) * item.height,
                                         root.originY + root.contentHeight - root.height)
        }

        onPressedChanged: {
            if (!pressed) {
                follow()
                return
            }
            if (root.count === 0)
                return
            // Restored by row, not by contentY: jumping to the end moves originY, so the old
            // contentY can point past the rows.
            const top = Math.max(0, rowAt(root.contentY))
            probing = true
            root.positionViewAtEnd()
            lastTop = Math.max(0, rowAt(root.contentY + 1))
            showRow(top)
            probing = false
        }

        onPositionChanged: {
            if (pressed && root.count > 0)
                showRow(size < 1 ? Math.min(1, position / (1 - size)) * lastTop : 0)
        }

        Connections {
            target: root.visibleArea
            function onYPositionChanged() { scrollBar.follow() }
        }
    }

    SectionJump {
        id: sectionJump
        list: root
    }

    Label {
        visible: root.count === 0
        anchors.horizontalCenter: parent.horizontalCenter
        y: 40
        width: Math.min(implicitWidth, parent.width - 40)
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: root.emptyText
        color: Theme.controls.descriptionColor
    }

    onContentYChanged: updateCurrentSection()
    onCountChanged: updateCurrentSection()

    delegate: ColumnLayout {
        readonly property string section: model.section
        readonly property string subsection: model.subsection
        readonly property Item rowItem: settingRow

        width: root.width - scrollBar.width - 8
        spacing: 0

        ColumnLayout {
            visible: model.firstInSection
            Layout.fillWidth: true
            Layout.topMargin: index === 0 ? 0 : Theme.settingsWindow.sectionSpacing
            Layout.bottomMargin: 10
            spacing: 4

            Label {
                text: model.sectionTitle
                color: Theme.settingsWindow.sectionHeaderColor
                font.pixelSize: Theme.settingsWindow.sectionHeaderFontSize
                font.weight: Theme.font.strongWeight
            }
            Label {
                visible: model.sectionDoc !== ""
                Layout.fillWidth: true
                text: model.sectionDoc
                color: Theme.controls.descriptionColor
                font.pixelSize: Theme.controls.descriptionFontSize
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            visible: model.firstInCard && model.subsection !== ""
            Layout.fillWidth: true
            Layout.topMargin: model.firstInSection ? 0 : Theme.settingsWindow.subsectionSpacing
            Layout.bottomMargin: 8
            spacing: 8

            Label {
                text: model.subsection
                color: Theme.settingsWindow.subsectionColor
                font.pixelSize: Theme.settingsWindow.subsectionFontSize
            }
            Label {
                text: model.subsectionCount
                color: Theme.controls.descriptionColor
                // Digits are cap height and semibold, so at the same size they read larger than the lowercase title.
                font.pixelSize: Math.round(Theme.settingsWindow.subsectionFontSize * 0.9)
                font.weight: Theme.font.emphasisWeight
            }
            Item { Layout.fillWidth: true }
        }

        Item {
            id: slice

            readonly property real padding: Theme.settingsWindow.cardPadding
            readonly property real topPadding: model.firstInCard ? padding : 0
            readonly property real bottomPadding: model.lastInCard ? padding : 0

            Layout.fillWidth: true
            implicitHeight: settingRow.implicitHeight + topPadding + bottomPadding
            clip: true

            // Runs past the slice's top/bottom unless it's the first/last row, so the
            // clip hides the rounded corners and outline there.
            Rectangle {
                readonly property real overhang: radius + border.width + 1

                y: model.firstInCard ? 0 : -overhang
                width: parent.width
                height: parent.height + (model.firstInCard ? 0 : overhang) + (model.lastInCard ? 0 : overhang)
                radius: Theme.settingsWindow.cardRadius
                color: Theme.settingsWindow.cardBackground
                border.color: Theme.settingsWindow.cardBorder
                border.width: Theme.settingsWindow.cardBorderWidth
            }

            Rectangle {
                visible: Theme.settingsWindow.rowDividers && !model.firstInCard
                x: slice.padding + Theme.settingsWindow.rowPadding
                width: parent.width - x * 2
                height: 1
                color: Theme.settingsWindow.rowDivider
            }

            SettingRow {
                id: settingRow
                firstInCard: model.firstInCard
                lastInCard: model.lastInCard
                x: slice.padding
                y: slice.topPadding
                width: parent.width - slice.padding * 2
                section: model.section
                key: model.key
                title: model.title
                type: model.type
                doc: model.doc
                min: model.min
                max: model.max
                choices: model.choices
                onRelease: model.onRelease
                defaultText: model.defaultText
                value: model.value
                raw: model.raw
                sourceLabel: model.sourceLabel
                isCustom: model.isCustom
                revertText: model.revertText
                linkTarget: model.linkTarget
                linkTitle: model.linkTitle
                followers: model.followers
                ownValue: model.ownValue

                onFollowersClicked: root.followersRequested(model.section, model.key)
                onGoToRequested: target => {
                    const dot = target.indexOf(".")
                    root.goToRequested(target.slice(0, dot), target.slice(dot + 1))
                }

                onResetClicked: root.message(qsTr("%1 reset to default").arg(title),
                                             { text: qsTr("Undo"), run: () => Theme.undoReset() })
            }
        }
    }
}
