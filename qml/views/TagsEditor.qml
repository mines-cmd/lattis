import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: root

    property var tags: []
    property var created: ({})
    property var recolored: ({})
    property bool creating: false
    property var marked: []
    // Each reset() starts a new set of chips, which shows at once.
    property int generation: 0

    readonly property var libraryTags: Library.tags
    readonly property string typed: input.text.trim()
    readonly property var suggestions: libraryTags.filter(t => !tags.some(c => c.name === t.name)
                                                             && t.name.toLowerCase().includes(typed.toLowerCase()))
    readonly property string caseTwin: {
        if (typed === "" || libraryTags.some(t => t.name === typed))
            return ""
        return libraryTags.find(t => t.name.toLowerCase() === typed.toLowerCase())?.name ?? ""
    }

    function reset(list) {
        ++generation
        tags = list.map(t => ({ name: t.name, color: t.color }))
        created = {}
        recolored = {}
        marked = []
        input.text = ""
        creating = false
    }

    function newTag() {
        creating = true
        input.forceActiveFocus()
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
        input.text = ""
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

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.layout.spacing

        Caption {
            Layout.fillWidth: root.marked.length === 0
            text: qsTr("On this game")
        }
        Label {
            visible: root.marked.length > 0
            Layout.fillWidth: true
            text: qsTr("from IGDB")
            color: Theme.gameEditor.markerColor
            font.pixelSize: Theme.gameEditor.markerFontSize
            elide: Text.ElideRight
        }
    }
    KeyedListModel {
        id: tagModel
        leaveSpeed: "fast"
        scope: root.generation
        source: root.tags.map(t => ({ key: t.name, name: t.name, color: t.color }))
    }
    Flow {
        visible: tagModel.count > 0
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
    }
    Note {
        visible: tagModel.count === 0
        text: qsTr("No tags yet")
    }

    Caption {
        visible: root.libraryTags.length > root.tags.length
        Layout.topMargin: Theme.layout.spacing
        text: qsTr("Your other tags")
    }
    Flow {
        visible: root.suggestions.length > 0
        Layout.fillWidth: true
        spacing: Theme.tags.spacing

        Repeater {
            model: root.suggestions

            TagChip {
                id: suggestion
                required property var modelData
                name: modelData.name
                colorIndex: modelData.color
                outlined: true
                outlineWidth: Theme.filters.tagBorderWidth
                plus: false
                textColor: Qt.hsla(suggestion.tagColor.hslHue, suggestion.tagColor.hslSaturation, Theme.filters.tagTextLightness, 1)
                detailColor: Qt.alpha(textColor, Theme.filters.tagCountOpacity)
                fillOpacity: suggestionMouse.containsMouse ? Theme.gameEditor.suggestionHoverFillOpacity
                                                           : Theme.gameEditor.suggestionFillOpacity
                raised: suggestionMouse.containsMouse
                raiseBy: Theme.gameEditor.tagHoverLift
                pressed: suggestionMouse.pressed

                MouseArea {
                    id: suggestionMouse
                    anchors.fill: parent
                    anchors.bottomMargin: -Theme.gameEditor.tagHoverLift
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.add(suggestion.modelData.name)
                }
                ToolTip.visible: suggestionMouse.containsMouse
                ToolTip.delay: Theme.tooltip.delay
                ToolTip.text: qsTr("Add %1 to this game").arg(suggestion.modelData.name)
            }
        }
    }
    Note {
        visible: root.typed !== "" && root.suggestions.length === 0 && root.libraryTags.length > root.tags.length
        text: qsTr("None match \"%1\"").arg(root.typed)
    }

    SettingsTextField {
        id: input
        visible: root.creating
        Layout.fillWidth: true
        Layout.topMargin: Theme.layout.spacing
        placeholderText: qsTr("Name of the new tag")
        onAccepted: root.add(root.typed)
        onActiveFocusChanged: {
            if (!activeFocus && text.trim() === "")
                root.creating = false
        }
    }

    Label {
        visible: root.caseTwin !== ""
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.warningColor
        text: qsTr("There's already a tag called %1; tags are case-sensitive, so Enter adds %2 as another one.")
                  .arg(root.caseTwin).arg(root.typed)
    }

    component Caption: Label {
        Layout.fillWidth: true
        color: Theme.gameEditor.labelColor
    }
    component Note: Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.gameEditor.hintColor
        font.pixelSize: Theme.gameEditor.hintFontSize
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
