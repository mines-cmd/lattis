import QtQuick
import QtQuick.Layouts

// A card that fades in a little after the one before it as the page opens.
Item {
    id: reveal

    default property alias content: holder.data
    property int order: 0
    // The card's name in [overview], which the page matches against the layout; its title is what edit mode calls it.
    property string name: ""
    property string title: ""
    property bool shown: false
    // Set by the page, which places the cards: the column it chose, and whether the card grows to end level with the other column.
    property int column: -1
    property bool stretch: false
    property bool seen: false
    // Whether the card has anything to show. Edit mode shows one that hasn't as its title over a note.
    property bool has: true
    property bool editing: false
    property bool canDrag: true
    property bool dragged: false
    readonly property bool hiddenCard: name !== "" && General.overviewHidden.includes(name)
    readonly property bool placeholder: editing && !has
    property real contentOpacity: editing && dragged ? Theme.overview.editDragOpacity
        : editing && hiddenCard ? Theme.overview.editHiddenOpacity : 1

    signal menuRequested(point position)
    signal eyeClicked()
    signal dragStarted(point scene)
    signal dragMoved(point scene)
    signal dragEnded(bool dropped)

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: point => reveal.menuRequested(point.position)
    }

    visible: editing || (has && !hiddenCard)
    z: dragged ? 10 : 0
    implicitWidth: holder.implicitWidth
    implicitHeight: placeholder ? note.implicitHeight : holder.implicitHeight
    opacity: 0

    // The others slide aside while a card is dragged; the card under the pointer follows it directly.
    Behavior on x { enabled: Motion.on && reveal.editing && !reveal.dragged; MotionNumber { speed: "normal" } }
    Behavior on y { enabled: Motion.on && reveal.editing && !reveal.dragged; MotionNumber { speed: "normal" } }
    Behavior on contentOpacity { enabled: Motion.fades; MotionNumber { fade: true } }

    onStretchChanged: {
        const card = holder.children[0]
        if (card)
            card.Layout.fillHeight = stretch
        noteCard.Layout.fillHeight = stretch
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
        visible: !reveal.placeholder
        opacity: reveal.contentOpacity
    }

    ColumnLayout {
        id: note
        width: parent.width
        height: parent.height
        spacing: 0
        visible: reveal.placeholder
        opacity: reveal.contentOpacity

        OverviewCard {
            id: noteCard
            Layout.fillWidth: true

            OverviewCardTitle { text: reveal.title }
            OverviewQuiet {
                text: qsTr("Nothing to show now")
                color: Theme.overview.editPlaceholder
            }
        }
    }

    OverviewEditFrame {
        anchors.fill: parent
        editing: reveal.editing
        title: reveal.title
        hiddenCard: reveal.hiddenCard
        canDrag: reveal.canDrag
        onEyeClicked: reveal.eyeClicked()
        onDragStarted: scene => reveal.dragStarted(scene)
        onDragMoved: scene => reveal.dragMoved(scene)
        onDragEnded: dropped => reveal.dragEnded(dropped)
    }
}
