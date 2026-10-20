import QtQuick
import QtQuick.Effects

// See docs/code/qml.md.
Item {
    id: root

    property var game: ({})
    property int current: 0

    readonly property color background: Theme.palette.background
    readonly property real dim: 1 - Theme.backdrop.opacity

    clip: true

    // A refresh of the same game (a download's progress, every few seconds) must not
    // crossfade: while both layers are partly transparent the window background shows
    // through as a dark flash.
    onGameChanged: {
        const shown = layers.itemAt(current)?.game
        if (shown && shown.id === game.id && shown.heroArt === game.heroArt && shown.hue === game.hue)
            return
        layers.itemAt(1 - current)?.load(game)
    }

    Repeater {
        id: layers
        model: 2
        // With a warm library cache the first game arrives before the layers exist, so
        // onGameChanged finds none: the visible layer picks it up when it's created.
        onItemAdded: (index, item) => {
            if (index === root.current)
                item.load(root.game)
        }

        Item {
            id: layer

            required property int index
            property var game: ({})
            property bool pending: false

            function load(g) {
                pending = true
                game = g
                if (art.status !== Image.Loading)
                    show()
            }
            function show() {
                if (!pending)
                    return
                pending = false
                root.current = index
            }

            anchors.fill: parent
            opacity: root.current === index ? 1 : 0
            Behavior on opacity {
                enabled: Motion.fades && Theme.backdrop.fadeDuration > 0
                MotionNumber { fade: true; duration: Motion.fadeDuration(Theme.backdrop.fadeDuration) }
            }

            ArtPlaceholder {
                anchors.fill: parent
                visible: Theme.backdrop.placeholder && art.status !== Image.Ready
                hue: layer.game.hue ?? 0
            }

            Image {
                id: art
                // Overscan so the blur doesn't darken the edges.
                anchors.fill: parent
                anchors.margins: -Theme.backdrop.blurMax
                visible: false
                source: layer.game.heroUrl ?? ""
                sourceSize.width: Theme.backdrop.decodeSize
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                smooth: true
                onStatusChanged: {
                    if (status !== Image.Loading)
                        layer.show()
                }
            }

            MultiEffect {
                anchors.fill: art
                source: art
                visible: art.status === Image.Ready
                autoPaddingEnabled: false
                blurEnabled: Theme.backdrop.blur > 0
                blur: Theme.backdrop.blur
                blurMax: Theme.backdrop.blurMax
                saturation: Theme.backdrop.saturation
            }
        }
    }

    // Dim the art, then fade it into the window background toward the bottom. With a "down"
    // or "up" gradient, fades to the gradient's color at the backdrop's bottom edge so the art
    // is covered and there's no seam; other directions keep the flat behavior.
    readonly property color fadeTarget: {
        if (Qt.colorEqual(Theme.palette.background, Theme.palette.backgroundEnd))
            return root.background
        const direction = Theme.palette.gradientDirection
        if (direction !== "down" && direction !== "up")
            return root.background
        const t = direction === "down" ? Theme.backdrop.height : 1 - Theme.backdrop.height
        const s = Theme.palette.background
        const e = Theme.palette.backgroundEnd
        return Qt.rgba(s.r + (e.r - s.r) * t, s.g + (e.g - s.g) * t, s.b + (e.b - s.b) * t, 1)
    }

    ShaderEffect {
        anchors.fill: parent
        property color background: root.background
        property color target: root.fadeTarget
        property real dim: root.dim
        property real fadeStart: Theme.backdrop.fadeStart
        property real smoothFade: Theme.backdrop.fadeCurve === "smooth" ? 1 : 0
        property real dither: Theme.backdrop.dither
        fragmentShader: "qrc:/qt/qml/Launcher/qml/shaders/backdropfade.frag.qsb"
    }
}
