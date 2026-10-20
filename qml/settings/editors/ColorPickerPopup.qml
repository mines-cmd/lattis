import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MotionPopup {
    id: root

    signal colorPicked(color c)
    signal referencePicked(string reference, color c)
    signal removeRequested()

    property bool removable: false

    property real hue: 0
    property real sat: 1
    property real val: 1
    property real alpha: 1
    property bool settingFromColor: false
    property var themeColors: []
    property string reference: ""
    property string excludedName: "" // a palette color can't reference itself

    function setFromColor(c) {
        settingFromColor = true
        // A gray has no hue (-1); keep the strip where it was.
        if (c.hsvHue >= 0)
            hue = c.hsvHue
        sat = c.hsvSaturation
        val = c.hsvValue
        alpha = c.a
        settingFromColor = false
    }

    function openFor(anchorItem, initial, raw) {
        setFromColor(initial)
        reference = (typeof raw === "string" && raw.startsWith("@")) ? raw : ""
        themeColors = Theme.paletteColors().filter(t => t.name !== excludedName).slice(0, Theme.settingsWindow.colorPickerPaletteColumns
                                                     * Theme.settingsWindow.colorPickerPaletteRows)
        parent = anchorItem
        // Line up with the swatch's right edge (editors sit on the right), and open above it
        // when there's no room below. `margins` then keeps it inside the window regardless.
        // Whole pixels, or the outline blurs unevenly.
        x = Math.round(anchorItem.width - width)
        const below = anchorItem.mapToItem(null, 0, anchorItem.height).y + 4
        const fitsBelow = below + implicitHeight + margins <= anchorItem.Window.height
        y = Math.round(fitsBelow ? anchorItem.height + 4 : -implicitHeight - 4)
        open()
    }

    function currentColor() {
        return Qt.hsva(hue, sat, val, alpha)
    }

    function pickFromStrips() {
        if (settingFromColor)
            return
        reference = ""
        colorPicked(currentColor())
    }

    onHueChanged: pickFromStrips()
    onSatChanged: pickFromStrips()
    onValChanged: pickFromStrips()
    onAlphaChanged: pickFromStrips()

    padding: 12
    margins: 8
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    background: Rectangle {
        color: Theme.settingsWindow.colorPickerBackground
        border.color: Theme.settingsWindow.colorPickerBorder
        border.width: Theme.settingsWindow.colorPickerBorderWidth
        radius: Theme.settingsWindow.inputRadius
    }

    component Marker: Rectangle {
        color: "transparent"
        border.color: Theme.settingsWindow.colorPickerMarkerOutline
        border.width: 1
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            color: "transparent"
            radius: Math.max(0, parent.radius - 1)
            border.color: Theme.settingsWindow.colorPickerMarker
            border.width: 2
        }
    }

    component Checker: Canvas {
        property real radius: 0
        readonly property var look: [Theme.settingsWindow.checkerSize, Theme.settingsWindow.checkerLight,
                                     Theme.settingsWindow.checkerDark, radius]
        onLookChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.beginPath()
            ctx.roundedRect(0, 0, width, height, radius, radius)
            ctx.clip()
            const size = Theme.settingsWindow.checkerSize
            for (let y = 0; y < height; y += size) {
                for (let x = 0; x < width; x += size) {
                    ctx.fillStyle = ((x + y) / size) % 2 === 0 ? Theme.settingsWindow.checkerLight
                                                               : Theme.settingsWindow.checkerDark
                    ctx.fillRect(x, y, size, size)
                }
            }
        }
    }

    component StripMarker: Marker {
        required property Item strip
        required property real position
        x: -3
        y: position * strip.height - height / 2
        width: strip.width + 6
        height: 7
        radius: 2
    }

    component StripArea: MouseArea {
        signal picked(real position)
        anchors.fill: parent
        preventStealing: true
        onPressed: mouse => picked(Math.max(0, Math.min(1, mouse.y / height)))
        onPositionChanged: mouse => { if (pressed) picked(Math.max(0, Math.min(1, mouse.y / height))) }
    }

    ColumnLayout {
        width: parent.width
        spacing: Theme.settingsWindow.controlSpacing

        RowLayout {
            id: pickerRow
            spacing: Theme.settingsWindow.controlSpacing

            Item {
                id: svSquare
                Layout.preferredWidth: Theme.settingsWindow.colorPickerSize
                Layout.preferredHeight: Theme.settingsWindow.colorPickerSize

                Rectangle {
                    anchors.fill: parent
                    color: Qt.hsva(root.hue, 1, 1, 1)
                }
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: "#ffffffff" }
                        GradientStop { position: 1; color: "#00ffffff" }
                    }
                }
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#00000000" }
                        GradientStop { position: 1; color: "#ff000000" }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    preventStealing: true
                    onPositionChanged: mouse => { if (pressed) update(mouse) }
                    onPressed: mouse => update(mouse)
                    function update(mouse) {
                        root.sat = Math.max(0, Math.min(1, mouse.x / width))
                        root.val = 1 - Math.max(0, Math.min(1, mouse.y / height))
                    }
                }
                Marker {
                    x: root.sat * svSquare.width - width / 2
                    y: (1 - root.val) * svSquare.height - height / 2
                    width: 12; height: 12; radius: 6
                }
            }

            // Hue runs from 360° at the top to 0° at the bottom, as in Photoshop.
            Item {
                id: hueStrip
                Layout.preferredWidth: Theme.settingsWindow.colorPickerStripWidth
                Layout.preferredHeight: Theme.settingsWindow.colorPickerSize

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0 / 6; color: Qt.hsva(6 / 6, 1, 1, 1) }
                        GradientStop { position: 1 / 6; color: Qt.hsva(5 / 6, 1, 1, 1) }
                        GradientStop { position: 2 / 6; color: Qt.hsva(4 / 6, 1, 1, 1) }
                        GradientStop { position: 3 / 6; color: Qt.hsva(3 / 6, 1, 1, 1) }
                        GradientStop { position: 4 / 6; color: Qt.hsva(2 / 6, 1, 1, 1) }
                        GradientStop { position: 5 / 6; color: Qt.hsva(1 / 6, 1, 1, 1) }
                        GradientStop { position: 6 / 6; color: Qt.hsva(0 / 6, 1, 1, 1) }
                    }
                }
                StripArea { onPicked: position => root.hue = 1 - position }
                StripMarker { strip: hueStrip; position: 1 - root.hue }
            }

            Item {
                id: alphaStrip
                Layout.preferredWidth: Theme.settingsWindow.colorPickerStripWidth
                Layout.preferredHeight: Theme.settingsWindow.colorPickerSize

                Checker { anchors.fill: parent }
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0; color: Qt.hsva(root.hue, root.sat, root.val, 1) }
                        GradientStop { position: 1; color: Qt.hsva(root.hue, root.sat, root.val, 0) }
                    }
                }
                StripArea { onPicked: position => root.alpha = 1 - position }
                StripMarker { strip: alphaStrip; position: 1 - root.alpha }
            }
        }

        Grid {
            id: themeGrid
            readonly property real cell: Math.floor((pickerRow.implicitWidth - (columns - 1) * spacing) / columns)
            visible: root.themeColors.length > 0
            Layout.topMargin: 4
            columns: Theme.settingsWindow.colorPickerPaletteColumns
            spacing: 4

            Repeater {
                model: root.themeColors

                Item {
                    id: themeSwatch
                    required property var modelData
                    readonly property bool picked: root.reference === "@" + modelData.name
                    width: themeGrid.cell
                    height: themeGrid.cell

                    Checker {
                        anchors.fill: parent
                        radius: Theme.settingsWindow.colorPickerSwatchRadius
                    }
                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.settingsWindow.colorPickerSwatchRadius
                        color: themeSwatch.modelData.color
                        border.color: Theme.settingsWindow.colorPickerSwatchBorder
                        border.width: Theme.settingsWindow.colorPickerSwatchBorderWidth
                    }
                    Marker {
                        visible: opacity > 0
                        opacity: themeSwatch.picked ? 1 : 0
                        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                        anchors.fill: parent
                        anchors.margins: -2
                        radius: Theme.settingsWindow.colorPickerSwatchRadius + 2
                    }
                    MouseArea {
                        id: swatchMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.setFromColor(themeSwatch.modelData.color)
                            root.reference = "@" + themeSwatch.modelData.name
                            root.referencePicked(root.reference, themeSwatch.modelData.color)
                        }
                    }
                    ToolTip.visible: swatchMouse.containsMouse
                    ToolTip.delay: Theme.tooltip.delay
                    ToolTip.text: "@" + modelData.name
                }
            }
        }

        SettingsButton {
            visible: root.removable
            Layout.fillWidth: true
            Layout.topMargin: 4
            danger: true
            iconPath: Icons.close
            text: qsTr("Remove color")
            onClicked: {
                root.close()
                root.removeRequested()
            }
        }
    }
}
