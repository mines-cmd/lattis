import QtQuick
import QtQuick.Controls.Basic

// See docs/code/qml.md.
StatChip {
    id: root

    required property string name
    required property GameFields fields
    required property SteamPendingMenu steamMenu
    property bool leaving: false
    property bool carded: false
    // A cell of a strip that draws the card itself.
    property bool inStrip: false

    readonly property var field: fields.all[name]
    readonly property bool opensHistory: field.history ?? false
    readonly property bool opensUrl: (field.action ?? "") === "url"

    signal historyRequested()

    fill: inStrip ? "transparent" : carded ? Theme.grid.chipBackground : Theme.statChip.background
    fillEnd: inStrip ? "transparent" : carded ? Theme.grid.chipBackground : Theme.statChip.backgroundEnd
    border.color: attention ? (attentionColor ?? Theme.notice.border) : carded ? Theme.grid.chipBorder : Theme.statChip.border
    border.width: attention ? Theme.notice.borderWidth : inStrip ? 0 : carded ? Theme.grid.chipBorderWidth : Theme.statChip.borderWidth
    valueFirst: inStrip
    showIcon: inStrip ? Theme.grid.stripIcons : Theme.statChip.showIcons
    iconSize: inStrip ? Theme.grid.stripIconSize : Theme.statChip.iconSize
    valueFontSize: inStrip ? Theme.grid.stripValueFontSize : Theme.font.size
    labelFontSize: inStrip ? Theme.grid.stripLabelFontSize : Theme.statChip.labelFontSize
    uppercaseLabels: inStrip ? Theme.grid.stripUppercaseLabels : Theme.statChip.uppercaseLabels
    valueColor: inStrip ? Theme.grid.stripValueColor : Theme.statChip.valueColor
    labelColor: inStrip ? Theme.grid.stripLabelColor : Theme.statChip.labelColor
    hoverValueColor: inStrip ? Theme.grid.stripHoverValueColor : Theme.statChip.hoverValueColor
    hoverLabelColor: inStrip ? Theme.grid.stripHoverLabelColor : Theme.statChip.hoverLabelColor
    attention: field.attention ?? false
    attentionColor: field.attentionColor
    iconTint: field.iconTint
    icon: General.chipsWithoutIcon.includes(name) ? "" : field.icon
    label: field.label
    value: field.value
    clickable: !leaving && (opensHistory || opensUrl || (field.filterPlatform ?? "") !== "")
    tip: clickable ? field.tip ?? "" : ""
    tipStars: field.stars ?? -1
    ToolTip.visible: chipHover.hovered && (field.hoverTip ?? "") !== ""
    ToolTip.text: field.hoverTip ?? ""
    ToolTip.delay: 500
    onClicked: {
        if (opensHistory)
            historyRequested()
        else if (opensUrl)
            Qt.openUrlExternally(field.url)
        else
            Library.platformFilter = field.filterPlatform
    }

    HoverHandler { id: chipHover }

    SettingsButton {
        id: pendingButton
        visible: (root.field.action ?? "") === "pending"
        implicitHeight: Theme.statChip.buttonHeight
        flat: true
        leftPadding: text === "" ? 0 : 14
        rightPadding: leftPadding
        implicitWidth: text === "" ? implicitHeight : contentItem.implicitWidth + leftPadding + rightPadding
        text: GameRunner.closingSteam ? qsTr("Closing Steam") : ""
        iconPath: GameRunner.closingSteam ? "" : Icons.chevronRight
        iconRotation: root.steamMenu.showing ? -90 : 90
        enabled: !GameRunner.closingSteam
        tip: root.field.tip ?? ""
        onClicked: root.steamMenu.popup(pendingButton, 0, pendingButton.height)
    }
}
