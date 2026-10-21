import QtQuick
import QtQuick.Controls.Basic

Item {
    id: root

    property var art: ({})
    property var pending: ({})

    function reset(urls) {
        art = urls
        pending = {}
    }

    function shown(kind) {
        return pending[kind] !== undefined ? pending[kind] : (art[kind] ?? "")
    }

    function set(kind, file) {
        const p = Object.assign({}, pending)
        p[kind] = file
        pending = p
    }

    function picked() {
        const out = {}
        for (const kind of Object.keys(pending)) {
            if (pending[kind] !== "")
                out[kind] = pending[kind]
        }
        return out
    }

    function apply(id) {
        const errors = []
        for (const kind of Object.keys(pending)) {
            const e = pending[kind] === "" ? Library.removeArt(id, kind) : Library.setArt(id, kind, pending[kind])
            if (e !== "")
                errors.push(e)
        }
        return errors
    }

    readonly property real heroAspect: 3
    readonly property real coverAspect: 2 / 3
    readonly property real logoAspect: 2.5
    readonly property real iconAspect: 1
    readonly property real spacing: Theme.gameEditor.artSpacing
    readonly property real gap: Theme.layout.spacing / 2
    readonly property real header: nameMetrics.implicitHeight + gap
    readonly property real smallAspect: logoAspect + iconAspect
    readonly property real leftWidth: Math.max(0, (width - spacing - coverAspect * (spacing - spacing / smallAspect + header))
                                                  / (1 + coverAspect * (1 / heroAspect + 1 / smallAspect)))
    readonly property real heroHeight: leftWidth / heroAspect
    readonly property real smallHeight: Math.max(0, (leftWidth - spacing) / smallAspect)
    readonly property real coverHeight: heroHeight + spacing + header + smallHeight

    implicitHeight: header + coverHeight

    Label {
        id: nameMetrics
        visible: false
        text: "Hg"
    }

    ArtCell {
        kind: "hero"
        name: qsTr("Hero")
        previewWidth: root.leftWidth
        previewHeight: root.heroHeight
    }
    ArtCell {
        x: root.leftWidth + root.spacing
        kind: "cover"
        name: qsTr("Cover")
        previewWidth: root.coverHeight * root.coverAspect
        previewHeight: root.coverHeight
    }
    ArtCell {
        y: root.header + root.heroHeight + root.spacing
        kind: "logo"
        name: qsTr("Logo")
        previewWidth: root.smallHeight * root.logoAspect
        previewHeight: root.smallHeight
    }
    ArtCell {
        x: root.smallHeight * root.logoAspect + root.spacing
        y: root.header + root.heroHeight + root.spacing
        kind: "icon"
        name: qsTr("Icon")
        previewWidth: root.smallHeight * root.iconAspect
        previewHeight: root.smallHeight
    }

    Loader {
        id: picker
        property string kind
        active: false
        sourceComponent: ImageFileDialog {
            title: qsTr("Choose art")
            onPicked: file => root.set(picker.kind, file)
            onClosed: picker.active = false
        }
    }

    component ArtCell: Item {
        id: cell

        property string kind
        property string name
        property real previewWidth
        property real previewHeight
        readonly property string source: root.shown(kind)

        width: previewWidth
        height: root.header + previewHeight

        Label {
            text: cell.name
            color: Theme.gameEditor.labelColor
        }
        Item {
            id: preview
            y: root.header
            width: cell.previewWidth
            height: cell.previewHeight

            HoverHandler { id: hover }

            ArtThumb {
                anchors.fill: parent
                cornerRadius: Theme.radius.small
                // Logos and icons are cut out on transparency; cropping would lose their edges.
                fillMode: cell.kind === "logo" || cell.kind === "icon" ? Image.PreserveAspectFit : Image.PreserveAspectCrop
                source: cell.source
            }
            Label {
                anchors.centerIn: parent
                visible: cell.source === "" && !hover.hovered
                text: qsTr("None")
                color: Theme.gameEditor.hintColor
            }
            Rectangle {
                anchors.fill: parent
                opacity: hover.hovered ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                radius: Theme.radius.small
                color: Theme.gameEditor.artHoverOverlay
                Icon {
                    anchors.centerIn: parent
                    path: Icons.folderImageFill
                    size: Theme.controls.iconSize * 1.5
                    color: Theme.gameEditor.artHoverIcon
                }
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    picker.kind = cell.kind
                    picker.active = true
                }
                ToolTip.visible: hover.hovered && !remove.hovered
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: qsTr("Pick an image file for the %1").arg(cell.name.toLowerCase())
            }
            IconButton {
                id: remove
                readonly property bool hovered: removeHover.hovered
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: Theme.layout.spacing / 2
                implicitWidth: Theme.controls.iconSize + Theme.layout.spacing
                implicitHeight: implicitWidth
                iconSize: Theme.controls.iconSize
                radius: Theme.radius.tiny
                fill: removeHover.hovered ? Theme.gameEditor.artRemoveHoverBackground : Qt.alpha(Theme.gameEditor.artRemoveHoverBackground, 0)
                fillEnd: removeHover.hovered ? Theme.gameEditor.artRemoveHoverBackground : Qt.alpha(Theme.gameEditor.artRemoveHoverBackground, 0)
                border.width: 0
                iconColor: Theme.gameEditor.artRemoveIcon
                opacity: cell.source !== "" && hover.hovered ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                icon: Icons.close
                tip: qsTr("Remove the %1").arg(cell.name.toLowerCase())
                onClicked: root.set(cell.kind, "")
                HoverHandler { id: removeHover }
            }
        }
    }
}
