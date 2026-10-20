import QtQuick

SettingsTextField {
    id: root

    property int minDigits: 2

    signal stepped(int direction)

    implicitWidth: Math.ceil(Math.max(digits.advanceWidth, placeholder.advanceWidth)) + leftPadding + rightPadding
    leftPadding: Theme.gameEditor.numberFieldPadding
    rightPadding: Theme.gameEditor.numberFieldPadding
    horizontalAlignment: TextInput.AlignHCenter
    // After the click that focused it has placed the cursor.
    onActiveFocusChanged: if (activeFocus) Qt.callLater(selectAll)
    Keys.onUpPressed: stepped(1)
    Keys.onDownPressed: stepped(-1)

    TextMetrics {
        id: digits
        font: root.font
        text: "0".repeat(Math.max(root.minDigits, root.text.length))
    }
    TextMetrics {
        id: placeholder
        font: root.font
        text: root.placeholderText
    }

    WheelHandler {
        enabled: root.activeFocus
        onWheel: event => root.stepped(Math.sign(event.angleDelta.y))
    }
}
