import QtQuick
import QtQuick.Layouts

// A card that fades in a little after the one before it as the page opens.
Item {
    id: reveal

    default property alias content: holder.data
    property int order: 0
    property bool shown: false

    implicitWidth: holder.implicitWidth
    implicitHeight: holder.implicitHeight
    opacity: 0

    onShownChanged: {
        if (shown) {
            fadeIn.restart()
        } else {
            fadeIn.stop()
            opacity = 0
        }
    }

    SequentialAnimation {
        id: fadeIn
        PauseAnimation { duration: Motion.on ? reveal.order * Theme.overview.cardStagger : 0 }
        MotionNumber { target: reveal; property: "opacity"; to: 1; speed: "normal"; fade: true; linear: true }
    }

    ColumnLayout {
        id: holder
        width: parent.width
        spacing: 0
    }
}
