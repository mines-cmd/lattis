pragma Singleton
import QtQuick

// Stand-ins for licensed/LicensedIcons.qml, which builds instead when present.
// An empty path makes Icons.forPlatform() fall back to the generic icon.
QtObject {
    readonly property var credits: []
    readonly property string home: "M20 20C20 20.5523 19.5523 21 19 21H5C4.44772 21 4 20.5523 4 20V11L1 11L11.3273 1.6115C11.7087 1.26475 12.2913 1.26475 12.6727 1.6115L23 11L20 11V20ZM11 13V19H13V13H11Z" // home-4-fill
    readonly property string wiiu: ""
    readonly property string wii: ""
    readonly property string gamecube: ""
    readonly property string penguin: ""
}
