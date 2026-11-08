import QtQuick
import QtTest
import "../../qml/components"

TestCase {
    id: test
    name: "JustifiedFlow"
    when: windowShown

    property var widths: []

    // A window of its own: the runner never shows the TestCase, so every item in it reads as hidden.
    Window {
        visible: true
        width: 100
        height: 100

        JustifiedFlow {
            id: flow
            width: 100
            spacing: 4

            Repeater {
                model: test.widths
                Item {
                    required property int modelData
                    implicitWidth: modelData
                    implicitHeight: 10
                }
            }
        }
    }

    function chips() {
        const out = []
        for (let i = 0; i < flow.children.length; ++i) {
            if (!(flow.children[i] instanceof Repeater))
                out.push(flow.children[i])
        }
        return out
    }

    function layOut(widths, maxRows) {
        flow.maxRows = maxRows
        test.widths = widths
        flow.relayout()
        return chips()
    }

    function test_fullRowsEndAtTheEdge() {
        // Rows: [30 30 30] [40 40] [25].
        const c = layOut([30, 30, 30, 40, 40, 25], -1)
        compare(c[0].x, 0)
        compare(c[2].x + c[2].width, 100)
        compare(c[3].y, 14)
        compare(c[4].x + c[4].width, 100)
        compare(c[4].x - (c[3].x + c[3].width), 4)
        verify(Math.abs(c[1].width - c[0].width) <= 1)
    }

    function test_lastRowStaysRagged() {
        const c = layOut([30, 30, 30, 40, 40, 25], -1)
        compare(c[5].y, 28)
        compare(c[5].width, 25)
        compare(flow.implicitHeight, 38)
    }

    function test_rowsPastMaxRowsAreHidden() {
        const c = layOut([30, 30, 30, 40, 40, 25], 2)
        verify(c[4].visible)
        verify(!c[5].visible)
        compare(flow.implicitHeight, 24)
        // The last shown row is cut short by maxRows, not by the list, so it still ends at the edge.
        compare(c[4].x + c[4].width, 100)
    }

    function test_roundingDoesNotAddUp() {
        const c = layOut([20, 20, 20, 21, 50], -1)
        compare(c[3].x + c[3].width, 100)
        for (const chip of c)
            compare(chip.x, Math.round(chip.x))
    }
}
