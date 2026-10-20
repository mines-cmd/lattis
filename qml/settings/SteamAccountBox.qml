import QtQuick

SettingsComboBox {
    id: root

    readonly property var steam: Library.steam
    signal failed(string error)

    readonly property var ids: [""].concat(steam.users.map(u => u.id))
    readonly property var recent: steam.users.find(u => u.mostRecent)

    enabled: steam.found && steam.users.length > 0
    model: [recent ? qsTr("Most recent login (%1)").arg(recent.name) : qsTr("Most recent login")]
           .concat(steam.users.map(u => u.name !== u.accountName && u.accountName !== ""
                                             ? qsTr("%1 (%2)").arg(u.name).arg(u.accountName) : u.name))
    currentIndex: Math.max(0, ids.indexOf(steam.user))
    onActivated: index => {
        const error = Library.setSteamValue("user", ids[index])
        if (error !== "")
            failed(error)
    }
}
