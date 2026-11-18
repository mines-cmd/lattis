import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Label {
    property bool indented: true

    Layout.fillWidth: true
    leftPadding: indented ? Theme.controls.checkboxSize + Theme.layout.spacing : 0
    wrapMode: Text.WordWrap
    color: Theme.gameEditor.hintColor
    font.pixelSize: Theme.gameEditor.hintFontSize
}
