import QtQuick

// The frame of a card on Play statistics and Library overview.
Surface {
    radius: Theme.stats.cardRadius
    fill: Theme.stats.cardBackground
    fillEnd: Theme.stats.cardBackgroundEnd
    direction: Theme.layout.gradientDirection
    border.color: Theme.stats.cardBorder
    border.width: Theme.layout.borderWidth
}
