import QtQuick
import QtQuick.Layouts
import QtQuick.Effects

CardFrame {
    id: card

    default property alias content: inner.data
    property real padding: Theme.stats.cardPadding
    property real horizontalPadding: padding
    property string watermark
    // Off for the cards that swap between the views, which sit on one frame that stays, since
    // two crossfading frames would let the background show through halfway.
    property bool framed: true

    implicitHeight: inner.implicitHeight + 2 * padding
    fill: framed ? Theme.stats.cardBackground : "transparent"
    fillEnd: framed ? Theme.stats.cardBackgroundEnd : "transparent"
    border.width: framed ? Theme.stats.cardBorderWidth : 0

    Loader {
        anchors.fill: parent
        active: card.watermark !== "" && Theme.stats.tileIconSize > 0
        sourceComponent: Item {
            Item {
                id: watermarkLayer
                anchors.fill: parent
                clip: true
                visible: false
                layer.enabled: true

                Icon {
                    x: card.width + size * (Theme.stats.tileIconOverhang - 1)
                    y: card.height + size * (Theme.stats.tileIconOverhang - 1)
                    path: card.watermark
                    size: Theme.stats.tileIconSize
                    color: Theme.stats.tileIcon
                }
            }

            // Plain clip would cut the icon square across the card's rounded corner.
            Rectangle {
                id: watermarkMask
                anchors.fill: parent
                radius: card.radius
                visible: false
                layer.enabled: true
            }

            MultiEffect {
                anchors.fill: parent
                source: watermarkLayer
                maskEnabled: true
                maskSource: watermarkMask
                maskThresholdMin: 0.5
                maskSpreadAtMin: 1.0
                opacity: Theme.stats.tileIconOpacity
            }
        }
    }

    ColumnLayout {
        id: inner
        anchors.fill: parent
        anchors.topMargin: parent.padding
        anchors.bottomMargin: parent.padding
        anchors.leftMargin: parent.horizontalPadding
        anchors.rightMargin: parent.horizontalPadding
        spacing: Theme.layout.spacing * 2
    }
}
