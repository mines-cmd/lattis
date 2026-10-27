import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtMultimedia

// See docs/code/qml.md.
MotionPopup {
    id: root

    property var current: ({})
    property var list: []
    // The Gallery's items carry gameId and share ids across games, so both identify one.
    readonly property int index: list.findIndex(i => i.id === current.id && i.gameId === current.gameId)
    // Game info by id ({title, platformName, platformIcon}); with it, items that carry a gameId get a line naming their game.
    property var games: ({})
    readonly property var game: games[current.gameId ?? ""]
    readonly property bool isClip: current.kind === "clip"
    readonly property rect picture: isClip ? (pictureKnown || lastPicture.width <= 0 ? video.contentRect : lastPicture)
                                           : Qt.rect((shot.width - shot.paintedWidth) / 2, (shot.height - shot.paintedHeight) / 2,
                                                     shot.paintedWidth, shot.paintedHeight)
    // Until the first frame is drawn contentRect is the whole item, even after hasVideo turns true.
    // A picture exactly the item's shape is trusted once playback has moved on a little.
    readonly property bool pictureKnown: file !== "" && player.hasVideo
                                         && (video.contentRect.width < video.width - 0.5
                                             || video.contentRect.height < video.height - 0.5
                                             || player.position > 200)
    property rect lastPicture: Qt.rect(0, 0, 0, 0)
    property string token: ""
    property string file: ""
    property string error: ""
    property real volume: 1
    property bool muted: false
    property bool ended: false
    // A screenshot's zoom, and how far its centre is moved from the middle of the area.
    readonly property real maxZoom: 8
    property real zoom: 1
    property real panX: 0
    property real panY: 0
    // Fullscreen the viewer turned on is turned off again when it closes; one the window already had stays.
    property bool madeFullscreen: false
    readonly property bool preparing: token !== "" && file === "" && error === ""
    readonly property bool needsFfmpeg: !FfmpegTool.available
    readonly property bool playing: player.playbackState === MediaPlayer.PlayingState
    readonly property var window: scope.Window.window
    readonly property bool fullscreen: window !== null && window.visibility === Window.FullScreen

    signal message(string text)
    signal gameRequested(string id)

    function show(item, items) {
        list = items
        current = item
        error = ""
        resetZoom()
        volume = General.clipVolume
        open()
        prepare()
    }

    function step(by) {
        const next = list[index + by]
        if (next === undefined)
            return
        current = next
        error = ""
        resetZoom()
        prepare()
    }

    function resetZoom() {
        zoom = 1
        panX = 0
        panY = 0
    }

    // Keeps the point at (px, py) of the area where it is.
    function zoomTo(value, px, py) {
        const next = Math.max(1, Math.min(maxZoom, value))
        const cx = shot.width / 2
        const cy = shot.height / 2
        const ux = (px - cx - panX) / zoom
        const uy = (py - cy - panY) / zoom
        zoom = next
        panBy(px - cx - ux * next - panX, py - cy - uy * next - panY)
    }

    function panBy(dx, dy) {
        const mx = Math.max(0, (shot.paintedWidth * zoom - shot.width) / 2)
        const my = Math.max(0, (shot.paintedHeight * zoom - shot.height) / 2)
        panX = Math.max(-mx, Math.min(mx, panX + dx))
        panY = Math.max(-my, Math.min(my, panY + dy))
    }

    function prepare() {
        discard()
        if (!isClip || !FfmpegTool.available)
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

    function goToGame(id) {
        close()
        gameRequested(id)
    }

    function dateText(time) {
        return time.toLocaleDateString(Qt.locale(), Locale.LongFormat) + ", " + time.toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
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
            if (root.opened && root.isClip && FfmpegTool.available && root.token === "" && root.error === "")
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
        gameId: root.game !== undefined ? root.current.gameId : ""
        onGameRequested: id => root.goToGame(id)
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
        property color fill: Theme.media.seekFill
        signal moved(real fraction)

        implicitHeight: Math.max(Theme.media.seekHeight, Theme.media.seekKnobSize) + 2 * Theme.layout.spacing

        Rectangle {
            id: track
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: Theme.media.seekHeight
            radius: Theme.radius.tiny
            color: Theme.media.seekTrack
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: track.width * bar.fraction
            height: track.height
            radius: track.radius
            color: bar.fill
        }
        Rectangle {
            readonly property bool shown: area.containsMouse || area.pressed
            visible: Theme.media.seekKnobSize > 0
            anchors.verticalCenter: parent.verticalCenter
            x: track.width * bar.fraction - width / 2
            width: Theme.media.seekKnobSize
            height: width
            radius: width / 2
            color: Theme.media.seekKnob
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

        Keys.onSpacePressed: {
            if (root.isClip)
                root.toggle()
        }
        // On a clip the arrows seek, so Page Up and Page Down step there.
        Keys.onLeftPressed: root.isClip ? root.seekBy(-5000) : root.step(-1)
        Keys.onRightPressed: root.isClip ? root.seekBy(5000) : root.step(1)
        Keys.onUpPressed: root.setVolume((root.muted ? 0 : root.volume) + 0.05)
        Keys.onDownPressed: root.setVolume((root.muted ? 0 : root.volume) - 0.05)
        Keys.onEscapePressed: root.leave()
        Keys.onPressed: event => {
            if (event.key === Qt.Key_M)
                root.toggleMute()
            else if (event.key === Qt.Key_F)
                root.toggleFullscreen()
            else if (!root.isClip && (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal))
                root.zoomTo(root.zoom * 1.25, shot.width / 2, shot.height / 2)
            else if (!root.isClip && event.key === Qt.Key_Minus)
                root.zoomTo(root.zoom / 1.25, shot.width / 2, shot.height / 2)
            else if (!root.isClip && event.key === Qt.Key_0)
                root.resetZoom()
            else if (event.key === Qt.Key_PageUp)
                root.step(-1)
            else if (event.key === Qt.Key_PageDown)
                root.step(1)
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

        // A click that just misses the clip controls must not close the viewer.
        MouseArea {
            readonly property real pad: Theme.layout.spacing * 4
            visible: root.isClip
            x: column.x + clipControls.x - pad
            y: column.y + clipControls.y - pad
            width: clipControls.width + 2 * pad
            height: clipControls.height + 2 * pad
            acceptedButtons: Qt.LeftButton | Qt.RightButton
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
                clip: true

                VideoOutput {
                    id: video
                    anchors.fill: parent
                    fillMode: VideoOutput.PreserveAspectFit
                    visible: root.file !== ""
                    onContentRectChanged: {
                        if (root.pictureKnown && contentRect.width > 0)
                            root.lastPicture = contentRect
                    }
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

                Image {
                    id: shot
                    width: parent.width
                    height: parent.height
                    x: root.panX
                    y: root.panY
                    scale: root.zoom
                    visible: !root.isClip
                    source: root.isClip ? "" : (root.current.image ?? "")
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    // Decoded at the size it's shown, so a 4K screenshot doesn't hold its full size in memory;
                    // zoomed in, at its own.
                    sourceSize.width: root.zoom > 1 ? 0 : width
                    sourceSize.height: root.zoom > 1 ? 0 : height
                }
                MouseArea {
                    id: shotArea
                    property point last
                    readonly property bool zoomed: root.zoom > 1
                    x: zoomed ? 0 : root.picture.x
                    y: zoomed ? 0 : root.picture.y
                    width: zoomed ? parent.width : root.picture.width
                    height: zoomed ? parent.height : root.picture.height
                    enabled: !root.isClip
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: !zoomed ? Qt.ArrowCursor : pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    onPressed: mouse => last = Qt.point(mouse.x, mouse.y)
                    onPositionChanged: mouse => {
                        if (pressed && zoomed) {
                            root.panBy(mouse.x - last.x, mouse.y - last.y)
                            last = Qt.point(mouse.x, mouse.y)
                        }
                    }
                    WheelHandler {
                        onWheel: event => {
                            const at = shotArea.mapToItem(shot.parent, point.position.x, point.position.y)
                            root.zoomTo(root.zoom * Math.pow(1.2, event.angleDelta.y / 120), at.x, at.y)
                        }
                    }
                    onClicked: mouse => {
                        if (mouse.button === Qt.RightButton)
                            clipMenu.popup(this, mouse.x, mouse.y)
                    }
                    onDoubleClicked: mouse => {
                        if (mouse.button === Qt.LeftButton)
                            root.toggleFullscreen()
                    }
                }

                Icon {
                    id: spinner
                    anchors.centerIn: parent
                    visible: root.isClip && (root.preparing || (root.file !== "" && !root.pictureKnown))
                    path: Icons.loader
                    size: Theme.media.viewerPlaySize
                    color: Theme.media.viewerControlColor
                    RotationAnimation on rotation {
                        running: spinner.visible
                        from: 0
                        to: 360
                        duration: Theme.iconButton.spinDuration
                        loops: Animation.Infinite
                    }
                }

                Surface {
                    visible: root.isClip ? root.file === "" && (root.needsFfmpeg || root.error !== "") : shot.status === Image.Error
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
                            visible: root.isClip && root.needsFfmpeg
                            Layout.fillWidth: true
                            textColor: Theme.gameEditor.labelColor
                            showNotNow: false
                        }
                        Label {
                            visible: !root.isClip || !root.needsFfmpeg
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: Theme.gameEditor.labelColor
                            text: !root.isClip ? qsTr("Couldn't open the screenshot")
                                               : root.error
                        }
                    }
                }
            }

            Item {
                visible: !root.isClip && shot.status !== Image.Error
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: zoomRow.implicitWidth
                implicitHeight: zoomRow.implicitHeight

                // A click that just misses the controls must not close the viewer.
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -Theme.layout.spacing * 4
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                }

                RowLayout {
                    id: zoomRow
                    spacing: Theme.layout.spacing

                    ViewerButton {
                        icon: Icons.search
                        tip: qsTr("Reset zoom (0)")
                        onClicked: root.resetZoom()
                    }
                    Bar {
                        Layout.preferredWidth: Theme.media.viewerVolumeWidth * 2
                        fraction: (root.zoom - 1) / (root.maxZoom - 1)
                        onMoved: f => root.zoomTo(1 + f * (root.maxZoom - 1), shot.width / 2, shot.height / 2)
                    }
                    Label {
                        id: zoomLabel
                        Layout.preferredWidth: zoomMetrics.width
                        horizontalAlignment: Text.AlignRight
                        text: Math.round(root.zoom * 100) + "%"
                        color: Theme.media.viewerTimeColor
                        TextMetrics { id: zoomMetrics; font: zoomLabel.font; text: "800%" }
                    }
                }
            }

            // As wide as the picture, not the window, so the controls line up under it.
            ColumnLayout {
                id: clipControls
                // Kept in the layout while the next clip loads, so the picture's area doesn't change size.
                // Until its first frame the picture has no size, so the last clip's stands in.
                readonly property bool sized: root.pictureKnown || root.lastPicture.width > 0
                visible: root.isClip
                opacity: root.error === "" && !root.needsFfmpeg && sized ? 1 : 0
                enabled: root.file !== ""
                Layout.fillWidth: true
                Layout.maximumWidth: root.pictureKnown ? video.contentRect.width
                                     : root.lastPicture.width > 0 ? root.lastPicture.width : Number.POSITIVE_INFINITY
                Layout.alignment: Qt.AlignHCenter
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.layout.spacing * 2

                    Label {
                        text: root.timeText(root.ended ? player.duration : player.position)
                        color: Theme.media.viewerTimeColor
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
                        color: Theme.media.viewerTimeColor
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
                            fill: Theme.media.volumeFill
                            fraction: root.muted ? 0 : root.volume
                            onMoved: f => root.setVolume(f)

                            WheelHandler {
                                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                onWheel: event => root.setVolume((root.muted ? 0 : root.volume) + (event.angleDelta.y > 0 ? 0.05 : -0.05))
                            }
                        }
                        Label {
                            id: percentLabel
                            Layout.preferredWidth: percentMetrics.width
                            horizontalAlignment: Text.AlignRight
                            text: Math.round((root.muted ? 0 : root.volume) * 100) + "%"
                            color: Theme.media.viewerTimeColor
                            TextMetrics { id: percentMetrics; font: percentLabel.font; text: "100%" }
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

            Item {
                id: gameLine
                visible: root.game !== undefined
                Layout.alignment: Qt.AlignHCenter
                Layout.maximumWidth: parent.width
                implicitWidth: lineRow.implicitWidth
                implicitHeight: lineRow.implicitHeight

                // A click that just misses the line must not close the viewer.
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -Theme.layout.spacing * 4
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                }

                RowLayout {
                    id: lineRow
                    width: Math.min(implicitWidth, gameLine.Layout.maximumWidth)
                    spacing: Theme.layout.spacing * 1.5

                    Icon {
                        visible: (root.game?.platformIcon ?? "") !== ""
                        size: Theme.gallery.viewerLineSize + 4
                        path: Icons.forPlatform(root.game?.platformIcon ?? "")
                        color: Theme.gallery.viewerLineColor
                    }
                    Label {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        elide: Text.ElideRight
                        text: root.game?.title ?? ""
                        color: Theme.gallery.viewerLineColor
                        font.pixelSize: Theme.gallery.viewerLineSize
                        font.weight: Theme.font.emphasisWeight
                    }
                    Label {
                        text: "·  " + (root.current.time ? root.dateText(root.current.time) : "") + "  ·  "
                              + qsTr("%1 of %2").arg(root.index + 1).arg(root.list.length)
                        color: Theme.gallery.viewerLineMuted
                        font.pixelSize: Theme.gallery.viewerLineSize
                    }
                    Surface {
                        Layout.leftMargin: Theme.layout.spacing
                        implicitWidth: goLabel.implicitWidth + 2 * Theme.layout.spacing * 1.5
                        implicitHeight: goLabel.implicitHeight + Theme.layout.spacing
                        radius: height / 2
                        fill: goArea.containsMouse ? Theme.gallery.viewerGameHover : "transparent"
                        fillEnd: fill
                        border.color: Theme.gallery.viewerLineMuted
                        border.width: Theme.layout.borderWidth
                        Behavior on fill { enabled: Motion.fades; MotionColor {} }

                        Label {
                            id: goLabel
                            anchors.centerIn: parent
                            text: qsTr("Go to game")
                            color: Theme.gallery.viewerLineColor
                            font.pixelSize: Theme.gallery.viewerLineSize
                        }
                        MouseArea {
                            id: goArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.goToGame(root.current.gameId)
                        }
                    }
                }
            }
        }

        // Centred on the window, not the picture: its area grows while a clip prepares and on a screenshot,
        // which have no controls under them.
        ViewerButton {
            visible: root.index > 0
            x: Math.max(0, (column.x - width) / 2)
            y: (parent.height - height) / 2
            icon: Icons.chevronLeft
            tip: root.isClip ? qsTr("Previous (Page Up)") : qsTr("Previous (Left)")
            onClicked: root.step(-1)
        }
        ViewerButton {
            visible: root.index >= 0 && root.index < root.list.length - 1
            x: parent.width - width - Math.max(0, (column.x - width) / 2)
            y: (parent.height - height) / 2
            icon: Icons.chevronRight
            tip: root.isClip ? qsTr("Next (Page Down)") : qsTr("Next (Right)")
            onClicked: root.step(1)
        }

        // Beside the picture's top right corner, in the room a 16:9 picture leaves at the sides.
        ViewerButton {
            readonly property real pictureRight: column.x + videoArea.x + root.picture.x + root.picture.width
            readonly property real pictureTop: column.y + videoArea.y + root.picture.y
            visible: !root.isClip || root.pictureKnown || root.file === "" && !root.preparing
            x: Math.min(parent.width - width - Theme.media.viewerCloseGap, pictureRight + Theme.media.viewerCloseGap)
            y: pictureTop + Theme.media.viewerCloseTopGap
            icon: Icons.close
            iconColor: Theme.media.viewerCloseColor
            tip: qsTr("Close (Esc)")
            onClicked: root.close()
        }
    }
}
