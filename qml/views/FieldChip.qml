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

    readonly property var field: fields.all[name]
    readonly property bool opensHistory: field.history ?? false
    readonly property bool opensUrl: (field.action ?? "") === "url"

    signal historyRequested()

    fill: carded ? Theme.grid.chipBackground : Theme.statChip.background
    fillEnd: carded ? Theme.grid.chipBackground : Theme.statChip.backgroundEnd
    border.color: attention ? (attentionColor ?? Theme.notice.border) : carded ? Theme.grid.chipBorder : Theme.statChip.border
    border.width: attention ? Theme.notice.borderWidth : carded ? Theme.grid.chipBorderWidth : Theme.statChip.borderWidth
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
