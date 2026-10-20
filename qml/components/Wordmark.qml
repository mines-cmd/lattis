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

    // Where the art sits in the PNG (tools/logo/export.js), as fractions of its height: the
    // margin (the icon's left edge; the same above and below), the wordmark's right edge from
    // the right, and its baseline.
    readonly property real artLeft: 44 / 422
    readonly property real artRight: 32 / 422
    readonly property real artBaseline: 307 / 422
    readonly property real textBaseline: logoHeight * (artBaseline - artLeft)

    implicitWidth: logo.width - logoHeight * (artLeft + artRight)
    implicitHeight: logoHeight * (1 - 2 * artLeft)

    Image {
        id: logo
        x: -root.logoHeight * root.artLeft
        y: -root.logoHeight * root.artLeft
        height: root.logoHeight
        width: root.logoHeight * implicitWidth / Math.max(1, implicitHeight)
        source: root.lightText ? "qrc:/qt/qml/Launcher/resources/logo/lattis-ember-horizontal.png"
                               : "qrc:/qt/qml/Launcher/resources/logo/lattis-ember-horizontal-light.png"
        sourceSize.height: Math.ceil(root.logoHeight * root.pixelScale)
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
    }
}
