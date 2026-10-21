import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Window

// See docs/code/qml.md.
FocusScope {
    id: root

    property bool reopened: false
    property real dragReserve: 0
    property int current: 0
    property var visited: [0]

    readonly property int lookStep: 4
    readonly property int doneStep: 5
    readonly property var steps: [
        { name: qsTr("Welcome"), title: "", description: "" },
        { name: qsTr("Steam"), title: qsTr("Steam"),
          description: qsTr("Installed Steam games show up in the library by themselves and launch through Steam.") },
        { name: qsTr("Emulators"), title: qsTr("Emulators and ROM folders"),
          description: qsTr("Add the folders where your ROMs are. Each platform's folders are scanned for new games at every start.") },
        { name: qsTr("Game info"), title: qsTr("Game info"),
          description: qsTr("With these free API keys, new games come in with a description, year, rating and art. "
                            + "Only titles are sent. Optional: without keys, games come in with their names, and "
                            + "“Look up missing info” fills them in later.") },
        { name: qsTr("Look"), title: qsTr("Look"),
          description: qsTr("Pick colors and a style; they apply right away. Everything is yours to change in "
                            + "Settings → Colors and Style.") },
        { name: qsTr("Done"), title: qsTr("You're all set"), description: "" },
    ]
    readonly property var page: pages.count > current ? pages.itemAt(current)?.item ?? null : null
    readonly property real cardScale: 1 + (Theme.onboarding.largeScreenScale - 1)
                                          * Math.max(0, Math.min(1, (Screen.height - 1080) / 360))
    property var shortcutsToAdd: []

    property int direction: 1
    property bool shown: false
    property bool closing: false
    property bool ready: false

    signal closed()
    signal message(string text)

    Component.onCompleted: Qt.callLater(() => {
        ready = true
        shown = true
    })

    function dismiss() {
        if (closing)
            return
        closing = true
        if (Motion.on && ready)
            shown = false
        else
            closed()
    }

    function goTo(step) {
        const target = Math.max(0, Math.min(step, steps.length - 1))
        direction = target >= current ? 1 : -1
        if (!visited.includes(target))
            visited = visited.concat([target])
        current = target
    }

    function next() {
        if (page?.next?.())
            return
        if (current === lookStep)
            finish()
        else if (current === doneStep)
            dismiss()
        else
            goTo(current + 1)
    }

    function finish() {
        endSetup()
        goTo(doneStep)
    }

    function skip() {
        endSetup()
        dismiss()
    }

    function endSetup() {
        if (!Library.setupPending)
            return
        let error = Library.finishSetup()
        if (shortcutsToAdd.length > 0) {
            error = Library.addSteamShortcuts(shortcutsToAdd) || error
            shortcutsToAdd = []
        }
        if (error !== "")
            message(error)
    }

    Keys.onPressed: event => {
        if (closing) {
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            next()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape) {
            if (reopened)
                dismiss()
            event.accepted = true
        }
    }

    SetupBackdrop {
        id: backdrop
        anchors.fill: parent
        active: root.shown
    }

    states: State {
        name: "hidden"
        when: !root.shown
        PropertyChanges { backdrop.opacity: 0; pop.opacity: 0; pop.scale: Theme.motion.popupScale }
    }
    transitions: [
        Transition {
            from: "hidden"
            enabled: Motion.on && root.ready
            MotionNumber { target: backdrop; property: "opacity"; speed: "normal"; linear: true }
            MotionNumber { target: pop; property: "opacity"; speed: "normal" }
            MotionPop { target: pop; property: "scale" }
        },
        Transition {
            to: "hidden"
            enabled: Motion.on && root.ready
            SequentialAnimation {
                ParallelAnimation {
                    MotionNumber { target: backdrop; property: "opacity"; speed: "fast"; linear: true }
                    MotionNumber { target: pop; property: "opacity"; speed: "fast" }
                    MotionNumber { target: pop; property: "scale"; speed: "fast" }
                }
                ScriptAction { script: root.closed() }
            }
        }
    ]

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted = true
        onPressed: root.forceActiveFocus()
    }

    Item {
        width: parent.width - root.dragReserve
        height: Theme.layout.margin + Theme.layout.toolbarHeight + Theme.layout.spacing

        DragHandler {
            target: null
            grabPermissions: PointerHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) root.Window.window.startSystemMove()
        }
    }

    component SkipButton: AbstractButton {
        id: skipButton
        implicitHeight: Theme.onboarding.buttonHeight
        implicitWidth: skipLabel.implicitWidth + leftPadding + rightPadding
        leftPadding: 12
        rightPadding: 12
        hoverEnabled: true
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        contentItem: Label {
            id: skipLabel
            text: skipButton.text
            color: skipButton.hovered || skipButton.visualFocus ? Theme.onboarding.skipHoverColor : Theme.onboarding.skipColor
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: Theme.controls.inputRadius
            color: skipButton.hovered ? Theme.palette.hoverOverlay : "transparent"
        }
    }

    Component {
        id: welcomePage
        SetupWelcome { renderScale: root.cardScale }
    }
    Component {
        id: steamPage
        SetupSteam { guide: root }
    }
    Component {
        id: emulatorsPage
        SetupEmulators { guide: root }
    }
    Component {
        id: metadataPage
        SetupMetadata { guide: root }
    }
    Component {
        id: lookPage
        SetupLook {}
    }
    Component {
        id: donePage
        SetupDone { guide: root }
    }

    Item {
        id: pop
        anchors.fill: parent
        enabled: !root.closing

        RectangularShadow {
            visible: Theme.onboarding.cardShadowSize > 0
            anchors.fill: card
            offset.y: Theme.onboarding.cardShadowSize / 6
            radius: Theme.onboarding.cardRadius
            blur: Theme.onboarding.cardShadowSize
            color: Theme.onboarding.cardShadowColor
            scale: root.cardScale
        }

        Surface {
            id: card
            // The bottom edge is fixed (see card_height), so the buttons stay put; the
            // card scales about its center, hence the (1 + scale) / 2.
            readonly property real anchorBottom: root.height / 2
                + Math.min(Theme.onboarding.cardHeight * root.cardScale,
                           root.height - 2 * Theme.onboarding.margin) / 2
            anchors.horizontalCenter: parent.horizontalCenter
            y: anchorBottom - height * (1 + root.cardScale) / 2
            width: Theme.onboarding.cardWidth
            implicitHeight: cardLayout.implicitHeight + 2 * Theme.onboarding.cardPadding
            height: Math.min(Theme.onboarding.cardHeight, (root.height - 2 * Theme.onboarding.margin) / root.cardScale)
            scale: root.cardScale
            Behavior on height { enabled: Motion.on && root.ready; MotionNumber { speed: "normal" } }
            fill: Theme.onboarding.cardBackground
            fillEnd: Theme.onboarding.cardBackgroundEnd
            radius: Theme.onboarding.cardRadius
            border.width: Theme.onboarding.cardBorderWidth
            border.color: Theme.onboarding.cardBorder
            clip: true

            ColumnLayout {
                id: cardLayout
                anchors {
                    fill: parent
                    topMargin: Theme.onboarding.cardPadding
                    bottomMargin: Theme.onboarding.cardPadding
                    leftMargin: Theme.onboarding.cardPadding
                    rightMargin: Theme.onboarding.cardPadding
                }
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.onboarding.stepSpacing

                    Repeater {
                        model: root.steps

                        RowLayout {
                            id: step
                            required property var modelData
                            required property int index
                            readonly property bool done: index < root.current
                            readonly property bool isCurrent: index === root.current
                            readonly property bool last: index === root.steps.length - 1
                            property int finishes: 0

                            onDoneChanged: if (done) finishes++

                            Layout.fillWidth: !last
                            spacing: Theme.onboarding.stepSpacing

                            RowLayout {
                                readonly property bool clickable: root.current !== root.doneStep
                                                                  && step.index !== root.doneStep
                                                                  && !step.isCurrent
                                spacing: Theme.onboarding.stepSpacing

                                HoverHandler {
                                    enabled: parent.clickable
                                    cursorShape: Qt.PointingHandCursor
                                }
                                TapHandler {
                                    enabled: parent.clickable
                                    onTapped: root.goTo(step.index)
                                }

                                Rectangle {
                                    implicitWidth: Theme.onboarding.stepSize
                                    implicitHeight: Theme.onboarding.stepSize
                                    radius: width / 2
                                    // Colors, not widths or "transparent", so the fades pass through
                                    // no other hue: the border stays 2 px and goes clear under the fill.
                                    color: step.done ? Qt.alpha(Theme.onboarding.stepDone, Theme.onboarding.stepDoneOpacity)
                                         : step.isCurrent ? Theme.onboarding.stepCurrent
                                         : Qt.alpha(Theme.onboarding.stepCurrent, 0)
                                    border.width: Theme.onboarding.stepBorderWidth
                                    border.color: step.done || step.isCurrent ? Qt.alpha(Theme.onboarding.stepBorder, 0)
                                                                              : Theme.onboarding.stepBorder

                                    Behavior on color { enabled: Motion.fades && root.ready; MotionColor { speed: "normal" } }
                                    Behavior on border.color { enabled: Motion.fades && root.ready; MotionColor { speed: "normal" } }

                                    BoxBump { value: step.finishes }

                                    MarkPop {
                                        anchors.fill: parent
                                        shown: step.done
                                        speed: "normal"
                                        hiddenScale: 0
                                        overshoot: Theme.motion.markOvershoot * 1.3

                                        Icon {
                                            anchors.centerIn: parent
                                            path: Icons.check
                                            size: Math.round(Theme.onboarding.stepSize * 0.6)
                                            color: Theme.onboarding.stepDone
                                        }
                                    }
                                    // Digits have no descenders, so the line box's middle sits below
                                    // theirs; the offset puts the digits' own middle in the circle's.
                                    FontMetrics { id: digitMetrics; font: number.font }
                                    Label {
                                        id: number
                                        anchors.centerIn: parent
                                        anchors.verticalCenterOffset: {
                                            const digit = digitMetrics.tightBoundingRect("0")
                                            return (digitMetrics.ascent + digitMetrics.descent) / 2
                                                   - (digitMetrics.ascent + digit.y + digit.height / 2)
                                        }
                                        opacity: step.done ? 0 : 1
                                        visible: opacity > 0
                                        text: step.index + 1
                                        font.pixelSize: Theme.onboarding.stepFontSize
                                        font.weight: step.isCurrent ? Theme.font.onAccentWeight : Font.Normal
                                        color: step.isCurrent ? Theme.onboarding.stepCurrentText : Theme.onboarding.stepLabelColor
                                        Behavior on opacity { enabled: Motion.fades && root.ready; MotionNumber { fade: true; speed: "normal" } }
                                        Behavior on color { enabled: Motion.fades && root.ready; MotionColor { speed: "normal" } }
                                    }
                                }
                                Label {
                                    text: step.modelData.name
                                    font.pixelSize: Theme.onboarding.stepLabelFontSize
                                    font.bold: step.isCurrent
                                    color: step.isCurrent ? Theme.onboarding.stepCurrentLabelColor : Theme.onboarding.stepLabelColor
                                    Behavior on color { enabled: Motion.fades && root.ready; MotionColor { speed: "normal" } }
                                }
                            }
                            Rectangle {
                                visible: !step.last
                                Layout.fillWidth: true
                                Layout.leftMargin: 4
                                Layout.rightMargin: 4
                                implicitHeight: 2
                                color: Theme.onboarding.stepLine
                            }
                        }
                    }
                }

                Flickable {
                    id: flick
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.onboarding.titleTopMargin
                    Layout.fillHeight: true
                    implicitHeight: contentCol.implicitHeight
                    clip: true
                    contentHeight: contentCol.height
                    boundsBehavior: Flickable.StopAtBounds

                    ScrollBar.vertical: SettingsScrollBar {}

                    ColumnLayout {
                        id: contentCol
                        width: flick.width - (flick.ScrollBar.vertical.visible
                                              ? Theme.controls.scrollbarWidth + Theme.layout.spacing : 0)
                        // Look scrolls its own list of colors so the styles below stay in view. The
                        // other pages fill a card taller than they are, so Welcome can center.
                        height: root.current === root.lookStep ? flick.height
                                                               : Math.max(implicitHeight, flick.height)
                        spacing: Theme.onboarding.spacing

                        Item {
                            id: stage
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            implicitHeight: {
                                pages.count
                                return pages.itemAt(root.current)?.pageHeight ?? 0
                            }

                            Repeater {
                                id: pages
                                model: root.steps.length

                                Item {
                                    id: slot
                                    required property int index
                                    readonly property bool isCurrent: index === root.current
                                    readonly property real pageHeight: slotLayout.implicitHeight
                                    property alias item: loader.item
                                    readonly property int slide: Motion.duration(Theme.motion.normal)
                                    // The old page fades out fast and is nearly gone when the new one
                                    // starts, so the two share a few frames while one is faint: no
                                    // blank gap to read as a blink, and no legible text on top of text.
                                    // The scroll resets in that moment, too faint to see the old page jump.
                                    readonly property int leaveTime: Math.round(slide * 0.4)
                                    readonly property int overlap: Math.round(slide * 0.15)
                                    property real shift: 0

                                    width: stage.width
                                    height: stage.height
                                    visible: opacity > 0
                                    opacity: isCurrent ? 1 : 0
                                    enabled: isCurrent

                                    onIsCurrentChanged: {
                                        leave.stop()
                                        arrive.stop()
                                        if (!root.ready || !Motion.on) {
                                            shift = 0
                                            opacity = isCurrent ? 1 : 0
                                            if (isCurrent)
                                                flick.contentY = 0
                                            return
                                        }
                                        if (isCurrent) {
                                            opacity = 0
                                            shift = root.direction * Theme.onboarding.stepOffset
                                            arrive.start()
                                        } else {
                                            leave.start()
                                        }
                                    }

                                    ParallelAnimation {
                                        id: leave
                                        NumberAnimation {
                                            target: slot
                                            property: "opacity"
                                            to: 0
                                            duration: slot.leaveTime
                                            easing.type: Motion.preset === "linear" ? Easing.Linear : Easing.OutQuad
                                        }
                                        NumberAnimation {
                                            target: slot
                                            property: "shift"
                                            to: -root.direction * Theme.onboarding.stepOffset
                                            duration: slot.leaveTime
                                            easing.type: Motion.preset === "linear" ? Easing.Linear : Easing.InQuad
                                        }
                                    }
                                    SequentialAnimation {
                                        id: arrive
                                        PauseAnimation { duration: slot.leaveTime - slot.overlap }
                                        ScriptAction { script: flick.contentY = 0 }
                                        ParallelAnimation {
                                            NumberAnimation {
                                                target: slot
                                                property: "opacity"
                                                to: 1
                                                duration: slot.slide - slot.leaveTime
                                                easing.type: Motion.preset === "linear" ? Easing.Linear : Easing.OutQuad
                                            }
                                            NumberAnimation {
                                                target: slot
                                                property: "shift"
                                                to: 0
                                                duration: slot.slide - slot.leaveTime
                                                easing.type: Motion.easingType
                                                easing.bezierCurve: Motion.easingCurve
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        id: slotLayout
                                        width: slot.width
                                        spacing: Theme.onboarding.spacing
                                        transform: Translate { x: slot.shift }

                                        RowLayout {
                                            id: header
                                            readonly property string title: loader.item?.title ?? root.steps[slot.index].title
                                            readonly property string description: loader.item?.description ?? root.steps[slot.index].description

                                            visible: title !== ""
                                            Layout.fillWidth: true
                                            spacing: Theme.onboarding.spacing

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.alignment: Qt.AlignTop
                                                spacing: 6

                                                Label {
                                                    Layout.fillWidth: true
                                                    text: header.title
                                                    color: Theme.onboarding.titleColor
                                                    font.pixelSize: Theme.onboarding.titleSize
                                                    font.bold: true
                                                    wrapMode: Text.Wrap
                                                }
                                                Label {
                                                    visible: text !== ""
                                                    Layout.fillWidth: true
                                                    text: header.description
                                                    color: Theme.onboarding.descriptionColor
                                                    lineHeight: 1.2
                                                    wrapMode: Text.Wrap
                                                }
                                                Loader {
                                                    Layout.topMargin: Theme.layout.spacing
                                                    // The page below is a header gap away; with the action in the
                                                    // way, a card's gap is enough.
                                                    Layout.bottomMargin: -Theme.onboarding.spacing
                                                    visible: item !== null
                                                    sourceComponent: loader.item?.headerAction ?? null
                                                }
                                            }
                                        }

                                        Loader {
                                            id: loader
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            active: root.visited.includes(slot.index)
                                            sourceComponent: [welcomePage, steamPage, emulatorsPage, metadataPage, lookPage, donePage][slot.index]
                                        }
                                    }
                                    // The leaving page keeps the height it had.
                                    Binding {
                                        target: slotLayout
                                        property: "height"
                                        value: slot.height
                                        when: slot.isCurrent
                                        // By default the height falls back to the page's own implicit
                                        // one the moment it leaves, which cuts Look's list short.
                                        restoreMode: Binding.RestoreNone
                                    }
                                    // Likewise the width: the stage narrows when the arriving page
                                    // scrolls (its scrollbar takes room), and the leaving page
                                    // would jump left with it.
                                    Binding {
                                        target: slot
                                        property: "width"
                                        value: stage.width
                                        when: slot.isCurrent
                                        restoreMode: Binding.RestoreNone
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Theme.onboarding.spacing
                    implicitHeight: 1
                    color: Theme.onboarding.footerDivider
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.onboarding.controlSpacing

                    SkipButton {
                        visible: root.current !== root.doneStep
                        text: root.reopened ? qsTr("Close") : qsTr("Skip setup")
                        onClicked: root.skip()
                    }
                    Item { Layout.fillWidth: true }
                    SettingsButton {
                        visible: root.current > 0 && root.current !== root.doneStep
                        implicitHeight: Theme.onboarding.buttonHeight
                        text: qsTr("Back")
                        onClicked: root.goTo(root.current - 1)
                    }
                    SettingsButton {
                        primary: true
                        implicitHeight: Theme.onboarding.buttonHeight
                        text: root.page?.nextText ?? (root.current === root.lookStep ? qsTr("Finish")
                                                      : root.current === root.doneStep ? qsTr("Start") : qsTr("Next"))
                        onClicked: root.next()
                    }
                }
            }
        }
    }
}
