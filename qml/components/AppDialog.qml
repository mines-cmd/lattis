import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
MotionPopup {
    id: root

    property string title
    property string size: "small"
    required property bool carded
    readonly property bool windowLike: size !== "small"
    readonly property int sizeWidth: size === "large" ? Theme.gameEditor.width
                                   : size === "medium" ? Theme.gameEditor.backupWidth
                                   : Theme.gameEditor.confirmWidth

    default property alias content: body.data
    property alias footer: footerRow.data
    property alias footerStart: footerStartRow.data
    // Replaces the title text where the heading is more than a line of text (a game's icon and name).
    property Component header: null
    property bool scrim: true
    // False while the dialog does something its Escape would not interrupt: the ✕ goes too.
    property bool closable: true
    property bool footerVisible: true

    parent: Overlay.overlay
    // Not anchors.centerIn: a dialog whose content changes height after it opens (lists that
    // fill in) would stay where it was, as centerIn does not re-run then.
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: Math.min(Math.max(sizeWidth, footerRow.implicitWidth + leftPadding + rightPadding),
                    parent.width - 2 * Theme.gameEditor.padding)
    modal: true
    focus: true
    padding: Theme.gameEditor.padding
    Overlay.modal: Scrim { color: root.scrim ? Theme.gameEditor.scrim : "transparent" }

    background: Surface {
        radius: Theme.gameEditor.radius
        fill: root.carded ? Theme.gameEditor.background : Theme.gameEditor.plainBackground
        fillEnd: root.carded ? Theme.gameEditor.backgroundEnd : Theme.gameEditor.plainBackground
        direction: Theme.gameEditor.gradientDirection
        border.color: Theme.gameEditor.border
        border.width: Theme.gameEditor.borderWidth
    }

    contentItem: ColumnLayout {
        spacing: Theme.gameEditor.rowSpacing

        RowLayout {
            visible: root.title !== "" || root.windowLike || root.header !== null
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            Loader {
                active: root.header !== null
                visible: active
                Layout.fillWidth: true
                sourceComponent: root.header
            }
            Label {
                visible: root.header === null
                Layout.fillWidth: true
                text: root.title
                color: Theme.gameEditor.headingColor
                font.pixelSize: root.windowLike ? Theme.gameEditor.headingFontSize : Theme.font.headingSize
                font.weight: Theme.font.strongWeight
                wrapMode: Text.WordWrap
            }
            WindowControls {
                visible: root.windowLike && root.closable && buttons.length > 0
                Layout.alignment: Qt.AlignTop
                dialog: root
            }
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.gameEditor.rowSpacing
        }

        RowLayout {
            visible: root.footerVisible && (footerRow.children.length > 0 || footerStartRow.children.length > 0)
            Layout.fillWidth: true
            spacing: Theme.layout.spacing

            RowLayout {
                id: footerStartRow
                spacing: Theme.layout.spacing
            }
            Item { Layout.fillWidth: true }
            RowLayout {
                id: footerRow
                spacing: Theme.layout.spacing
            }
        }
    }
}
