import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtMultimedia

// See docs/code/qml.md.
MotionPopup {
    id: root

    property var current: ({})
    property string token: ""
    property string file: ""
    property string error: ""
    property real volume: 1
    property bool muted: false
    property bool ended: false
    // Fullscreen the viewer turned on is turned off again when it closes; one the window already had stays.
    property bool madeFullscreen: false
    readonly property bool preparing: token !== "" && file === "" && error === ""
    readonly property bool needsFfmpeg: !FfmpegTool.available
    readonly property bool playing: player.playbackState === MediaPlayer.PlayingState
    readonly property var window: scope.Window.window
    readonly property bool fullscreen: window !== null && window.visibility === Window.FullScreen

    signal message(string text)

    function show(c) {
        current = c
        error = ""
        volume = General.clipVolume
        open()
        prepare()
    }

    function prepare() {
        discard()
        if (!FfmpegTool.available || current.videoDir === undefined)
            return
        token = current.id + "-" + Date.now()
        Library.prepareClip(token, current.videoDir, FfmpegTool.path)
    }

    // The temp file goes with the viewer, as soon as the player lets go of it.
    function discard() {
        player.stop()
        player.source = ""
        ended = false
        Library.discardClip(file)
        file = ""
        token = ""
    }

    function toggle() {
        if (ended) {
            ended = false
            player.position = 0
            player.play()
        } else if (playing) {
            player.pause()
        } else {
            player.play()
        }
    }

    function seekBy(ms) {
        ended = false
        player.position = Math.max(0, Math.min(player.duration, player.position + ms))
    }

    function setVolume(v) {
        volume = Math.round(Math.max(0, Math.min(1, v)) * 100) / 100
        muted = false
        saveVolume.restart()
    }

    function toggleMute() {
        if (muted || volume === 0) {
            muted = false
            if (volume === 0)
                setVolume(0.5)
        } else {
            muted = true
        }
    }

    // Main.qml's toggleFullscreen() remembers whether the window was maximized.
    function toggleFullscreen() {
        if (window === null)
            return
        madeFullscreen = !fullscreen
        window.toggleFullscreen()
    }

    function leave() {
        if (madeFullscreen && fullscreen)
            toggleFullscreen()
        else
            close()
    }

    function timeText(ms) {
        const total = Math.floor(Math.max(0, ms) / 1000)
        const s = String(total % 60).padStart(2, "0")
        const m = Math.floor(total / 60) % 60
        const h = Math.floor(total / 3600)
        return h > 0 ? h + ":" + String(m).padStart(2, "0") + ":" + s : m + ":" + s
    }

    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent.width
    height: parent.height
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.NoAutoClose
    Overlay.modal: Rectangle {
        color: Theme.media.viewerScrim

        Behavior on opacity {
            id: fade
            enabled: Motion.fades
            MotionNumber { fade: true; speed: fade.targetValue > 0 ? "normal" : "fast"; linear: true }
        }
    }
    background: null

    onClosed: {
        discard()
        if (saveVolume.running) {
            saveVolume.stop()
            saveVolume.triggered()
        }
        if (madeFullscreen && fullscreen)
            window.toggleFullscreen()
        madeFullscreen = false
    }

    // Writing settings.toml on every step of a drag would be wasteful.
    Timer {
        id: saveVolume
        interval: 400
        onTriggered: {
            const failure = General.setClipVolume(root.volume)
            if (failure !== "")
                root.message(failure)
        }
    }

    Connections {
        target: Library
        function onClipPrepared(done, path, failure) {
            if (done !== root.token)
                return
            if (failure !== "") {
                root.error = failure
                return
            }
            root.file = path
            player.source = "file://" + path
            player.play()
        }
    }
    Connections {
        target: FfmpegTool
        function onChanged() {
            if (root.opened && FfmpegTool.available && root.token === "" && root.error === "")
                root.prepare()
        }
    }

    MediaPlayer {
        id: player
        videoOutput: video
        audioOutput: AudioOutput {
            volume: root.volume
            muted: root.muted
        }
        onErrorOccurred: (code, text) => root.error = text
        // At the end Qt clears the picture, so playback stops just short of it to keep the last frame.
        onPositionChanged: {
            if (playbackState === MediaPlayer.PlayingState && duration > 0 && position >= duration - 100) {
                root.ended = true
                pause()
            }
        }
        // A jump to the very end can reach it anyway; seeking back while paused shows the frame again,
        // after a blink.
        onMediaStatusChanged: {
            if (mediaStatus !== MediaPlayer.EndOfMedia)
                return
            root.ended = true
            pause()
            position = Math.max(0, duration - 50)
        }
    }

    ClipMenu {
        id: clipMenu
        media: root.current
        onMessage: text => root.message(text)
    }

    component ViewerButton: IconButton {
        plain: true
        implicitWidth: Theme.media.viewerButtonSize
        implicitHeight: Theme.media.viewerButtonSize
        radius: height / 2
        iconSize: Theme.media.viewerIconSize
        iconColor: Theme.media.viewerControlColor
        hoverTint: Theme.media.viewerButtonHover
    }

    component Bar: Item {
        id: bar

        property real fraction
        signal moved(real fraction)

        implicitHeight: Math.max(Theme.media.seekHeight, Theme.media.seekKnobSize) + 2 * Theme.layout.spacing

        Rectangle {
            id: track
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: Theme.media.seekHeight
            radius: Theme.radius.tiny
            color: Theme.media.viewerControlColor
            opacity: 0.25
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: track.width * bar.fraction
            height: track.height
            radius: track.radius
            color: Theme.media.viewerControlColor
        }
        Rectangle {
            readonly property bool shown: area.containsMouse || area.pressed
            visible: Theme.media.seekKnobSize > 0
            anchors.verticalCenter: parent.verticalCenter
            x: track.width * bar.fraction - width / 2
            width: Theme.media.seekKnobSize
            height: width
            radius: width / 2
            color: Theme.media.viewerControlColor
            opacity: shown ? 1 : 0
            scale: shown ? 1 : 0.5
            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
            Behavior on scale { enabled: Motion.on; MotionNumber {} }
        }
        MouseArea {
            id: area
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            function at(x) { bar.moved(Math.max(0, Math.min(1, x / width))) }
            onPressed: mouse => at(mouse.x)
            onPositionChanged: mouse => {
                if (pressed)
                    at(mouse.x)
            }
        }
    }

    contentItem: FocusScope {
        id: scope
        focus: true

        Keys.onSpacePressed: root.toggle()
        Keys.onLeftPressed: root.seekBy(-5000)
        Keys.onRightPressed: root.seekBy(5000)
        Keys.onUpPressed: root.setVolume((root.muted ? 0 : root.volume) + 0.05)
        Keys.onDownPressed: root.setVolume((root.muted ? 0 : root.volume) - 0.05)
        Keys.onEscapePressed: root.leave()
        Keys.onPressed: event => {
            if (event.key === Qt.Key_M)
                root.toggleMute()
            else if (event.key === Qt.Key_F)
                root.toggleFullscreen()
            else
                return
            event.accepted = true
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: mouse => {
                if (mouse.button === Qt.RightButton)
                    clipMenu.popup(this, mouse.x, mouse.y)
                else
                    root.close()
            }
        }

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: Theme.media.viewerMargin
            anchors.topMargin: Theme.media.viewerTopMargin
            anchors.bottomMargin: Theme.media.viewerBottomMargin
            spacing: Theme.layout.spacing

            Item {
                id: videoArea
                Layout.fillWidth: true
                Layout.fillHeight: true

                VideoOutput {
                    id: video
                    anchors.fill: parent
                    fillMode: VideoOutput.PreserveAspectFit
                    visible: root.file !== ""
                }
                MouseArea {
                    x: video.contentRect.x
                    y: video.contentRect.y
                    width: video.contentRect.width
                    height: video.contentRect.height
                    enabled: root.file !== ""
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: mouse => {
                        if (mouse.button === Qt.RightButton)
                            clipMenu.popup(this, mouse.x, mouse.y)
                        else
                            root.toggle()
                    }
                    onDoubleClicked: mouse => {
                        if (mouse.button === Qt.LeftButton)
                            root.toggleFullscreen()
                    }
                }

                Surface {
                    visible: root.file === ""
                    anchors.centerIn: parent
                    width: Math.min(parent.width, Theme.gameEditor.confirmWidth)
                    height: message.implicitHeight + 2 * Theme.gameEditor.padding
                    radius: Theme.gameEditor.radius
                    fill: Theme.gameEditor.background
                    fillEnd: Theme.gameEditor.backgroundEnd
                    direction: Theme.gameEditor.gradientDirection
                    border.color: Theme.gameEditor.border
                    border.width: Theme.gameEditor.borderWidth

                    ColumnLayout {
                        id: message
                        x: Theme.gameEditor.padding
                        y: Theme.gameEditor.padding
                        width: parent.width - 2 * Theme.gameEditor.padding

                        FfmpegPrompt {
                            visible: root.needsFfmpeg
                            Layout.fillWidth: true
                            textColor: Theme.gameEditor.labelColor
                            showNotNow: false
                        }
                        Label {
                            visible: !root.needsFfmpeg
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: Theme.gameEditor.labelColor
                            text: root.error !== "" ? root.error : qsTr("Preparing the clip")
                        }
                    }
                }
            }

            // As wide as the picture, not the window, so the controls line up under it.
            ColumnLayout {
                visible: root.file !== ""
                Layout.fillWidth: true
                Layout.maximumWidth: video.contentRect.width > 0 ? video.contentRect.width : Number.POSITIVE_INFINITY
                Layout.alignment: Qt.AlignHCenter
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.layout.spacing * 2

                    Label {
                        text: root.timeText(root.ended ? player.duration : player.position)
                        color: Theme.media.viewerControlColor
                    }
                    Bar {
                        Layout.fillWidth: true
                        fraction: player.duration > 0 ? Math.min(1, player.position / player.duration) : 0
                        onMoved: f => {
                            root.ended = false
                            if (player.duration > 0)
                                player.position = f * player.duration
                        }
                    }
                    Label {
                        text: root.timeText(player.duration)
                        color: Theme.media.viewerControlColor
                    }
                }

                Item {
                    Layout.fillWidth: true
                    implicitHeight: Math.max(Theme.media.viewerPlaySize, Theme.media.viewerButtonSize)

                    RowLayout {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.layout.spacing

                        ViewerButton {
                            readonly property bool silent: root.muted || root.volume === 0
                            icon: silent ? Icons.volumeMuteFill : root.volume < 0.5 ? Icons.volumeDownFill : Icons.volumeUpFill
                            tip: silent ? qsTr("Unmute (M)") : qsTr("Mute (M)")
                            onClicked: root.toggleMute()
                        }
                        Bar {
                            Layout.preferredWidth: Theme.media.viewerVolumeWidth
                            fraction: root.muted ? 0 : root.volume
                            onMoved: f => root.setVolume(f)

                            WheelHandler {
                                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                onWheel: event => root.setVolume((root.muted ? 0 : root.volume) + (event.angleDelta.y > 0 ? 0.05 : -0.05))
                            }
                        }
                    }

                    Row {
                        anchors.centerIn: parent
                        spacing: Theme.media.viewerControlsGap

                        ViewerButton {
                            anchors.verticalCenter: parent.verticalCenter
                            icon: Icons.replay5Fill
                            tip: qsTr("Back 5 seconds (Left)")
                            onClicked: root.seekBy(-5000)
                        }
                        IconButton {
                            anchors.verticalCenter: parent.verticalCenter
                            implicitWidth: Theme.media.viewerPlaySize
                            implicitHeight: Theme.media.viewerPlaySize
                            radius: height / 2
                            fill: Theme.media.viewerPlayBackground
                            fillEnd: Theme.media.viewerPlayBackground
                            border.width: 0
                            icon: root.playing ? Icons.pauseFill : Icons.playFill
                            iconSize: Theme.media.viewerPlayIconSize
                            iconColor: Theme.media.viewerPlayIconColor
                            tip: root.playing ? qsTr("Pause (Space)") : qsTr("Play (Space)")
                            onClicked: root.toggle()
                        }
                        ViewerButton {
                            anchors.verticalCenter: parent.verticalCenter
                            icon: Icons.forward5Fill
                            tip: qsTr("Forward 5 seconds (Right)")
                            onClicked: root.seekBy(5000)
                        }
                    }

                    ViewerButton {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        icon: root.fullscreen ? Icons.fullscreenExitLine : Icons.fullscreenLine
                        tip: root.fullscreen ? qsTr("Exit fullscreen (F)") : qsTr("Fullscreen (F)")
                        onClicked: root.toggleFullscreen()
                    }
                }
            }
        }

        // Beside the picture's top right corner, in the room a 16:9 clip leaves at the sides, and
        // kept below the window's own buttons, which the scrim dims but doesn't hide.
        ViewerButton {
            readonly property real pictureRight: column.x + videoArea.x + video.contentRect.x + video.contentRect.width
            readonly property real pictureTop: column.y + videoArea.y + video.contentRect.y
            readonly property real belowWindowButtons: root.fullscreen ? 0
                : Theme.windowControls.margin + Theme.windowControls.size + Theme.media.viewerCloseGap
            x: Math.min(parent.width - width - Theme.media.viewerCloseGap, pictureRight + Theme.media.viewerCloseGap)
            y: Math.max(pictureTop, belowWindowButtons)
            icon: Icons.close
            tip: qsTr("Close (Esc)")
            onClicked: root.close()
        }
    }
}
