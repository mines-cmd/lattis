import QtQuick
import QtTest

// Motion.qml reads the C++ singletons Theme and General, which this runner doesn't have, so the
// test loads the file's source with those two reads pointed at `stub`.
TestCase {
    id: test
    name: "Motion"

    QtObject {
        id: stub
        property real durationScale: 1
        property bool reduceMotion: false
        property string easing: "smooth"
        property int slow: 400
        property int normal: 200
        property int fast: 100
    }

    property var motion: null

    function initTestCase() {
        const request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl("../../qml/components/Motion.qml"), false)
        request.send()
        const source = request.responseText
            .replace("pragma Singleton", "")
            .replace(/Theme\.motion\./g, "stub.")
            .replace(/General\.reduceMotion/g, "stub.reduceMotion")
        motion = Qt.createQmlObject(source, test, "motion-under-test")
        verify(motion !== null)
    }

    function init() {
        stub.durationScale = 1
        stub.reduceMotion = false
    }

    function test_full_motion() {
        compare(motion.on, true)
        compare(motion.fades, true)
        compare(motion.reduced, false)
        compare(motion.duration(200), 200)
        compare(motion.fadeDuration(200), 200)
    }

    function test_reduce_motion_keeps_fades() {
        stub.reduceMotion = true
        compare(motion.on, false)
        compare(motion.fades, true)
        compare(motion.reduced, true)
        compare(motion.duration(200), 0)
        compare(motion.fadeDuration(200), 200)
    }

    function test_duration_scale_zero_stops_everything() {
        stub.durationScale = 0
        compare(motion.on, false)
        compare(motion.fades, false)
        compare(motion.duration(200), 0)
        compare(motion.fadeDuration(200), 0)
    }

    function test_duration_scale_scales_both() {
        stub.durationScale = 1.5
        compare(motion.duration(200), 300)
        compare(motion.fadeDuration(200), 300)
        stub.reduceMotion = true
        compare(motion.duration(200), 0)
        compare(motion.fadeDuration(200), 300)
    }
}
