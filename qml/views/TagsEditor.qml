import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: root

    property var tags: []
    property var created: ({})
    property var recolored: ({})
    property var marked: []
    // Each reset() starts a new set of chips, which shows at once.
    property int generation: 0

    readonly property var libraryTags: Library.tags

    function reset(list) {
        ++generation
        tags = list.map(t => ({ name: t.name, color: t.color }))
        created = {}
        recolored = {}
        marked = []
        addPopup.close()
    }

    function names() { return tags.map(t => t.name) }

    function nextColor() {
        const size = Theme.tags.palette.length
        const used = new Set(libraryTags.filter(t => t.listed).map(t => t.color).concat(Object.values(created)))
        for (let i = 0; i < size; ++i) {
            if (!used.has(i))
                return i
        }
        return 0
    }

    function add(name) {
        if (name === "" || tags.some(t => t.name === name))
            return
        const known = libraryTags.find(t => t.name === name)
        let color = known?.color ?? created[name]
        if (color === undefined) {
            color = nextColor()
            const c = Object.assign({}, created)
            c[name] = color
            created = c
        }
        tags = tags.concat([{ name: name, color: color }])
    }

    function addSuggested(names) {
        const added = names.filter(name => !tags.some(t => t.name === name))
        tags = tags.concat(added.map(name => ({ name: name, color: libraryTags.find(t => t.name === name)?.color ?? 0 })))
        marked = marked.concat(added)
    }

    function removeSuggested() {
        for (const name of marked)
            remove(name)
    }

    function remove(name) {
        tags = tags.filter(t => t.name !== name)
        marked = marked.filter(m => m !== name)
        if (created[name] !== undefined) {
            const c = Object.assign({}, created)
            delete c[name]
            created = c
        }
    }

    function recolor(name, color) {
        tags = tags.map(t => t.name === name ? { name: name, color: color } : t)
        if (created[name] !== undefined) {
            const c = Object.assign({}, created)
            c[name] = color
            created = c
        } else {
            const r = Object.assign({}, recolored)
            r[name] = color
            recolored = r
        }
    }

    function apply() {
        const errors = []
        for (const name of Object.keys(created)) {
            if (tags.some(t => t.name === name)) {
                const e = Library.addTag(name, created[name])
                if (e !== "")
                    errors.push(e)
            }
        }
        for (const name of Object.keys(recolored)) {
            const e = Library.setTagColor(name, recolored[name])
            if (e !== "")
                errors.push(e)
        }
        return errors
    }

    spacing: Theme.layout.spacing

    Label {
        visible: root.marked.length > 0
        Layout.fillWidth: true
        text: qsTr("Tags from IGDB are marked with a dot")
        color: Theme.gameEditor.markerColor
        font.pixelSize: Theme.gameEditor.markerFontSize
        elide: Text.ElideRight
    }
    KeyedListModel {
        id: tagModel
        leaveSpeed: "fast"
        scope: root.generation
        source: root.tags.map(t => ({ key: t.name, name: t.name, color: t.color }))
    }
    Flow {
        Layout.fillWidth: true
        spacing: Theme.tags.spacing

        add: Transition {
            enabled: Motion.on && !tagModel.rebuilt
            MotionNumber { property: "opacity"; from: 0; to: 1 }
            MotionPop { property: "scale"; from: Theme.motion.chipScale; to: 1; speed: "normal" }
        }
        move: Transition {
            enabled: Motion.on && !tagModel.rebuilt
            MotionNumber { properties: "x,y"; speed: "normal" }
        }

        Repeater {
            model: tagModel

            TagChip {
                id: chip
                required property var model
                readonly property bool leaving: model.leaving
                name: model.name
                colorIndex: model.color
                enabled: !leaving
                dot: root.marked.includes(model.name)
                dotColor: Theme.gameEditor.suggestedTagDot
                dotSize: Theme.gameEditor.suggestedTagDotSize
                removable: true
                removeTip: qsTr("Remove %1 from this game").arg(model.name)
                onRemoved: root.remove(model.name)
                raised: chipMouse.containsMouse || chip.removeHovered
                raiseBy: Theme.gameEditor.tagHoverLift
                pressed: chipMouse.pressed && !chip.removeHovered

                PopLeave { target: chip; leaving: chip.leaving }

                MouseArea {
                    id: chipMouse
                    anchors.fill: parent
                    anchors.bottomMargin: -Theme.gameEditor.tagHoverLift
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: paletteMenu.openForChip(chip, chip.model.name, chip.model.color)
                }
                ToolTip.visible: chipMouse.containsMouse && !chip.removeHovered
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: root.marked.includes(chip.model.name)
                              ? qsTr("Suggested by IGDB. Click to change the color of %1 on every game").arg(chip.model.name)
                              : qsTr("Change the color of %1 on every game").arg(chip.model.name)
            }
        }

        Rectangle {
            id: addChip
            implicitWidth: addRow.implicitWidth + 2 * Theme.tags.padding
            implicitHeight: Theme.tags.height
            radius: Theme.tags.radius
            color: addMouse.containsMouse || addPopup.visible ? Theme.gameEditor.addTagHover : "transparent"
            border.color: Theme.controls.inputBorder
            border.width: Theme.tags.outlineWidth
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: addLabel.text
            Keys.onSpacePressed: addPopup.openFor(addChip)
            Keys.onReturnPressed: addPopup.openFor(addChip)

            Behavior on color { enabled: Motion.fades; MotionColor {} }

            Row {
                id: addRow
                anchors.centerIn: parent
                spacing: Theme.tags.fontSize / 4

                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    path: Icons.add
                    size: Theme.tags.fontSize
                    color: Theme.gameEditor.labelColor
                }
                Label {
                    id: addLabel
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Add tag")
                    color: Theme.gameEditor.labelColor
                    font.pixelSize: Theme.tags.fontSize
                    font.weight: Theme.font.emphasisWeight
                }
            }
            MouseArea {
                id: addMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: addPopup.openFor(addChip)
            }
            ToolTip.visible: addMouse.containsMouse && !addPopup.visible
            ToolTip.delay: Theme.tooltip.delay
            ToolTip.text: qsTr("Add one of your tags to this game, or create a new one")
        }
    }

    TagAddPopup {
        id: addPopup
        editor: root
    }

    TagPalettePopup {
        id: paletteMenu
        property string tag
        onPicked: index => root.recolor(tag, index)
        function openForChip(item, name, color) {
            tag = name
            openFor(item, color)
        }
    }
}
