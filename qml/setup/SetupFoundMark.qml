import QtQuick

Rectangle {
    property bool found

    opacity: found ? 1 : 0
    visible: opacity > 0
    Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
    implicitWidth: Theme.onboarding.foundIconSize
    implicitHeight: Theme.onboarding.foundIconSize
    radius: width / 2
    color: Qt.alpha(Theme.onboarding.foundColor, Theme.onboarding.stepDoneOpacity)

    Icon {
        anchors.centerIn: parent
        path: Icons.check
        size: Math.round(Theme.onboarding.foundIconSize / 2)
        color: Theme.onboarding.foundColor
    }
}
