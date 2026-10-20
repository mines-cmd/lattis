import QtQuick

// See docs/code/qml.md.
Item {
    id: root

    // Paused rather than stopped while hidden, so the wall's rows don't jump back on return.
    property bool active: true

    Rectangle {
        anchors.fill: parent
        color: Theme.onboarding.background
    }

    Loader {
        anchors.fill: parent
        source: "SetupCoverWall.qml"
        onLoaded: {
            item.active = Qt.binding(() => root.active)
            item.tint = Qt.binding(() => Theme.onboarding.backdropTint)
            item.dim = Qt.binding(() => Theme.onboarding.backdropDim)
            item.vignette = Qt.binding(() => Theme.onboarding.backdropVignette)
        }
    }
}
