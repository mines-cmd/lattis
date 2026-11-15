import QtQuick

// One place in TopNotices' deck: where a notice sits for its depth, folded or fanned out, and the
// motion between them. The notice goes inside and fills it.
Item {
    id: root

    // -1: not showing.
    property int depth: -1
    property bool fanned: false
    // No motion before the first frame.
    property bool animated: false
    default property alias content: holder.data

    readonly property bool present: depth >= 0
    // A notice that was dismissed fades out where it was, not at the front.
    property int shownDepth: 0
    readonly property int foldedDepth: Math.min(shownDepth, Theme.notice.peekCount)
    // Fan-out moves the one behind after the one in front.
    readonly property int stagger: fanned ? Motion.duration(Theme.notice.fanStagger) * shownDepth : 0

    // Arrival and dismissal are a short slide on top of the deck's own places.
    property real rise: 0
    property bool snapping: false
    property real rowY: fanned ? shownDepth * (Theme.layout.toolbarHeight + Theme.layout.spacing)
                               : foldedDepth * Theme.notice.peekOffset

    onDepthChanged: if (depth >= 0) shownDepth = depth
    Component.onCompleted: if (depth >= 0) shownDepth = depth
    onPresentChanged: {
        if (!animated)
            return
        if (present) {
            if (opacity === 0) {
                snapping = true
                rise = Motion.on ? -Theme.notice.enterOffset : 0
                snapping = false
            }
            rise = 0
        } else {
            rise = Motion.on ? -Theme.notice.leaveOffset : 0
        }
    }

    enabled: present
    visible: opacity > 0
    z: -shownDepth
    y: rowY + rise
    scale: fanned ? 1 : Math.pow(Theme.notice.peekScale, foldedDepth)
    transformOrigin: Item.Top
    opacity: !present ? 0
             : fanned ? 1
             : shownDepth > Theme.notice.peekCount ? 0
             : Math.pow(Theme.notice.peekOpacity, shownDepth)

    Behavior on rowY {
        enabled: Motion.on && root.animated
        SequentialAnimation {
            PauseAnimation { duration: root.stagger }
            MotionNumber { speed: "fast" }
        }
    }
    Behavior on rise {
        enabled: Motion.on && root.animated && !root.snapping
        MotionNumber { speed: "fast"; duration: Motion.duration(root.present ? Motion.speedMs("fast") : Theme.notice.dismissDuration) }
    }
    Behavior on scale {
        enabled: Motion.on && root.animated
        SequentialAnimation {
            PauseAnimation { duration: root.stagger }
            MotionNumber { speed: "fast" }
        }
    }
    Behavior on opacity {
        enabled: Motion.fades && root.animated
        SequentialAnimation {
            PauseAnimation { duration: root.present ? root.stagger : 0 }
            MotionNumber {
                speed: "fast"
                fade: true
                duration: Motion.fadeDuration(root.present ? Motion.speedMs("fast") : Theme.notice.dismissDuration)
            }
        }
    }

    Item {
        id: holder
        anchors.fill: parent
    }
}
