import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects

// See docs/code/qml.md.
Item {
    id: root

    property var game: ({})
    property bool logoOnly: false

    signal message(string text)

    readonly property int decodeWidth: Math.ceil(width * Screen.devicePixelRatio / 320) * 320

    // Two layers, as in Backdrop: a new game loads into the hidden one and fades in over the
    // visible one once its art has loaded, so the placeholder and the title never show in between.
    property int current: 0
    // The old layer stays opaque under the new one until that has faded in.
    property bool settling: false
    // Set for a pick that follows the last one closely (details.rapid_switch): the new art and logo
    // replace the old without fading, zooming or rising. The art is still waited for.
    property bool instant: false
    property var shown: null
    readonly property var shownGame: shown?.game ?? ({})
    readonly property real logoIn: shown?.logoIn ?? 1
    readonly property real logoStart: 0.3

    function syncShown() {
        shown = layers.itemAt(current)
    }
    onCurrentChanged: syncShown()

    property real pendingScale: -1
    property string pendingPosition: ""
    property real pendingOffsetX: NaN
    property real pendingOffsetY: NaN
    property string pendingScrimStyle: ""
    property string pendingScrimSide: ""
    property real pendingScrimStrength: -1
    readonly property string scrimChoice: pendingScrimStyle !== "" ? (pendingScrimStyle === "theme" ? "" : pendingScrimStyle)
        : (game.logoScrim ?? "")
    readonly property string scrimSideChoice: pendingScrimSide !== "" ? (pendingScrimSide === "theme" ? "" : pendingScrimSide)
        : (game.logoScrimSide ?? "")
    readonly property string scrimStyle: scrimChoice !== "" ? scrimChoice : Theme.hero.scrim ? Theme.hero.scrimStyle : "none"
    readonly property real scrimStrength: pendingScrimStrength >= 0 ? pendingScrimStrength : (game.logoScrimStrength ?? 1)
    readonly property bool scrimCustom: scrimChoice !== "" || scrimSideChoice !== "" || scrimStrength !== 1
    readonly property real logoScale: pendingScale > 0 ? pendingScale : (game.logoScale ?? 1)

    readonly property string position: pendingPosition !== "" ? pendingPosition
        : (game.logoPosition ?? "") !== "" ? game.logoPosition : Theme.hero.logoPosition
    readonly property string hAlign: position.endsWith("left") ? "left" : position.endsWith("right") ? "right" : "center"
    readonly property string vAlign: position.startsWith("top") ? "top" : position.startsWith("bottom") ? "bottom" : "center"
    readonly property real offsetX: !isNaN(pendingOffsetX) ? pendingOffsetX : (game.logoOffsetX ?? 0)
    readonly property real offsetY: !isNaN(pendingOffsetY) ? pendingOffsetY : (game.logoOffsetY ?? 0)
    readonly property real edge: height * Theme.hero.logoMargin

    readonly property real maxBoxWidth: Math.max(1, width - 2 * edge)
    readonly property real maxBoxHeight: Math.max(1, height - 2 * edge)
    readonly property real baseBoxWidth: width * Theme.hero.logoMaxWidth
    readonly property real baseBoxHeight: height * Theme.hero.logoMaxHeight
    readonly property real maxScale: Math.max(minScale, Math.min(maxBoxWidth / Math.max(1, baseBoxWidth),
                                                                 maxBoxHeight / Math.max(1, baseBoxHeight), 4))
    readonly property real minScale: 0.2

    readonly property bool logoShown: shown?.logoShown ?? false
    readonly property rect target: {
        const s = shown
        if (!s)
            return Qt.rect(0, 0, 0, 0)
        if (logoOnly) {
            const h = s.hAlign
            const v = s.vAlign
            const w = logoShown ? s.plainLogo.paintedWidth : s.plainTitle.contentWidth
            const ht = logoShown ? s.plainLogo.paintedHeight : s.plainTitle.contentHeight
            return Qt.rect(h === "left" ? 0 : h === "right" ? width - w : (width - w) / 2,
                           v === "top" ? 0 : v === "bottom" ? height - ht : (height - ht) / 2, w, ht)
        }
        return s.drawnRect
    }

    onGameChanged: {
        if (game.id !== adjustedGameId)
            adjusting = false
        // A save arriving while the adjustment goes on must not put the logo back.
        if (!(adjuster.item?.busy ?? false)) {
            pendingScale = -1
            pendingPosition = ""
            pendingOffsetX = NaN
            pendingOffsetY = NaN
            clearPendingScrim()
        }

        const visibleLayer = shown
        if (!visibleLayer)
            return
        const hidden = layers.itemAt(1 - current)
        const was = visibleLayer.game
        // A refresh of the same game (saving the logo's scale) must not crossfade.
        // The same art under a new heroUrl is its display copy arriving: the picture stays as it is.
        if (was.id === game.id && was.heroArt === game.heroArt && was.logoUrl === game.logoUrl && was.hue === game.hue) {
            visibleLayer.game = was.heroUrl === game.heroUrl ? game : Object.assign({}, game, { heroUrl: was.heroUrl })
            hidden?.cancel()
            deferred = null
        } else if (settling) {
            // The hidden layer is still the opaque one under the new art: changing it now would
            // flash the placeholder, so only the latest game waits for the settle to end.
            deferred = game
        } else {
            hidden?.load(game)
        }
    }

    property var deferred: null

    function loadDeferred() {
        const next = deferred
        deferred = null
        if (next && next.id !== undefined && next.id !== shown?.game.id)
            layers.itemAt(1 - current)?.load(next)
    }

    function clearPendingScrim() {
        pendingScrimStyle = ""
        pendingScrimSide = ""
        pendingScrimStrength = -1
    }

    function save(values) {
        const error = Library.saveGame(game.id, values)
        if (error !== "") {
            pendingScale = -1
            pendingPosition = ""
            pendingOffsetX = NaN
            pendingOffsetY = NaN
            clearPendingScrim()
            message(error)
        }
    }

    // A preset puts the logo exactly there, so it drops any offset.
    function setPosition(value) {
        if (value === position && offsetX === 0 && offsetY === 0)
            return
        pendingPosition = value
        pendingOffsetX = 0
        pendingOffsetY = 0
        save({ logo_position: value, logo_offset_x: null, logo_offset_y: null })
    }

    function resetPosition() {
        pendingPosition = Theme.hero.logoPosition
        pendingOffsetX = 0
        pendingOffsetY = 0
        save({ logo_position: null, logo_offset_x: null, logo_offset_y: null })
    }

    function saveScale(value) {
        const rounded = Math.round(value * 100) / 100
        pendingScale = rounded
        save({ logo_scale: Math.abs(rounded - 1) < 0.005 ? null : rounded })
    }

    function setScrimStyle(value) {
        pendingScrimStyle = value === "" ? "theme" : value
        save({ logo_scrim: value === "" ? null : value })
    }

    function setScrimSide(value) {
        pendingScrimSide = value === "" ? "theme" : value
        save({ logo_scrim_side: value === "" ? null : value })
    }

    function previewScrimStrength(value) {
        pendingScrimStrength = value
    }

    function saveScrimStrength(value) {
        const rounded = Math.round(value * 100) / 100
        pendingScrimStrength = rounded
        save({ logo_scrim_strength: Math.abs(rounded - 1) < 0.005 ? null : rounded })
    }

    function resetScrim() {
        pendingScrimStyle = "theme"
        pendingScrimSide = "theme"
        pendingScrimStrength = 1
        save({ logo_scrim: null, logo_scrim_side: null, logo_scrim_strength: null })
    }

    property bool adjusting: false
    property string adjustedGameId

    readonly property int adjusterCursor: adjusting ? (adjuster.item?.cursor ?? Qt.ArrowCursor) : Qt.ArrowCursor

    function startAdjust() {
        adjustedGameId = game.id
        adjuster.setSource("LogoAdjuster.qml", { hero: root })
        adjusting = true
    }
    function finishAdjust() {
        adjuster.item?.finish()
    }

    function openScrimMenu(side) {
        openMenu()
        menu.item.openBehindLogo(side)
    }

    function openMenu(x, y) {
        if (menu.item === null)
            menu.setSource("LogoMenu.qml", { hero: root })
        menu.active = true
        if (x === undefined)
            menu.item.popup(root, target.x + target.width / 2, target.y + target.height / 2)
        else
            menu.item.popup()
    }

    // Drawn through the MultiEffect below, so the logoOnly parts switch by opacity, not visible:
    // a child's visible changing doesn't redraw a hidden source.
    Item {
        id: content
        anchors.fill: parent
        visible: false

        Repeater {
            id: layers
            model: 2
            // With a warm library cache the first game arrives before the layers exist, so
            // onGameChanged finds none: the visible layer picks it up when it's created.
            onItemAdded: (index, item) => {
                root.syncShown()
                if (index === root.current)
                    item.load(root.game)
            }

            Item {
                id: layer

                required property int index
                property var game: ({})
                property bool pending: false
                // 0 when the layer starts to fade in, 1 once it has settled; the art zooms out
                // and the logo rises with it. A layer that was never switched to is already 1.
                property real arrival: 1
                readonly property real logoIn: Theme.hero.animateLogo ? Math.max(0, Math.min(1, (arrival - root.logoStart) / (1 - root.logoStart))) : 1

                readonly property bool isCurrent: root.current === index
                // An Image whose source is cleared keeps reporting Ready (its last image), so the
                // title can't go by the status alone.
                readonly property bool hasLogo: (game.logoUrl ?? "") !== ""
                readonly property bool loading: art.status === Image.Loading || logoImage.status === Image.Loading
                                                || plainLogoImage.status === Image.Loading
                readonly property bool heroLoading: art.status === Image.Loading
                readonly property bool logoShown: hasLogo && (root.logoOnly ? plainLogoImage.status : logoImage.status) === Image.Ready

                // Its own game's scale and position, so the old layer doesn't jump while fading out.
                readonly property real logoScale: isCurrent && root.pendingScale > 0 ? root.pendingScale : (game.logoScale ?? 1)
                readonly property string position: isCurrent && root.pendingPosition !== "" ? root.pendingPosition
                    : (game.logoPosition ?? "") !== "" ? game.logoPosition : Theme.hero.logoPosition
                readonly property string hAlign: position.endsWith("left") ? "left" : position.endsWith("right") ? "right" : "center"
                readonly property string vAlign: position.startsWith("top") ? "top" : position.startsWith("bottom") ? "bottom" : "center"

                readonly property real offsetX: isCurrent && !isNaN(root.pendingOffsetX) ? root.pendingOffsetX : (game.logoOffsetX ?? 0)
                readonly property real offsetY: isCurrent && !isNaN(root.pendingOffsetY) ? root.pendingOffsetY : (game.logoOffsetY ?? 0)

                // Its own game's scrim, so the old layer keeps its look while fading out. A game's
                // choice wins over the theme's both ways.
                readonly property string scrimChoice: isCurrent && root.pendingScrimStyle !== ""
                    ? (root.pendingScrimStyle === "theme" ? "" : root.pendingScrimStyle) : (game.logoScrim ?? "")
                readonly property string scrimStyle: scrimChoice !== "" ? scrimChoice
                    : Theme.hero.scrim ? Theme.hero.scrimStyle : "none"
                readonly property string scrimSideChoice: isCurrent && root.pendingScrimSide !== ""
                    ? (root.pendingScrimSide === "theme" ? "" : root.pendingScrimSide) : (game.logoScrimSide ?? "")
                readonly property string scrimSide: scrimSideChoice !== "" ? scrimSideChoice : Theme.hero.scrimSide
                readonly property real scrimStrength: isCurrent && root.pendingScrimStrength >= 0
                    ? root.pendingScrimStrength : (game.logoScrimStrength ?? 1)

                property alias plainLogo: plainLogoImage
                property alias plainTitle: plainTitleLabel

                readonly property real artScale: Theme.hero.animateArt ? 1 + (Theme.hero.switchZoom - 1) * (1 - arrival) : 1
                readonly property real logoRise: Theme.hero.switchRise * (1 - logoIn)

                readonly property real drawnWidth: logoShown ? logoImage.paintedWidth : titleLabel.contentWidth
                readonly property real drawnHeight: logoShown ? logoImage.paintedHeight : titleLabel.contentHeight
                readonly property rect drawnRect: {
                    const box = logoShown ? logoImage : titleLabel
                    return Qt.rect(box.x + innerX(box.width, drawnWidth), box.y + innerY(box.height, drawnHeight),
                                   drawnWidth, drawnHeight)
                }

                function innerX(boxWidth: real, drawn: real): real {
                    return hAlign === "left" ? 0 : hAlign === "right" ? boxWidth - drawn : (boxWidth - drawn) / 2
                }
                function innerY(boxHeight: real, drawn: real): real {
                    return vAlign === "top" || !logoShown ? 0 : vAlign === "bottom" ? boxHeight - drawn : (boxHeight - drawn) / 2
                }
                function boxX(boxWidth: real, drawn: real): real {
                    const base = hAlign === "left" ? root.edge : hAlign === "right" ? root.width - boxWidth - root.edge : (root.width - boxWidth) / 2
                    const inner = innerX(boxWidth, drawn)
                    return Math.max(0, Math.min(root.width - drawn, base + offsetX * root.height + inner)) - inner
                }
                function boxY(boxHeight: real, drawn: real): real {
                    const base = vAlign === "top" ? root.edge : vAlign === "bottom" ? root.height - boxHeight - root.edge : (root.height - boxHeight) / 2
                    const inner = innerY(boxHeight, drawn)
                    return Math.max(0, Math.min(root.height - drawn, base + offsetY * root.height + inner)) - inner
                }

                function load(g) {
                    pending = true
                    game = g
                    if (!loading || Theme.hero.switchTimeout === 0)
                        show()
                    else
                        timeout.restart()
                }
                function cancel() {
                    pending = false
                    timeout.stop()
                }
                function show() {
                    if (!pending)
                        return
                    cancel()
                    root.awaitingArt = false
                    root.settling = game.id !== undefined && !root.instant && (Theme.hero.animateArt || Theme.hero.animateLogo) && Motion.duration(Theme.hero.fadeDuration) > 0
                    if (root.settling) {
                        arrival = 0
                        arrivalAnimation.restart()
                    } else {
                        arrival = 1
                    }
                    root.current = index
                    if (root.settling)
                        settleTimer.restart()
                }
                onLoadingChanged: {
                    if (!loading)
                        show()
                }

                Timer {
                    id: timeout
                    interval: Theme.hero.switchTimeout
                    onTriggered: layer.show()
                }

                anchors.fill: parent
                z: isCurrent ? 1 : 0
                opacity: isCurrent || (root.settling && Theme.hero.animateArt && !root.logoOnly) ? 1 : 0
                // Without the art underneath (logoOnly) both fade, else the old one just goes
                // once the new one covers it: two half-transparent layers would show the window
                // background as a dark dip.
                Behavior on opacity {
                    enabled: Motion.fades && !root.instant && Theme.hero.fadeDuration > 0 && (layer.isCurrent || root.logoOnly)
                        && (root.logoOnly ? Theme.hero.animateLogo : Theme.hero.animateArt)
                    MotionNumber { fade: true; duration: Motion.fadeDuration(Theme.hero.fadeDuration) }
                }
                MotionNumber {
                    id: arrivalAnimation
                    target: layer
                    property: "arrival"
                    from: 0
                    to: 1
                    duration: Motion.duration(Theme.hero.fadeDuration)
                }

                ArtPlaceholder {
                    anchors.fill: parent
                    // While the art of a switch that timed out loads, the old art shows through
                    // instead: a gradient fading in over it, then the art, reads as a glitch.
                    opacity: root.logoOnly || layer.heroLoading || layer.game.id === undefined ? 0 : 1
                    scale: layer.artScale
                    hue: layer.game.hue ?? 0
                }

                Image {
                    id: art
                    anchors.fill: parent
                    scale: layer.artScale
                    source: root.logoOnly ? "" : (layer.game.heroUrl ?? "") // not decoded while hidden
                    sourceSize.width: root.decodeWidth
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    opacity: status === Image.Ready ? 1 : 0
                    // Only for art arriving after the switch timed out; otherwise the layer fades.
                    Behavior on opacity {
                        enabled: Motion.fades && !root.instant && Theme.hero.fadeDuration > 0 && layer.isCurrent && Theme.hero.animateArt
                        MotionNumber { fade: true; duration: Motion.fadeDuration(Theme.hero.fadeDuration) }
                    }
                }

                LogoScrim {
                    style: root.logoOnly ? "none" : layer.scrimStyle
                    side: layer.scrimSide
                    strength: layer.scrimStrength
                    logo: layer.logoShown ? logoImage : titleLabel
                    drawnRect: layer.drawnRect
                    vAlign: layer.vAlign
                    edge: root.edge
                    rise: layer.logoRise
                }

                Image {
                    id: logoImage
                    opacity: root.logoOnly ? 0 : layer.logoIn
                    transform: Translate { y: layer.logoRise }
                    width: Math.min(root.baseBoxWidth * layer.logoScale, root.maxBoxWidth)
                    height: Math.min(root.baseBoxHeight * layer.logoScale, root.maxBoxHeight)
                    x: layer.boxX(width, paintedWidth)
                    y: layer.boxY(height, paintedHeight)
                    source: layer.game.logoUrl ?? ""
                    sourceSize.width: root.decodeWidth
                    fillMode: Image.PreserveAspectFit
                    horizontalAlignment: layer.hAlign === "left" ? Image.AlignLeft : layer.hAlign === "right" ? Image.AlignRight : Image.AlignHCenter
                    verticalAlignment: layer.vAlign === "top" ? Image.AlignTop : layer.vAlign === "bottom" ? Image.AlignBottom : Image.AlignVCenter
                    asynchronous: true
                }

                Label {
                    id: titleLabel
                    opacity: !root.logoOnly && (!layer.hasLogo || logoImage.status !== Image.Ready) ? layer.logoIn : 0
                    transform: Translate { y: layer.logoRise }
                    width: Math.min(root.width * 0.6 * layer.logoScale, root.maxBoxWidth)
                    x: layer.boxX(width, contentWidth)
                    y: layer.boxY(height, contentHeight)
                    text: layer.game.title ?? ""
                    color: Theme.hero.titleColor
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    horizontalAlignment: layer.hAlign === "left" ? Text.AlignLeft : layer.hAlign === "right" ? Text.AlignRight : Text.AlignHCenter
                    font.pixelSize: Math.min(Theme.hero.titleMaxFontSize, Math.max(16, root.height * 0.13)) * layer.logoScale
                    font.weight: Theme.hero.titleWeight
                }

                Image {
                    id: plainLogoImage
                    opacity: root.logoOnly ? layer.logoIn : 0
                    transform: Translate { y: layer.logoRise }
                    anchors.fill: parent
                    source: root.logoOnly ? (layer.game.logoUrl ?? "") : ""
                    sourceSize.height: Math.ceil(height * Screen.devicePixelRatio)
                    fillMode: Image.PreserveAspectFit
                    horizontalAlignment: layer.hAlign === "left" ? Image.AlignLeft : layer.hAlign === "right" ? Image.AlignRight : Image.AlignHCenter
                    verticalAlignment: layer.vAlign === "top" ? Image.AlignTop : layer.vAlign === "bottom" ? Image.AlignBottom : Image.AlignVCenter
                    asynchronous: true
                }
                Label {
                    id: plainTitleLabel
                    opacity: root.logoOnly && (!layer.hasLogo || plainLogoImage.status !== Image.Ready) ? layer.logoIn : 0
                    transform: Translate { y: layer.logoRise }
                    anchors.fill: parent
                    text: layer.game.title ?? ""
                    color: Theme.hero.titleColor
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    horizontalAlignment: layer.hAlign === "left" ? Text.AlignLeft : layer.hAlign === "right" ? Text.AlignRight : Text.AlignHCenter
                    verticalAlignment: layer.vAlign === "top" ? Text.AlignTop : layer.vAlign === "bottom" ? Text.AlignBottom : Text.AlignVCenter
                    font.pixelSize: Math.max(12, Math.min(Theme.hero.titleMaxFontSize, height * 0.36))
                    font.weight: Theme.hero.titleWeight
                }
            }
        }
    }

    Timer {
        id: settleTimer
        interval: Motion.duration(Theme.hero.fadeDuration)
        onTriggered: {
            // Art still loading after the switch timed out: the old layer stays under it until
            // the art has faded in.
            if (root.shown?.heroLoading) {
                root.awaitingArt = true
                return
            }
            root.settling = false
            root.loadDeferred()
        }
    }

    property bool awaitingArt: false

    Connections {
        target: root.shown
        function onHeroLoadingChanged() {
            if (root.awaitingArt && !root.shown.heroLoading) {
                root.awaitingArt = false
                settleTimer.restart()
            }
        }
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        radius: Theme.hero.radius
        visible: false
        layer.enabled: true
    }

    MultiEffect {
        anchors.fill: parent
        source: content
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }

    MouseArea {
        x: root.target.x
        y: root.target.y
        width: root.target.width
        height: root.target.height
        visible: root.game.id !== undefined
        acceptedButtons: Qt.RightButton
        onClicked: mouse => root.openMenu(mouse.x, mouse.y)
    }

    Loader {
        id: menu
        active: false
    }

    Loader {
        id: adjuster
        anchors.fill: parent
        active: root.adjusting || opacity > 0
        enabled: root.adjusting
        opacity: root.adjusting ? 1 : 0

        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true; speed: "fast" } }
    }
}
