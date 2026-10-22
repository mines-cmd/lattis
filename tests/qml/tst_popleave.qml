import QtQuick
import QtTest

// PopLeave.qml uses Motion, Theme and the Motion* animations, which this runner doesn't have, so the
// test loads the file's source with those pointed at `stub` and plain NumberAnimations.
TestCase {
    id: test
    name: "PopLeave"
    when: windowShown

    QtObject {
        id: stub
        property bool on: true
        property real chipScale: 0.5
        property int duration: 100
    }

    Item {
        id: chip
        property real popScale: 1
    }

    property string source

    function initTestCase() {
        const request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl("../../qml/components/PopLeave.qml"), false)
        request.send()
        source = request.responseText
            .replace("ParallelAnimation {", "ParallelAnimation {\n"
                     + "component MotionNumber: NumberAnimation { property string speed; duration: stub.duration }\n"
                     + "component MotionPop: NumberAnimation { property string speed; duration: stub.duration }\n")
            .replace(/Motion\.on/g, "stub.on")
            .replace(/Theme\.motion\.chipScale/g, "stub.chipScale")
    }

    function make(properties) {
        const leave = Qt.createQmlObject(source, test, "popleave-under-test")
        verify(leave !== null)
        leave.target = chip
        for (const name in properties)
            leave[name] = properties[name]
        return leave
    }

    function init() {
        stub.on = true
        chip.opacity = 1
        chip.scale = 1
        chip.popScale = 1
    }

    function test_leave_plays_to_the_end() {
        const leave = make({})
        leave.leaving = true
        tryCompare(chip, "opacity", 0)
        compare(chip.scale, stub.chipScale)
        leave.destroy()
    }

    function test_brought_back_mid_leave_comes_back() {
        const leave = make({})
        leave.leaving = true
        wait(stub.duration / 2)
        verify(chip.opacity < 1)
        leave.leaving = false
        tryCompare(chip, "opacity", 1)
        tryCompare(chip, "scale", 1)
        verify(!leave.running)
        leave.destroy()
    }

    function test_brought_back_after_leave_ended_comes_back() {
        const leave = make({})
        leave.leaving = true
        tryCompare(chip, "opacity", 0)
        leave.leaving = false
        tryCompare(chip, "opacity", 1)
        tryCompare(chip, "scale", 1)
        leave.destroy()
    }

    function test_without_animation_snaps_both_ways() {
        const leave = make({ animated: false })
        leave.leaving = true
        compare(chip.opacity, 0)
        compare(chip.scale, stub.chipScale)
        leave.leaving = false
        compare(chip.opacity, 1)
        compare(chip.scale, 1)
        leave.destroy()
    }

    function test_scale_property() {
        const leave = make({ scaleProperty: "popScale", animated: false })
        leave.leaving = true
        compare(chip.popScale, stub.chipScale)
        compare(chip.scale, 1)
        leave.leaving = false
        compare(chip.popScale, 1)
        leave.destroy()
    }

    Item {
        id: bound
        property bool dim: false
        opacity: dim ? 0.5 : 1
    }

    function test_snapping_keeps_the_targets_opacity_binding() {
        const leave = make({ animated: false })
        leave.target = bound
        leave.leaving = true
        compare(bound.opacity, 0)
        leave.leaving = false
        bound.dim = true
        compare(bound.opacity, 0.5)
        leave.destroy()
    }
}
