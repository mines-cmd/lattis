import QtQuick
import QtQuick.Controls.Basic

// A tag in its own color with its game count; clicking it asks for the games behind it.
TagChip {
    id: root

    property string countText

    signal clicked()

    detail: countText
    plus: false
    outlined: true
    outlineWidth: Theme.overview.tagBorderWidth
    textColor: Qt.hsla(tagColor.hslHue, tagColor.hslSaturation, Theme.overview.tagTextLightness, 1)
    detailColor: Qt.alpha(textColor, Theme.overview.tagCountOpacity)
    fillOpacity: hover.hovered ? Theme.overview.tagHoverOpacity : Theme.overview.tagOffOpacity
    lit: hover.hovered

    Behavior on fillOpacity { enabled: Motion.fades; MotionNumber { fade: true } }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler { onTapped: root.clicked() }
}
