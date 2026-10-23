import QtQuick

// An emulator install's bar: filled to `progress` (0 to 1), or a part sliding across while it's unknown (-1).
Rectangle {
    id: track

    property real progress: -1
    property color fillColor: Theme.settingsWindow.installProgressFill
    readonly property bool unknown: progress < 0

    implicitHeight: Theme.settingsWindow.installProgressHeight
    radius: Theme.settingsWindow.installProgressRadius
    color: Theme.settingsWindow.installProgressTrack
    clip: true

    Rectangle {
        id: fill
        height: parent.height
        radius: parent.radius
        color: track.fillColor
        width: track.unknown ? track.width / 3 : track.width * Math.min(1, track.progress)
        // Without motion the unknown part can't slide, so it fills the bar faintly instead.
        opacity: track.unknown && !Motion.on ? 0.5 : 1

        Binding on width {
            when: track.unknown && !Motion.on
            value: track.width
        }

        Behavior on width { enabled: Motion.on && !track.unknown; MotionNumber {} }

        NumberAnimation on x {
            running: track.unknown && Motion.on && track.visible
            loops: Animation.Infinite
            from: -track.width / 3
            to: track.width
            duration: Motion.duration(Theme.settingsWindow.installProgressSlideDuration)
            onRunningChanged: if (!running) fill.x = 0
        }
    }
}
