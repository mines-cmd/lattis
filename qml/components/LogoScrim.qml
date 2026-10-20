import QtQuick
import QtQuick.Effects

// What darkens the art behind a hero's logo: a gradient on one side (or the whole hero), or a soft
// shadow that follows the logo's outline. Fills the logo's parent, so `logo` and `drawnRect` are in
// its own coordinates.
Item {
    id: scrim
    property string style: "none"
    property string side: "auto"
    property real strength: 1
    property Item logo
    property rect drawnRect
    property string vAlign: "bottom"
    property real edge
    property real rise
    // The hero preview in "Look up missing info" is a fraction of the game page's size; its shadow shrinks with it.
    property real shadowScale: 1
    readonly property color color: Qt.rgba(Theme.hero.scrimColor.r, Theme.hero.scrimColor.g, Theme.hero.scrimColor.b,
                                           Math.min(1, Theme.hero.scrimColor.a * strength))
    anchors.fill: parent

    Rectangle {
        id: gradientScrim
        readonly property bool autoSide: scrim.side === "auto"
        readonly property string side: autoSide ? (scrim.vAlign === "top" ? "top" : "bottom") : scrim.side
        readonly property bool vertical: side === "top" || side === "bottom" || side === "full"
        readonly property bool darkFirst: side === "top" || side === "left"
        readonly property real logoMiddleX: scrim.drawnRect.x + scrim.drawnRect.width / 2
        readonly property real logoMiddleY: scrim.drawnRect.y + scrim.drawnRect.height / 2

        opacity: scrim.style === "gradient" && scrim.strength > 0 ? 1 : 0
        x: side === "right" ? parent.width - width : 0
        y: side === "bottom" ? parent.height - height : 0
        width: side === "left" ? Math.min(parent.width, Math.max(parent.width * 0.35, autoSide || logoMiddleX < parent.width / 2
                                          ? scrim.drawnRect.x + scrim.drawnRect.width + scrim.edge : 0))
            : side === "right" ? Math.min(parent.width, Math.max(parent.width * 0.35, logoMiddleX > parent.width / 2
                                          ? parent.width - scrim.drawnRect.x + scrim.edge : 0))
            : parent.width
        height: side === "top" ? Math.min(parent.height, Math.max(parent.height * 0.55, autoSide || logoMiddleY < parent.height / 2
                                          ? scrim.drawnRect.y + scrim.drawnRect.height + scrim.edge : 0))
            : side === "bottom" ? (autoSide && scrim.vAlign === "center" ? parent.height
                  : Math.min(parent.height, Math.max(parent.height * 0.55, autoSide || logoMiddleY > parent.height / 2
                                          ? parent.height - scrim.drawnRect.y + scrim.edge : 0)))
            : parent.height
        gradient: Gradient {
            orientation: gradientScrim.vertical ? Gradient.Vertical : Gradient.Horizontal
            GradientStop { position: 0; color: gradientScrim.side === "full" || gradientScrim.darkFirst ? scrim.color : "transparent" }
            GradientStop { position: 1; color: gradientScrim.side === "full" || !gradientScrim.darkFirst ? scrim.color : "transparent" }
        }
    }

    // The logo blurred and colored, drawn behind it. Made only for the logos that use it. The texture
    // is padded by hand: the effect's own padding doesn't reach outside a Loader's item.
    Loader {
        id: shadow
        active: scrim.style === "shadow" && scrim.strength > 0 && scrim.logo !== null
        readonly property real pad: Theme.hero.shadowSize * scrim.shadowScale
        x: (scrim.logo?.x ?? 0) - pad
        y: (scrim.logo?.y ?? 0) - pad
        width: (scrim.logo?.width ?? 0) + 2 * pad
        height: (scrim.logo?.height ?? 0) + 2 * pad
        transform: Translate { y: scrim.rise }
        sourceComponent: Item {
            ShaderEffectSource {
                id: copy
                visible: false
                sourceItem: scrim.logo
                sourceRect: Qt.rect(-shadow.pad, -shadow.pad, shadow.width, shadow.height)
            }
            MultiEffect {
                anchors.fill: parent
                source: copy
                autoPaddingEnabled: false
                blurMax: Math.max(1, Math.round(Theme.hero.shadowSize * scrim.shadowScale))
                shadowEnabled: true
                shadowBlur: 1
                shadowVerticalOffset: Theme.hero.shadowOffset * scrim.shadowScale
                shadowColor: Qt.rgba(Theme.hero.shadowColor.r, Theme.hero.shadowColor.g, Theme.hero.shadowColor.b, 1)
                shadowOpacity: Math.min(1, Theme.hero.shadowColor.a * scrim.strength)
            }
        }
    }
}
