import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Install, taking the first route; with two routes, the arrow opens a menu naming each by its source.
SettingsButton {
    id: root

    required property string emulatorId
    // The emulator's Installer.emulators row.
    required property var info

    readonly property var routes: info?.routes ?? []
    readonly property bool choice: routes.length > 1
    readonly property real arrowWidth: Theme.controls.iconSize + Theme.controls.controlSpacing * 2

    function routeLabel(route) {
        return route === "flatpak" ? qsTr("Flatpak from Flathub") : qsTr("AppImage from %1").arg(info?.source ?? "")
    }

    text: !choice && routes[0] === "flatpak" ? qsTr("Install Flatpak") : qsTr("Install")
    tip: choice ? "" : routeLabel(routes[0])
    rightPadding: choice ? arrowWidth + Theme.controls.controlSpacing : leftPadding
    onClicked: Installer.install(emulatorId, routes[0])

    AbstractButton {
        id: arrow
        visible: root.choice
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.arrowWidth
        hoverEnabled: true
        focusPolicy: Qt.NoFocus
        onClicked: menu.popup(root, root.width - menu.width, root.height)

        ToolTip.visible: hovered && !menu.visible
        ToolTip.delay: Theme.tooltip.delay
        ToolTip.text: qsTr("Choose how to install it")

        HoverHandler { cursorShape: Qt.PointingHandCursor }

        background: Item {
            Rectangle {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(1, Theme.controls.inputBorderWidth)
                height: parent.height / 2
                color: Theme.controls.inputBorder
            }
        }
        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                path: Icons.chevron
                size: Theme.controls.iconSize
                color: root.textColor
                rotation: menu.showing ? -90 : 90

                Behavior on rotation { enabled: Motion.on; MotionNumber {} }
            }
        }
    }

    SettingsMenu {
        id: menu

        Instantiator {
            model: root.routes
            delegate: SettingsMenuItem {
                required property string modelData
                required property int index
                text: index === 0 ? qsTr("%1 (recommended)").arg(root.routeLabel(modelData))
                                  : modelData === "flatpak" ? qsTr("%1 (updated by your system)").arg(root.routeLabel(modelData))
                                                            : root.routeLabel(modelData)
                onTriggered: Installer.install(root.emulatorId, modelData)
            }
            onObjectAdded: (index, object) => menu.insertItem(index, object)
            onObjectRemoved: (index, object) => menu.removeItem(object)
        }
    }
}
