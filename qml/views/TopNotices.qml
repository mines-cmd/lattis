import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
Item {
    id: root

    property bool startedUp: false
    readonly property bool hovered: readOnlyNotice.hovered || (importNotice.item?.hovered ?? false)

    signal message(string text)

    NoticeBar {
        id: readOnlyNotice
        visible: Library.readOnly
        opacity: visible ? 1 : 0
        width: root.width
        height: Theme.layout.toolbarHeight
        text: Library.readOnlyNotice

        Behavior on opacity { enabled: Motion.fades && root.startedUp; MotionNumber { fade: true; speed: "normal" } }
    }

    // After an Import, below that one when both show. Loaded after the first frame: Backups is
    // made on first use.
    Loader {
        id: importNotice
        active: root.startedUp
        y: Library.readOnly ? Theme.layout.toolbarHeight + Theme.layout.spacing : 0
        width: root.width
        height: Theme.layout.toolbarHeight
        source: "ImportNotice.qml"
    }
    Connections {
        target: importNotice.item
        function onMessage(text) { root.message(text) }
    }
}
