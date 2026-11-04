import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// A note under a field; `open` unfolds it, so one that appears while the editor is open
// doesn't push what's below it down at once.
Unfold {
    property alias text: note.text
    property alias color: note.color

    open: true
    visible: open || progress > 0
    Layout.fillWidth: true

    Label {
        id: note
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.hintColor
        font.pixelSize: Theme.gameEditor.hintFontSize
    }
}
