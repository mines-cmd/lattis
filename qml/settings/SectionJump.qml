import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window

Item {
    id: root

    required property ListView list
    // True from go() until the switch is over; the page ignores scrolling meanwhile, so the
    // sidebar keeps the section jumped to.
    readonly property bool busy: grabbing || jumping || animation.running
    property bool grabbing: false
    property bool jumping: false
    property var change: null
    // 0: the direction follows the scroll, and nothing animates if the list didn't move.
    property int forcedDirection: 0
    property var grab: null

    readonly property int slide: Motion.duration(Theme.motion.normal)
    readonly property int fade: Math.round(slide / 2)

    // A child of the list itself, not its contentItem, so the snapshot stays put and is clipped
    // with the rows.
    parent: list
    anchors.fill: parent
    z: 1

    function jump(toRow, gap = Theme.settingsWindow.sectionSpacing) {
        jumping = true
        list.positionViewAtIndex(toRow, ListView.Beginning)
        // Skip the gap above the heading so the heading lands at the top.
        if (toRow > 0)
            list.contentY = Math.max(list.originY,
                                     Math.min(list.contentY + gap,
                                              list.contentHeight - list.height + list.originY))
        jumping = false
    }

    function go(toRow, gap) {
        run(() => jump(toRow, gap), 0)
    }

    function swap(changeRows, direction) {
        run(changeRows, direction)
    }

    function run(changeRows, direction) {
        change = changeRows
        forcedDirection = direction
        if (!Motion.on || !list.visible) {
            changeRows()
            return
        }
        animation.complete()
        if (grabbing)
            return
        grabbing = true
        const dpr = Screen.devicePixelRatio
        list.grabToImage(result => {
            // The url is only good while the result lives.
            grab = result
            snapshot.source = result.url
            // Setting the source can load the image and run onStatusChanged's start() already.
            if (grabbing && snapshot.status === Image.Ready)
                start()
        }, Qt.size(list.width * dpr, list.height * dpr))
    }

    // Only once the snapshot can be drawn: jumping before it is would leave a frame with neither
    // the old view nor the new rows, a blink.
    function start() {
        grabbing = false
        const from = list.contentY
        const bar = list.scrollBar ?? list.ScrollBar.vertical
        const handleY = bar?.contentItem?.y ?? 0
        root.change()
        if (forcedDirection === 0 && list.contentY === from) {
            snapshot.source = ""
            grab = null
            return
        }
        const direction = forcedDirection !== 0 ? forcedDirection : list.contentY > from ? 1 : -1
        // The real scroll bar stays visible beside the snapshot and its handle glides; in the
        // snapshot it would fade over itself and the track would brighten where they overlap.
        snapshotFrame.width = bar?.visible ? bar.x : list.width
        bar?.glide(handleY, root.slide)
        snapshotShift.to = -direction * Theme.settingsWindow.pageOffset
        rowsShift.from = direction * Theme.settingsWindow.pageOffset
        animation.restart()
    }

    // Shifts the rows, not the list, so they stay clipped inside it.
    Translate { id: rowsTranslate }
    Component.onCompleted: list.contentItem.transform = [rowsTranslate]

    Item {
        id: snapshotFrame
        height: root.height
        clip: true
        visible: animation.running
        transform: Translate { id: snapshotTranslate }

        Image {
            id: snapshot
            width: root.width
            height: root.height
            cache: false
            onStatusChanged: {
                if (!root.grabbing || root.grab === null)
                    return
                if (status === Image.Ready) {
                    root.start()
                } else if (status === Image.Error) {
                    root.grabbing = false
                    root.grab = null
                    root.change()
                }
            }
        }
    }

    // The fades are linear and run together, so the two opacities always add up to 1.
    ParallelAnimation {
        id: animation
        onFinished: {
            snapshot.source = ""
            root.grab = null
        }
        NumberAnimation { target: snapshot; property: "opacity"; from: 1; to: 0; duration: root.fade }
        NumberAnimation {
            id: snapshotShift
            target: snapshotTranslate
            property: "y"
            from: 0
            duration: root.slide
            easing.type: Motion.easingType
            easing.bezierCurve: Motion.easingCurve
        }
        NumberAnimation { target: root.list.contentItem; property: "opacity"; from: 0; to: 1; duration: root.fade }
        NumberAnimation {
            id: rowsShift
            target: rowsTranslate
            property: "y"
            to: 0
            duration: root.slide
            easing.type: Motion.easingType
            easing.bezierCurve: Motion.easingCurve
        }
    }
}
