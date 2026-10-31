import QtQuick
import QtQuick.Shapes

// See docs/code/qml.md.
Item {
    id: root
    clip: true

    // Image URLs; the wall repeats them to fill its rows.
    property var sources: []
    property bool active: true
    property bool moving: true

    property real rotateX: 18
    property real rotateY: -12
    property real rotateZ: -8
    property real perspective: 1400
    property real tileHeight: 265
    property real tileWidth: tileHeight * 2 / 3
    property real gap: 14
    property real tileRadius: 8
    property real speed: 10
    property size decodeSize: Qt.size(300, 450)
    property real dim: 0.65
    property real vignette: 0.6
    property color tint: "transparent"

    readonly property real stride: tileWidth + gap
    readonly property real rowStride: tileHeight + gap

    // Rx · Ry · Rz, the order CSS applies `rotateX() rotateY() rotateZ()` in; row-major, y down,
    // z toward the viewer.
    readonly property var rot: rotation3()
    // The part of the plane the window shows, as the bounding box of the window's four corners
    // mapped back onto the plane, in distances from the plane's center (which sits at the window's
    // center). The flat wall only needs tiles there.
    readonly property var plane: visiblePlane()
    readonly property int rowCount: Math.ceil((plane.bottom - plane.top) / rowStride) + 2
    // A row holds one set of tiles, a little wider than the plane's part; a tile that leaves on
    // one side comes back on the other.
    readonly property int setCount: Math.ceil((plane.right - plane.left) / stride) + 3
    readonly property real setWidth: setCount * stride

    // Distance every row has moved, in pixels, at a fixed speed (a loop whose length followed the
    // window's size kept the length it started with and ran too fast). Each row adds its own fixed
    // offset, so a resize doesn't make rows jump and screenshots are stable.
    property real travelled: 0
    NumberAnimation on travelled {
        from: 0; to: 1000000
        loops: Animation.Infinite
        duration: Math.max(1, Motion.duration(to / root.speed * 1000))
        running: root.moving && Motion.on
        paused: !root.active
    }
    // 0 loads every source at once; otherwise this many start past the number already settled,
    // for a server that answers a burst of requests with errors.
    property int concurrency: 0
    property int retries: 0
    property int settledCount: 0
    readonly property bool ready: settledCount >= sources.length
    property var failed: ({})
    // The tiles only show images that loaded, so a failed one leaves no empty tile.
    readonly property var good: sources.filter(s => !failed[s])

    // Decodes each image once, so the rows' tiles take it from the cache and the wall
    // appears only when none can still be empty.
    Repeater {
        model: root.sources
        Image {
            id: probe
            required property string modelData
            required property int index
            property bool counted: false
            property int tries: 0
            // Latched: a source that followed settledCount would loop through its own status.
            property bool started: root.concurrency === 0
            visible: false
            asynchronous: true
            cache: true
            sourceSize: root.decodeSize
            source: started ? modelData : ""

            readonly property bool gone: status === Image.Error && tries >= root.retries
            onStatusChanged: {
                if (status === Image.Error && tries < root.retries) {
                    retry.start()
                } else if (!counted && (status === Image.Ready || gone)) {
                    counted = true
                    root.settledCount++
                    if (gone)
                        root.failed = Object.assign({}, root.failed, { [modelData]: true })
                }
            }
            Timer {
                id: retry
                interval: 1000 * (probe.tries + 1)
                onTriggered: {
                    probe.tries++
                    probe.started = false
                    Qt.callLater(() => { probe.started = true })
                }
            }
            Connections {
                target: root
                function onSettledCountChanged() { probe.start() }
            }
            Component.onCompleted: start()
            function start() {
                if (!started && tries === 0 && index < root.settledCount + root.concurrency)
                    Qt.callLater(() => { started = true })
            }
        }
    }

    function multiply(a, b) {
        var r = []
        for (var i = 0; i < 4; ++i)
            for (var j = 0; j < 4; ++j) {
                var s = 0
                for (var k = 0; k < 4; ++k)
                    s += a[i * 4 + k] * b[k * 4 + j]
                r.push(s)
            }
        return r
    }

    function rotation3() {
        var rad = Math.PI / 180
        var ax = rotateX * rad, ay = rotateY * rad, az = rotateZ * rad
        var rx = [1, 0, 0, 0,  0, Math.cos(ax), -Math.sin(ax), 0,  0, Math.sin(ax), Math.cos(ax), 0,  0, 0, 0, 1]
        var ry = [Math.cos(ay), 0, Math.sin(ay), 0,  0, 1, 0, 0,  -Math.sin(ay), 0, Math.cos(ay), 0,  0, 0, 0, 1]
        var rz = [Math.cos(az), -Math.sin(az), 0, 0,  Math.sin(az), Math.cos(az), 0, 0,  0, 0, 1, 0,  0, 0, 0, 1]
        return multiply(multiply(rx, ry), rz)
    }

    // The plane point (u, v) rotated by R lands at the screen offset (x', y') / (1 - z' / d) from
    // the center; solved for (u, v) at a screen offset (X, Y), that is two linear equations
    // (coverwall.frag does the same per pixel).
    function planePoint(X, Y) {
        var r = rot, d = perspective
        var a11 = r[0] + X * r[8] / d, a12 = r[1] + X * r[9] / d
        var a21 = r[4] + Y * r[8] / d, a22 = r[5] + Y * r[9] / d
        var det = a11 * a22 - a12 * a21
        return Qt.point((X * a22 - Y * a12) / det, (a11 * Y - a21 * X) / det)
    }

    function visiblePlane() {
        var hw = width / 2, hh = height / 2
        var corners = [planePoint(-hw, -hh), planePoint(hw, -hh), planePoint(hw, hh), planePoint(-hw, hh)]
        var b = {left: Infinity, top: Infinity, right: -Infinity, bottom: -Infinity}
        for (var i = 0; i < 4; ++i) {
            b.left = Math.min(b.left, corners[i].x)
            b.right = Math.max(b.right, corners[i].x)
            b.top = Math.min(b.top, corners[i].y)
            b.bottom = Math.max(b.bottom, corners[i].y)
        }
        return b
    }

    // The wall laid out flat, drawn into a texture that the shader below tilts. (Qt's own 3D
    // transform, applied to the tiles one by one, dropped the ones on the near side.)
    Item {
        id: flat
        width: root.plane.right - root.plane.left
        height: root.plane.bottom - root.plane.top
        visible: false
        layer.enabled: true
        layer.smooth: true
        layer.mipmap: true

        Repeater {
            model: root.good.length > 0 ? root.rowCount : 0

            Item {
                id: row
                required property int index
                y: (index - 1) * root.rowStride

                readonly property real offset: (index * 0.37) % 1
                readonly property real along: (root.travelled + offset * root.setWidth) % root.setWidth
                readonly property real shift: index % 2 === 0 ? along : root.setWidth - along

                Repeater {
                    model: root.setCount

                    ShaderEffect {
                        id: tile
                        required property int index
                        // Each row starts 7 tiles further along, so all of them show across the
                        // rows and none stacks straight down.
                        readonly property string url: root.good[(row.index * 7 + index) % root.good.length]
                        // A tile that leaves on the left comes back on the right, a set further on.
                        x: ((index + 1) * root.stride - row.shift + root.setWidth) % root.setWidth - root.stride
                        width: root.tileWidth
                        height: root.tileHeight
                        property variant source: art
                        property size size: Qt.size(width, height)
                        property real radius: root.tileRadius
                        fragmentShader: "qrc:/qt/qml/Launcher/qml/shaders/coverround.frag.qsb"

                        Image {
                            id: art
                            visible: false
                            width: root.tileWidth
                            height: root.tileHeight
                            asynchronous: true
                            cache: true
                            mipmap: true
                            sourceSize: root.decodeSize
                            source: tile.url
                        }
                    }
                }
            }
        }
    }

    ShaderEffect {
        anchors.fill: parent
        opacity: root.ready ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true; speed: "slow"; linear: true } }
        property variant source: flat
        property vector4d rot0: Qt.vector4d(root.rot[0], root.rot[1], root.rot[8], root.rot[9])
        property vector4d rot1: Qt.vector4d(root.rot[4], root.rot[5], 0, 0)
        property size viewSize: Qt.size(width, height)
        property point origin: Qt.point(root.plane.left, root.plane.top)
        property size planeSize: Qt.size(flat.width, flat.height)
        property real viewDistance: root.perspective
        fragmentShader: "qrc:/qt/qml/Launcher/qml/shaders/coverwall.frag.qsb"
    }

    Rectangle {
        anchors.fill: parent
        color: root.tint
        opacity: root.dim
    }

    Shape {
        anchors.fill: parent
        ShapePath {
            strokeColor: "transparent"
            fillGradient: RadialGradient {
                centerX: root.width / 2; centerY: root.height / 2
                centerRadius: Math.hypot(root.width, root.height) / 2
                focalX: centerX; focalY: centerY
                GradientStop { position: 0.3; color: Qt.alpha(root.tint, 0) }
                GradientStop { position: 1.0; color: Qt.alpha(root.tint, root.vignette) }
            }
            startX: 0; startY: 0
            PathLine { x: root.width; y: 0 }
            PathLine { x: root.width; y: root.height }
            PathLine { x: 0; y: root.height }
            PathLine { x: 0; y: 0 }
        }
    }
}
