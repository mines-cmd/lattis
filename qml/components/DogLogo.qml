import QtQuick
import QtQuick.Shapes
import QtQuick.Window

// See docs/code/qml.md.
Item {
    id: root

    property real radius: width * 68 / 256
    // The tile's corner in the drawing's 256 units, before the dog shrinks to fit it.
    readonly property real corner: width > 0 ? Math.min(128, radius * 256 / width) : 0
    // Past this roundness the ear and the barks would touch the tile's edge.
    readonly property real fit: 1 - 0.12 * Math.max(0, corner - 80) / 48

    readonly property color coat: Theme.rail.dogCoat
    readonly property color coatEnd: Theme.rail.dogCoatEnd
    readonly property color details: Theme.rail.dogDetails
    readonly property color coatMiddle: mixed(coat, coatEnd, 0.4)

    function mixed(a, b, amount) {
        return Qt.rgba(a.r + (b.r - a.r) * amount, a.g + (b.g - a.g) * amount,
                       a.b + (b.b - a.b) * amount, 1)
    }

    // The neck runs off the tile, so its end follows the tile's outline.
    function neckPath() {
        const r = corner / fit
        const left = 128 - 128 / fit
        const bottom = 128 + 128 / fit
        const tilt = 18.94 / 37.79
        const edgeY = 193.37 + (42.29 - left) * tilt
        const cy = bottom - r
        let start = `M${left} ${edgeY}L${left} ${cy}`
        if (edgeY > cy) {
            // Where the neck's upper line meets the corner's arc, as a step x right of the edge.
            const a = 1 + tilt * tilt
            const b = -r - tilt * (edgeY - cy)
            const c = (edgeY - cy) * (edgeY - cy)
            const x = (-b - Math.sqrt(b * b - a * c)) / a
            start = `M${left + x} ${edgeY - x * tilt}`
        }
        const cornerX = bottom - r
        const past = 142.4 - cornerX
        const end = past > 0
                  ? `L${cornerX} ${bottom}A${r} ${r} 0 0 0 142.4 ${cy + Math.sqrt(r * r - past * past)}`
                  : `L142.4 ${bottom}`
        const arc = r > 0 ? `A${r} ${r} 0 0 0 ${left + r} ${bottom}` : ""
        return `${start}${arc}${end}L142.4 215.41A1.92 1.92 0 0 0 140.87 213.54L43.86 193.16A2.4 2.4 0 0 0 42.29 193.37Z`
    }

    // Scaled as one picture: redrawn at each step of the press dip, the paths would snap to pixels
    // one by one and the brow would jump. It's drawn larger, since the paths' own antialiasing
    // leaves the collar's slight tilt in steps; the margin keeps the tile's smooth edge in it.
    Item {
        readonly property real density: 4 * Screen.devicePixelRatio
        anchors.fill: parent
        anchors.margins: -2
        layer.enabled: true
        layer.smooth: true
        layer.mipmap: true
        layer.textureSize: Qt.size(width * density, height * density)

        Item {
            anchors.fill: parent
            anchors.margins: 2

            Surface {
                anchors.fill: parent
                radius: root.radius
                fill: Theme.rail.dogBackground
                fillEnd: Theme.rail.dogBackgroundEnd
                direction: Theme.rail.gradientDirection
            }

            Shape {
                width: 256
                height: 256
                anchors.centerIn: parent
                scale: root.width / 256 * root.fit
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: "transparent"
                    fillGradient: LinearGradient {
                        x1: 96; y1: 192; x2: 32; y2: 256
                        GradientStop { position: 0; color: root.coatMiddle }
                        GradientStop { position: 1; color: Qt.darker(root.coatEnd, 1.06) }
                    }
                    PathSvg { path: root.neckPath() }
                }
                ShapePath {
                    strokeColor: "transparent"
                    fillGradient: LinearGradient {
                        x1: 128; y1: 48; x2: 96; y2: 200
                        GradientStop { position: 0.25; color: root.coat }
                        GradientStop { position: 1; color: root.coatEnd }
                    }
                    PathSvg { path: "M62.03 81.97A6.72 6.72 0 0 1 66.78 80L108.68 80A4.16 4.16 0 0 0 111.62 78.78L152.69 37.71A1.28 1.28 0 0 1 154.51 37.71L182.81 66.01A4.16 4.16 0 0 1 182.97 71.73L138.08 121.77A8 8 0 0 1 135.01 123.89L114.07 132A0.96 0.96 0 0 0 114 133.76L128.36 140.67A12.8 12.8 0 0 0 138.95 140.91L189.4 119.29A4.16 4.16 0 0 1 195.2 123.11L195.2 141.43A4.16 4.16 0 0 1 192.9 145.15L153.5 164.85A20.08 20.08 0 0 0 142.4 182.81L142.4 193.89A1.92 1.92 0 0 1 140.09 195.77L71.34 181.33A3.2 3.2 0 0 1 68.8 178.2L68.8 147.84A3.84 3.84 0 0 0 64.96 144L64.64 144A3.84 3.84 0 0 0 60.8 147.84L60.8 172.63A4.16 4.16 0 0 1 58.5 176.35L20.42 195.39A4.16 4.16 0 0 1 14.4 191.67L14.4 132.38A6.72 6.72 0 0 1 16.37 127.63Z" }
                }
                ShapePath {
                    strokeColor: "transparent"
                    fillColor: Theme.rail.dogBark
                    PathSvg { path: "M191.44 64.69A2.56 2.56 0 0 1 190.58 61.53L198.85 43.3A2.56 2.56 0 0 1 202.65 42.26L212.02 48.82A2.56 2.56 0 0 1 212.34 52.75L198.03 66.75A2.56 2.56 0 0 1 194.77 67.02ZM202.18 79.32A2.56 2.56 0 0 1 203 76.16L226.52 59.21A2.56 2.56 0 0 1 230.34 60.21L236.92 74.32A2.56 2.56 0 0 1 235.23 77.88L207.12 85A2.56 2.56 0 0 1 204.18 83.6ZM204.82 97.22A2.56 2.56 0 0 1 207.2 94.89L229.49 93.45A2.56 2.56 0 0 1 232.21 96.23L231.33 106.23A2.56 2.56 0 0 1 228.18 108.49L206.48 103.2A2.56 2.56 0 0 1 204.53 100.49Z" }
                }
                ShapePath {
                    strokeColor: "transparent"
                    fillColor: root.details
                    PathSvg { path: "M152.69 37.71A1.28 1.28 0 0 1 154.51 37.71L166.57 49.77A1.92 1.92 0 0 1 166.5 52.55L162.17 56.44A6.4 6.4 0 0 1 157.1 58.04L148.4 56.95A6.4 6.4 0 0 1 146.13 56.22L140.65 53.23A1.92 1.92 0 0 1 140.21 50.19ZM82.4 101.6a8.8 8.8 0 1 0 17.6 0a8.8 8.8 0 1 0 -17.6 0Z" }
                }
                ShapePath {
                    strokeColor: "transparent"
                    fillColor: root.mixed(root.details, root.coat, 0.35)
                    PathSvg { path: "M154.87 42.73A1.6 1.6 0 0 1 157.13 42.73L161.1 46.7A1.6 1.6 0 0 1 160.92 49.11L160.24 49.62A1.6 1.6 0 0 1 158.22 49.55L154.09 45.93A1.6 1.6 0 0 1 154.01 43.59Z" }
                }
                ShapePath {
                    strokeColor: "transparent"
                    fillColor: root.coatMiddle
                    PathSvg { path: "M91.36 98.4a2.72 2.72 0 1 0 5.44 0a2.72 2.72 0 1 0 -5.44 0Z" }
                }
            }
        }
    }
}
