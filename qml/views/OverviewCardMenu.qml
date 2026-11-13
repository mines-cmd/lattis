import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
SettingsMenu {
    id: root

    property string card
    property bool canLeft: false
    property bool canRight: false
    property bool canUp: false
    property bool canDown: false
    property bool canBelow: false
    property bool canAbove: false

    signal hideRequested()
    signal leftRequested()
    signal rightRequested()
    signal upRequested()
    signal downRequested()
    signal belowRequested()
    signal aboveRequested()
    signal customizeRequested()

    atPointer: true

    SettingsMenuItem { text: qsTr("Hide card"); onTriggered: root.hideRequested() }
    SettingsMenuItem { want: root.canLeft; text: qsTr("Move to left column"); onTriggered: root.leftRequested() }
    SettingsMenuItem { want: root.canRight; text: qsTr("Move to right column"); onTriggered: root.rightRequested() }
    SettingsMenuItem { want: root.canUp; text: qsTr("Move up"); onTriggered: root.upRequested() }
    SettingsMenuItem { want: root.canDown; text: qsTr("Move down"); onTriggered: root.downRequested() }
    SettingsMenuItem { want: root.canBelow; text: qsTr("Move below the cards"); onTriggered: root.belowRequested() }
    SettingsMenuItem { want: root.canAbove; text: qsTr("Move above the cards"); onTriggered: root.aboveRequested() }
    SettingsMenuItem { text: qsTr("Customize overview"); onTriggered: root.customizeRequested() }
}
