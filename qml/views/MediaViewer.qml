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
    // Game info by id ({title, platformName, platformIcon}); with it, items that carry a gameId get their game named in the top bar.
    property var games: ({})
    readonly property var game: games[current.gameId ?? ""]
    // Steam's clips are joined by ffmpeg first; a folder's video plays as it is.
    readonly property bool isSteamClip: current.kind === "clip"
    readonly property bool isClip: isSteamClip || current.kind === "video"
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
    // The bars sit on the picture, so they wait until it has a size instead of jumping when the first frame lands.
    readonly property bool settled: !isClip || pictureKnown || lastPicture.width > 0 || file === "" && !preparing
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
    property bool awake: true
    readonly property bool barsShown: awake || Theme.media.viewerHideDelay === 0 || topHover.hovered || bottomHover.hovered || zoomHover.hovered
                                      || clipMenu.visible || isClip && (!playing || file === "")
    readonly property bool preparing: token !== "" && file === "" && error === ""
    readonly property bool needsFfmpeg: isSteamClip && !FfmpegTool.available
    property bool tempFile: false
    readonly property bool playing: player.playbackState === MediaPlayer.PlayingState
    readonly property var window: scope.Window.window
    readonly property bool fullscreen: window !== null && window.visibility === Window.FullScreen

    signal message(string text)
    signal gameRequested(string id)
    signal clipGone()

    function show(item, items) {
        list = items
        current = item
        error = ""
        resetZoom()
        volume = General.clipVolume
        open()
        prepare()
        wake()
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

    function wake() {
        awake = true
        if (Theme.media.viewerHideDelay > 0)
            idle.restart()
    }

    function flash(icon, text) {
        flashSign.show(icon, text ?? "")
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
        if (current.kind === "video") {
            file = current.file
            player.source = file
            player.play()
            return
        }
        if (!isSteamClip || !FfmpegTool.available)
            return
        token = current.id + "-" + Date.now()
        Library.prepareClip(token, current.videoDir, FfmpegTool.path)
    }

    // The temp file goes with the viewer, as soon as the player lets go of it.
    function discard() {
        player.stop()
        player.source = ""
        ended = false
        if (tempFile)
            Library.discardClip(file)
        tempFile = false
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

    function toggleWithFlash() {
        const pausing = playing && !ended
        toggle()
        flash(pausing ? Icons.pauseFill : Icons.playFill)
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

    function nudgeVolume(by) {
        setVolume((muted ? 0 : volume) + by)
        flashVolume()
    }

    function flashVolume() {
        const level = muted ? 0 : volume
        flash(volumeIcon(), Math.round(level * 100) + "%")
    }

    function volumeIcon() {
        return muted || volume === 0 ? Icons.volumeMuteFill : volume < 0.5 ? Icons.volumeDownFill : Icons.volumeUpFill
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
        // Without the system locale the app is en_US, whose 12 h clock isn't wanted.
        const clock = General.systemLocale ? time.toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
                                           : time.toLocaleTimeString(Qt.locale(), "HH:mm")
        return time.toLocaleDateString(Qt.locale(), Locale.LongFormat) + ", " + clock
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

    Timer {
        id: idle
        interval: Theme.media.viewerHideDelay
        onTriggered: root.awake = false
    }

    Connections {
        target: Library
        function onClipPrepared(done, path, failure) {
            if (done !== root.token)
                return
            if (failure !== "") {
                root.error = failure
                if (!Library.clipExists(root.current.videoDir ?? ""))
                    root.clipGone()
                return
            }
            root.file = path
            root.tempFile = true
            player.source = "file://" + path
            player.play()
        }
    }
    Connections {
        target: FfmpegTool
        function onChanged() {
            if (root.opened && root.isSteamClip && FfmpegTool.available && root.token === "" && root.error === "")
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
        onPlayingChanged: root.wake()
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
        property bool preview: false
        property string tipText: ""
        readonly property bool active: area.containsMouse || area.pressed
        readonly property real hoverFraction: width > 0 ? Math.max(0, Math.min(1, area.mouseX / width)) : 0
        signal moved(real fraction)

        implicitHeight: Math.max(Theme.media.seekHoverHeight, Theme.media.seekKnobSize) + 2 * Theme.layout.spacing

        Rectangle {
            id: track
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: bar.active ? Theme.media.seekHoverHeight : Theme.media.seekHeight
            radius: Math.min(Theme.radius.tiny, height / 2)
            color: Theme.media.seekTrack
            Behavior on height { enabled: Motion.on; MotionNumber {} }
        }
        Rectangle {
            visible: bar.preview && area.containsMouse
            anchors.verticalCenter: parent.verticalCenter
            width: track.width * bar.hoverFraction
            height: track.height
            radius: track.radius
            color: Theme.media.seekHoverFill
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: track.width * bar.fraction
            height: track.height
            radius: track.radius
            color: bar.fill
        }
        Rectangle {
            visible: Theme.media.seekKnobSize > 0
            anchors.verticalCenter: parent.verticalCenter
            x: track.width * bar.fraction - width / 2
            width: Theme.media.seekKnobSize
            height: width
            radius: width / 2
            color: Theme.media.seekKnob
            opacity: bar.active ? 1 : 0
            scale: bar.active ? 1 : 0.5
            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
            Behavior on scale { enabled: Motion.on; MotionNumber {} }
        }
        Rectangle {
            visible: bar.tipText !== "" && area.containsMouse
            x: Math.max(0, Math.min(bar.width - width, area.mouseX - width / 2))
            y: -height
            width: tipLabel.implicitWidth + Theme.layout.spacing * 1.5
            height: tipLabel.implicitHeight + Theme.layout.spacing / 2
            radius: Theme.media.seekTipRadius
            color: Theme.media.seekTipBackground

            Label {
                id: tipLabel
                anchors.centerIn: parent
                text: bar.tipText
                color: Theme.media.seekTipText
                font.features: { "tnum": 1 }
            }
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

    // One shape per group of controls; a single button makes it a circle.
    component ControlPill: Rectangle {
        default property alias content: pillRow.data
        property real padding: 0
        implicitWidth: pillRow.implicitWidth + 2 * padding
        implicitHeight: Theme.media.viewerButtonSize
        radius: Math.min(Theme.media.viewerPillRadius, height / 2)
        color: Theme.media.viewerPillBackground

        Row {
            id: pillRow
            x: parent.padding
            height: parent.height
        }
    }

    component EdgeShade: Rectangle {
        property bool fromTop: true
        readonly property color edge: Theme.media.viewerBarScrim
        gradient: Gradient {
            GradientStop { position: 0; color: fromTop ? edge : Qt.rgba(edge.r, edge.g, edge.b, 0) }
            GradientStop { position: 1; color: fromTop ? Qt.rgba(edge.r, edge.g, edge.b, 0) : edge }
        }
    }

    contentItem: FocusScope {
        id: scope
        focus: true

        Keys.onSpacePressed: {
            if (root.isClip)
                root.toggleWithFlash()
        }
        // On a clip the arrows seek, so Page Up and Page Down step there.
        Keys.onLeftPressed: {
            if (root.isClip) {
                root.seekBy(-5000)
                root.flash(Icons.replay5Line)
            } else {
                root.step(-1)
            }
        }
        Keys.onRightPressed: {
            if (root.isClip) {
                root.seekBy(5000)
                root.flash(Icons.forward5Line)
            } else {
                root.step(1)
            }
        }
        Keys.onUpPressed: root.nudgeVolume(0.05)
        Keys.onDownPressed: root.nudgeVolume(-0.05)
        Keys.onEscapePressed: root.leave()
        // Runs before the handlers for single keys above, so every key brings the bars back.
        Keys.onPressed: event => {
            root.wake()
            if (event.key === Qt.Key_M) {
                root.toggleMute()
                root.flashVolume()
            } else if (event.key === Qt.Key_F)
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
            // Set, so leaving the video brings back the pointer it hid.
            cursorShape: Qt.ArrowCursor
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
                    // A screenshot leaves the video empty, and an empty one's contentRect is the whole area:
                    // its hidden pointer would cover the margins beside the picture.
                    visible: root.isClip && root.file !== ""
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: root.barsShown ? Qt.ArrowCursor : Qt.BlankCursor
                    onClicked: mouse => {
                        if (mouse.button === Qt.RightButton)
                            clipMenu.popup(this, mouse.x, mouse.y)
                        else
                            root.toggleWithFlash()
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
                    size: Theme.media.viewerButtonSize
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
                    visible: root.isClip ? root.error !== "" || root.file === "" && root.needsFfmpeg : shot.status === Image.Error
                    anchors.centerIn: parent
                    width: Math.min(parent.width, Theme.gameEditor.confirmWidth)
                    height: message.implicitHeight + 2 * Theme.gameEditor.rowSpacing
                    radius: Theme.gameEditor.radius
                    fill: Theme.gameEditor.background
                    fillEnd: Theme.gameEditor.backgroundEnd
                    direction: Theme.gameEditor.gradientDirection
                    border.color: Theme.gameEditor.border
                    border.width: Theme.gameEditor.borderWidth

                    ColumnLayout {
                        id: message
                        x: Theme.gameEditor.rowSpacing
                        y: Theme.gameEditor.rowSpacing
                        width: parent.width - 2 * Theme.gameEditor.rowSpacing

                        FfmpegPrompt {
                            visible: root.needsFfmpeg
                            Layout.fillWidth: true
                            textColor: Theme.gameEditor.labelColor
                            onNotNow: root.close()
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

                // Laid over the picture; with no picture to show (an error, a screenshot that won't load),
                // over the whole area, so the close button is still there.
                Item {
                    id: bars
                    readonly property bool hasPicture: root.picture.width > 1 && root.picture.height > 1
                    readonly property real pad: Theme.media.viewerBarPadding
                    x: hasPicture ? root.picture.x : 0
                    y: hasPicture ? root.picture.y : 0
                    width: hasPicture ? root.picture.width : parent.width
                    height: hasPicture ? root.picture.height : parent.height
                    visible: root.settled
                    opacity: root.barsShown ? 1 : 0

                    // Only movement over the picture wakes the bars, and leaving it hides them at once.
                    HoverHandler {
                        id: pictureHover
                        onPointChanged: {
                            if (hovered)
                                root.wake()
                        }
                        onHoveredChanged: {
                            if (!hovered)
                                root.awake = false
                        }
                    }

                    Behavior on opacity {
                        id: barsFade
                        enabled: Motion.fades
                        MotionNumber { fade: true; speed: barsFade.targetValue > 0 ? "fast" : "normal" }
                    }

                    EdgeShade {
                        width: parent.width
                        height: topBar.height + 2 * bars.pad + Theme.media.viewerBarScrimReach
                    }
                    EdgeShade {
                        visible: bottomBar.visible
                        fromTop: false
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: bottomBar.height + 2 * bars.pad + Theme.media.viewerBarScrimReach
                    }

                    Item {
                        id: topBar
                        x: bars.pad
                        y: bars.pad
                        width: parent.width - 2 * bars.pad
                        implicitHeight: topRow.implicitHeight
                        height: implicitHeight

                        HoverHandler { id: topHover }
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                        }

                        RowLayout {
                            id: topRow
                            width: parent.width
                            spacing: Theme.layout.spacing * 1.5

                            Rectangle {
                                readonly property real pad: Theme.media.viewerPillPadding
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.maximumWidth: implicitWidth
                                implicitWidth: infoRow.implicitWidth + 2 * pad
                                implicitHeight: infoRow.implicitHeight + Theme.layout.spacing * 1.5
                                radius: Math.min(Theme.media.viewerPillRadius, height / 2)
                                color: Theme.media.viewerPillBackground

                                RowLayout {
                                    id: infoRow
                                    x: parent.pad
                                    width: parent.width - 2 * parent.pad
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: Theme.layout.spacing * 1.5

                                    Icon {
                                        visible: (root.game?.platformIcon ?? "") !== ""
                                        size: Theme.gallery.viewerLineSize + 6
                                        path: Icons.forPlatform(root.game?.platformIcon ?? "")
                                        color: Theme.gallery.viewerLineColor
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        spacing: 0

                                        Label {
                                            visible: root.game !== undefined
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                            text: root.game?.title ?? ""
                                            color: Theme.gallery.viewerLineColor
                                            font.pixelSize: Theme.gallery.viewerLineSize
                                            font.weight: Theme.font.emphasisWeight
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                            text: (root.current.time ? root.dateText(root.current.time) + "  ·  " : "")
                                                  + qsTr("%1 of %2").arg(root.index + 1).arg(root.list.length)
                                            color: Theme.gallery.viewerLineMuted
                                            font.pixelSize: Math.max(6, Theme.gallery.viewerLineSize - 2)
                                        }
                                    }
                                }
                            }
                            Item { Layout.fillWidth: true }
                            Surface {
                                visible: root.game !== undefined
                                implicitWidth: goLabel.implicitWidth + 2 * Theme.media.viewerPillPadding
                                implicitHeight: Theme.media.viewerButtonSize
                                radius: Math.min(Theme.media.viewerPillRadius, height / 2)
                                fill: goArea.containsMouse ? Qt.tint(Theme.media.viewerPillBackground, Theme.gallery.viewerGameHover)
                                                           : Theme.media.viewerPillBackground
                                fillEnd: fill
                                border.width: 0
                                scale: goArea.pressed ? Theme.motion.pressScale : 1
                                Behavior on fill { enabled: Motion.fades; MotionColor {} }
                                Behavior on scale { enabled: Motion.on; MotionNumber {} }

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
                            ControlPill {
                                ViewerButton {
                                    anchors.verticalCenter: parent.verticalCenter
                                    icon: Icons.close
                                    iconColor: Theme.media.viewerCloseColor
                                    tip: qsTr("Close (Esc)")
                                    onClicked: root.close()
                                }
                            }
                        }
                    }

                    Item {
                        id: bottomBar
                        x: bars.pad
                        y: parent.height - height - bars.pad / 2
                        width: parent.width - 2 * bars.pad
                        height: bottomColumn.implicitHeight
                        visible: root.isClip && root.error === "" && !root.needsFfmpeg
                        enabled: root.file !== ""

                        HoverHandler { id: bottomHover }
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                        }

                        ColumnLayout {
                            id: bottomColumn
                            width: parent.width
                            spacing: 0

                            Bar {
                                id: seekBar
                                Layout.fillWidth: true
                                preview: true
                                fraction: player.duration > 0 ? Math.min(1, player.position / player.duration) : 0
                                tipText: player.duration > 0 ? root.timeText(hoverFraction * player.duration) : ""
                                onMoved: f => {
                                    root.ended = false
                                    if (player.duration > 0)
                                        player.position = f * player.duration
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.media.viewerControlsGap

                                ControlPill {
                                    ViewerButton {
                                        anchors.verticalCenter: parent.verticalCenter
                                        icon: root.playing ? Icons.pauseFill : Icons.playFill
                                        iconSize: Theme.media.viewerPlayIconSize
                                        tip: root.playing ? qsTr("Pause (Space)") : qsTr("Play (Space)")
                                        onClicked: root.toggle()
                                    }
                                }
                                ControlPill {
                                    ViewerButton {
                                        anchors.verticalCenter: parent.verticalCenter
                                        icon: Icons.replay5Line
                                        tip: qsTr("Back 5 seconds (Left)")
                                        onClicked: root.seekBy(-5000)
                                    }
                                    ViewerButton {
                                        anchors.verticalCenter: parent.verticalCenter
                                        icon: Icons.forward5Line
                                        tip: qsTr("Forward 5 seconds (Right)")
                                        onClicked: root.seekBy(5000)
                                    }
                                }

                                ControlPill {
                                    id: volumeGroup
                                    readonly property bool open: volumeHover.hovered || volumeBar.active

                                    HoverHandler { id: volumeHover }

                                    ViewerButton {
                                        readonly property bool silent: root.muted || root.volume === 0
                                        anchors.verticalCenter: parent.verticalCenter
                                        icon: root.volumeIcon()
                                        tip: silent ? qsTr("Unmute (M)") : qsTr("Mute (M)")
                                        onClicked: root.toggleMute()
                                    }
                                    Item {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: volumeGroup.open ? Theme.media.viewerVolumeWidth + Theme.media.seekKnobSize + Theme.media.viewerPillPadding : 0
                                        height: volumeBar.implicitHeight
                                        clip: true
                                        Behavior on width { enabled: Motion.on; MotionNumber { speed: "normal" } }

                                        Bar {
                                            id: volumeBar
                                            // Inset so the knob at either end isn't cut off.
                                            x: Theme.media.seekKnobSize / 2
                                            width: Theme.media.viewerVolumeWidth
                                            fill: Theme.media.volumeFill
                                            fraction: root.muted ? 0 : root.volume
                                            opacity: volumeGroup.open ? 1 : 0
                                            onMoved: f => root.setVolume(f)
                                            Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }

                                            WheelHandler {
                                                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                                onWheel: event => root.nudgeVolume(event.angleDelta.y > 0 ? 0.05 : -0.05)
                                            }
                                        }
                                    }
                                }

                                ControlPill {
                                    padding: Theme.media.viewerPillPadding
                                    Label {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: root.timeText(root.ended ? player.duration : player.position)
                                        color: Theme.media.viewerTimeColor
                                        font.features: { "tnum": 1 }
                                    }
                                    Label {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: " / " + root.timeText(player.duration)
                                        color: Theme.media.viewerDurationColor
                                        font.features: { "tnum": 1 }
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                ControlPill {
                                    ViewerButton {
                                        anchors.verticalCenter: parent.verticalCenter
                                        icon: root.fullscreen ? Icons.fullscreenExitLine : Icons.fullscreenLine
                                        tip: root.fullscreen ? qsTr("Exit fullscreen (F)") : qsTr("Fullscreen (F)")
                                        onClicked: root.toggleFullscreen()
                                    }
                                }
                            }
                        }
                    }
                    ControlPill {
                        visible: !root.isClip && shot.status !== Image.Error
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: bars.pad

                        HoverHandler { id: zoomHover }

                        ViewerButton {
                            anchors.verticalCenter: parent.verticalCenter
                            icon: Icons.search
                            tip: qsTr("Reset zoom (0)")
                            onClicked: root.resetZoom()
                        }
                        Bar {
                            anchors.verticalCenter: parent.verticalCenter
                            width: Theme.media.viewerVolumeWidth
                            fraction: (root.zoom - 1) / (root.maxZoom - 1)
                            onMoved: f => root.zoomTo(1 + f * (root.maxZoom - 1), shot.width / 2, shot.height / 2)
                        }
                        Label {
                            id: zoomLabel
                            anchors.verticalCenter: parent.verticalCenter
                            width: zoomMetrics.width + Theme.layout.spacing + Theme.media.viewerPillPadding
                            leftPadding: Theme.layout.spacing
                            rightPadding: Theme.media.viewerPillPadding
                            horizontalAlignment: Text.AlignRight
                            text: Math.round(root.zoom * 100) + "%"
                            color: Theme.media.viewerTimeColor
                            font.features: { "tnum": 1 }
                            TextMetrics { id: zoomMetrics; font: zoomLabel.font; text: "800%" }
                        }
                    }
                }

                // Outside the bars, so it still shows while they are hidden.
                Rectangle {
                    id: flashSign

                    property string icon
                    property string text

                    function show(icon, text) {
                        flashSign.icon = icon
                        flashSign.text = text
                        flashRun.restart()
                    }

                    x: root.picture.x + (root.picture.width - width) / 2
                    y: root.picture.y + (root.picture.height - height) / 2
                    visible: Theme.media.viewerFlashSize > 0 && opacity > 0
                    height: Theme.media.viewerFlashSize
                    width: text === "" ? height : flashRow.implicitWidth + height * 0.6
                    radius: height / 2
                    color: Theme.media.viewerFlashBackground
                    opacity: 0

                    Row {
                        id: flashRow
                        anchors.centerIn: parent
                        spacing: Theme.layout.spacing

                        Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            path: flashSign.icon
                            size: Theme.media.viewerFlashIconSize
                            color: Theme.media.viewerControlColor
                        }
                        Label {
                            visible: flashSign.text !== ""
                            anchors.verticalCenter: parent.verticalCenter
                            text: flashSign.text
                            color: Theme.media.viewerControlColor
                            font.pixelSize: Theme.media.viewerFlashIconSize * 0.6
                            font.features: { "tnum": 1 }
                        }
                    }

                    SequentialAnimation {
                        id: flashRun
                        PropertyAction { target: flashSign; property: "opacity"; value: 1 }
                        PauseAnimation { duration: 350 }
                        NumberAnimation {
                            target: flashSign
                            property: "opacity"
                            to: 0
                            duration: Motion.fadeDuration(300)
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }
        }

        ViewerButton {
            id: previousButton
            visible: root.index > 0
            // Outside the picture the bars are hidden, but the way to the next item stays. Not bound to the
            // bars' opacity: entering the picture would drop it to the bars' fade-in and back.
            opacity: !pictureHover.hovered || root.barsShown ? 1 : 0
            Behavior on opacity {
                enabled: Motion.fades
                MotionNumber { fade: true; speed: "normal" }
            }
            x: Theme.media.viewerArrowInset
            y: (parent.height - height) / 2
            icon: Icons.chevronLeft
            tip: root.isClip ? qsTr("Previous (Page Up)") : qsTr("Previous (Left)")
            onClicked: root.step(-1)
        }
        ViewerButton {
            id: nextButton
            visible: root.index >= 0 && root.index < root.list.length - 1
            opacity: !pictureHover.hovered || root.barsShown ? 1 : 0
            Behavior on opacity {
                enabled: Motion.fades
                MotionNumber { fade: true; speed: "normal" }
            }
            x: parent.width - width - Theme.media.viewerArrowInset
            y: (parent.height - height) / 2
            icon: Icons.chevronRight
            tip: root.isClip ? qsTr("Next (Page Down)") : qsTr("Next (Right)")
            onClicked: root.step(1)
        }
    }
}
