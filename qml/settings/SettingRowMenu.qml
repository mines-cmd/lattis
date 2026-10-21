import QtQuick
import QtQuick.Controls.Basic

SettingsMenu {
    id: root

    pops: true
    textColor: Theme.menu.rowMenuText

    required property var row
    property var info: ({})

    readonly property string target: row.linkTarget !== "" ? row.linkTarget : (info.defaultTarget ?? "")
    readonly property bool hasRevert: (info.revert ?? "") !== ""
    readonly property bool hasRemove: (info.file ?? "") !== ""
    readonly property bool hasDefault: info.fromParent ?? false
    readonly property bool hasFollowAgain: row.linkTarget === "" && !hasDefault && (info.defaultTarget ?? "") !== ""
    readonly property bool hasOwn: row.linkTarget !== ""
    readonly property bool hasLinkGroup: target !== "" || row.followers > 0 || hasOwn || hasFollowAgain
    readonly property bool hasValueGroup: hasRevert || hasRemove || hasDefault

    minWidth: hasLinkGroup || hasValueGroup ? Theme.menu.minWidth : 0

    function openAt(item, x, y) {
        info = Theme.rowMenu(row.section, row.key)
        if (item) {
            popDirection = "below"
            popup(item, x, y + 2)
        }
        // At the click, not the cursor: a reopening waits for the old menu's exit, and the pointer may have moved.
        else popup(row, x, y)
    }

    SettingsMenuItem {
        want: !root.hasLinkGroup && !root.hasValueGroup
        enabled: false
        text: qsTr("Nothing to change")
    }

    SettingsMenuItem {
        want: root.target !== ""
        iconPath: Icons.arrowRight
        text: qsTr("Go to %1").arg(root.target)
        onTriggered: root.row.goToRequested(root.target)
    }
    SettingsMenuItem {
        want: root.row.followers > 0
        iconPath: Icons.menuSearchLine
        text: qsTr("Show %1 that follow this").arg(root.row.followers)
        onTriggered: root.row.followersClicked()
    }
    SettingsMenuItem {
        want: root.hasOwn
        iconPath: Icons.unlink
        text: qsTr("Use own value")
        keys: root.info.current ?? ""
        onTriggered: {
            root.row.forceActiveFocus()
            Theme.useOwnValue(root.row.section, root.row.key)
        }
    }
    SettingsMenuItem {
        want: root.hasFollowAgain
        iconPath: Icons.link
        text: qsTr("Follow %1 again").arg(root.target)
        keys: "→ " + (root.info.defaultValue ?? "")
        onTriggered: {
            root.row.forceActiveFocus()
            Theme.useDefault(root.row.section, root.row.key)
        }
    }
    SettingsMenuSeparator { visible: root.hasLinkGroup && root.hasValueGroup }

    SettingsMenuItem {
        want: root.hasRevert
        iconPath: Icons.history
        text: qsTr("Revert to %1").arg(root.info.revert ?? "")
        keys: qsTr("when Settings opened")
        onTriggered: {
            root.row.forceActiveFocus() // the editor shows the new value only when it has let go of focus
            Theme.revertValue(root.row.section, root.row.key)
        }
    }
    SettingsMenuItem {
        want: root.hasRemove
        iconPath: Icons.close
        text: qsTr("Remove from %1").arg(root.info.file ?? "")
        keys: "→ " + (root.info.fallback ?? "")
        onTriggered: {
            root.row.forceActiveFocus()
            // First: with "Changed" on, the reset removes this row.
            root.row.resetClicked()
            Theme.resetValue(root.row.section, root.row.key)
        }
    }
    SettingsMenuItem {
        want: root.hasDefault
        iconPath: Icons.undo
        text: qsTr("Use default")
        keys: "→ " + (root.info.default ?? "")
        onTriggered: {
            root.row.forceActiveFocus()
            Theme.useDefault(root.row.section, root.row.key)
        }
    }
}
