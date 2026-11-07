import QtQuick

// See docs/code/qml.md.
Card {
    padding: Theme.overview.cardPadding
    radius: Theme.overview.cardRadius
    fill: Theme.overview.cardBackground
    fillEnd: Theme.overview.cardBackgroundEnd
    border.color: Theme.overview.cardBorder

    function percentText(part: real, whole: real): string {
        const percent = whole > 0 ? 100 * part / whole : 0
        return percent > 0 && percent < 0.5 ? qsTr("<1%") : qsTr("%1%").arg(Math.round(percent))
    }
}
