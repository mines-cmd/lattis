import QtQuick
import QtQuick.Window

// See docs/code/qml.md.
Item {
    id: root

    property real logoHeight: 96
    property string text: "auto"
    property color background: "black"
    property real pixelScale: Screen.devicePixelRatio

    readonly property bool lightText: {
        if (text !== "auto")
            return text === "light"
        return 0.2126 * background.r + 0.7152 * background.g + 0.0722 * background.b < 0.5
    }

    // The same dog as the app icon, so the setting's choice shows here too.
    readonly property string iconChoice: Theme.availableColors.includes(General.appIcon) ? General.appIcon : "theme"
    readonly property var colors: iconChoice !== "theme" ? Theme.dogColors(iconChoice) : {
        background: Theme.rail.dogBackground, backgroundEnd: Theme.rail.dogBackgroundEnd,
        coat: Theme.rail.dogCoat, coatEnd: Theme.rail.dogCoatEnd, details: Theme.rail.dogDetails
    }

    // Where the art sits in resources/logo/dog/lattis-dog-wordmark.svg, as fractions of its
    // viewBox's height: the margin (the icon's left edge; the same above and below), the name's
    // right edge from the right, and its baseline.
    readonly property real artAspect: 1083.83 / 323.44
    readonly property real artLeft: 33.72 / 323.44
    readonly property real artRight: 24.52 / 323.44
    readonly property real artBaseline: 256.68 / 323.44
    readonly property real textBaseline: logoHeight * (artBaseline - artLeft)

    // Whole pixels throughout: the image is rendered at its drawn size, and a fractional offset
    // would resample it again and blur it.
    readonly property int margin: Math.round(logoHeight * artLeft)

    implicitWidth: logo.width - margin - Math.round(logoHeight * artRight)
    implicitHeight: logo.height - 2 * margin

    Image {
        id: logo
        x: -root.margin
        y: -root.margin
        height: Math.round(root.logoHeight)
        width: Math.round(root.logoHeight * root.artAspect)
        source: "image://appicon/wordmark/" + (root.lightText ? "light/" : "dark/")
                + [root.colors.background, root.colors.backgroundEnd, root.colors.coat, root.colors.coatEnd,
                   root.colors.details].map(color => String(color).substring(1)).join("/")
        sourceSize.height: Math.ceil(root.logoHeight * root.pixelScale)
        fillMode: Image.PreserveAspectFit
        smooth: true
    }
}
