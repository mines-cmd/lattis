import QtQuick
import QtTest

// KeyedListModel.qml times its leaves with Motion, which this runner doesn't have, so the test loads the
// file's source with fixed intervals.
TestCase {
    id: test
    name: "KeyedListModel"

    property string source

    function initTestCase() {
        const request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl("../../qml/components/KeyedListModel.qml"), false)
        request.send()
        source = request.responseText.replace(/Motion\.duration\(.*\)/g, "100")
    }

    function make() {
        const model = Qt.createQmlObject(source, test, "keyedlistmodel-under-test")
        verify(model !== null)
        return model
    }

    function keys(model) {
        const result = []
        for (let i = 0; i < model.count; ++i)
            result.push(model.get(i).key)
        return result
    }

    function entries(names) {
        return names.map(name => ({ key: name }))
    }

    function test_removal_in_scope_leaves() {
        const model = make()
        model.scope = "a"
        model.source = entries(["x", "y"])
        wait(0)
        model.source = entries(["x"])
        compare(keys(model), ["x", "y"])
        verify(model.get(1).leaving)
        verify(model.leaving)
        tryCompare(model, "count", 1)
        model.destroy()
    }

    function test_new_scope_replaces() {
        const model = make()
        model.scope = "a"
        model.source = entries(["x", "y"])
        wait(0)
        model.scope = "b"
        model.source = entries(["x"])
        compare(keys(model), ["x"])
        verify(!model.leaving)
        verify(model.rebuilt)
        model.destroy()
    }

    function test_correction_in_the_same_pass_as_a_new_scope_replaces() {
        const model = make()
        model.scope = "a"
        model.source = entries(["x", "update"])
        wait(0)
        model.scope = "b"
        model.source = entries(["x", "update"])
        model.source = entries(["x"])
        compare(keys(model), ["x"])
        verify(!model.leaving)
        wait(0)
        model.source = entries([])
        verify(model.get(0).leaving)
        model.destroy()
    }
}
