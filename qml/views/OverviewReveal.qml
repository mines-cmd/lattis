import QtQuick
import QtQuick.Layouts

// A card that fades in a little after the one before it as the page opens.
Item {
    id: reveal

    default property alias content: holder.data
    property int order: 0
    // The card's name in [overview], which the page matches against the layout.
    property string name: ""
    property bool shown: false
    // Set by the page, which places the cards: the column it chose, and whether the card grows to end level with the other column.
    property int column: -1
    property bool stretch: false
    property bool seen: false

    signal menuRequested(point position)

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: point => reveal.menuRequested(point.position)
    }

    implicitWidth: holder.implicitWidth
    implicitHeight: holder.implicitHeight
    opacity: 0

    onStretchChanged: {
        const card = holder.children[0]
        if (card)
            card.Layout.fillHeight = stretch
    }

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
        height: parent.height
        spacing: 0
    }
}
