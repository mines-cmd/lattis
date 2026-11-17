import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects

Surface {
    id: root

    property bool installed: true
    property bool rom: false
    property bool gameNotFound: false
    property bool installable: false
    property bool locatable: false
    property bool running: false
    property string tip
    property string updateState: "none"
    // The game's own file is being replaced: reads like an update in progress and can't be pressed.
    property bool updateBlocks: false
    // Set when the game's emulator isn't found but can be installed: the button installs it.
    property string emulatorName
    // What the emulator's install is doing ("45 %", "Unpacking"); empty when it isn't running.
    property string emulatorStep
    // A surface that wants its own Launch button look sets these; the page follows the theme's launch_button.
    property int buttonHeight: Theme.launchButton.height
    property int buttonRadius: Theme.launchButton.radius
    property color buttonBorder: Theme.launchButton.border
    property int buttonBorderWidth: Theme.launchButton.borderWidth
    property color idleBackground: Theme.launchButton.background
    property color idleBackgroundEnd: Theme.launchButton.backgroundEnd
    property color idleText: Theme.launchButton.text
    property string idleLabel: Theme.launchButton.label
    property int buttonFontSize: Theme.launchButton.fontSize
    property bool buttonBold: Theme.launchButton.bold

    readonly property bool emulatorBusy: installed && emulatorName !== "" && emulatorStep !== ""
    readonly property bool emulatorInstall: installed && emulatorName !== "" && emulatorStep === ""
    readonly property bool installing: (!installed && installable) || emulatorInstall
    readonly property bool launchable: (installed || installable || locatable) && !running && !emulatorBusy && !updateBlocks
    readonly property bool updateAction: installed && !running && updateState === "waiting"
    readonly property bool updateBusy: emulatorBusy || (updateBlocks && installed && !running) || ((installed || installable) && !running && !updateAction
                                       && (updateState === "downloading" || updateState === "paused"))

    readonly property bool stopping: running && mouse.containsMouse

    signal clicked()
    signal stopRequested()

    implicitWidth: emulatorName !== "" ? Math.max(Theme.launchButton.width, label.implicitWidth + 32) : Theme.launchButton.width
    implicitHeight: buttonHeight
    radius: buttonRadius
    fill: {
        const base = stopping ? Theme.launchButton.stopBackground : running ? Theme.launchButton.runningBackground
                   : updateBusy ? Theme.launchButton.updatingBackground
                   : installing ? Theme.launchButton.installBackground
                   : !installed ? Theme.launchButton.uninstalledBackground
                   : updateAction ? Theme.launchButton.updateBackground
                   : root.idleBackground
        return (launchable || running) && mouse.containsMouse ? Qt.lighter(base, Theme.launchButton.hoverBrightness) : base
    }
    fillEnd: {
        const base = stopping ? Theme.launchButton.stopBackgroundEnd : running ? Theme.launchButton.runningBackgroundEnd
                   : updateBusy ? Theme.launchButton.updatingBackgroundEnd
                   : installing ? Theme.launchButton.installBackgroundEnd
                   : !installed ? Theme.launchButton.uninstalledBackgroundEnd
                   : updateAction ? Theme.launchButton.updateBackgroundEnd
                   : root.idleBackgroundEnd
        return (launchable || running) && mouse.containsMouse ? Qt.lighter(base, Theme.launchButton.hoverBrightness) : base
    }
    direction: Theme.launchButton.gradientDirection
    border.color: buttonBorder
    border.width: buttonBorderWidth
    opacity: updateBlocks ? Theme.launchButton.disabledOpacity : 1
    scale: mouse.pressed && (launchable || running) ? Theme.motion.pressScale : 1

    Behavior on scale { enabled: Motion.on; MotionNumber {} }
    Behavior on fill { enabled: Motion.fades; MotionColor {} }
    Behavior on fillEnd { enabled: Motion.fades; MotionColor {} }

    Label {
        id: label
        anchors.centerIn: parent
        width: Math.min(implicitWidth, parent.width - 16)
        elide: Text.ElideRight
        text: root.stopping && Theme.launchButton.stopLabel !== "" ? Theme.launchButton.stopLabel : root.running ? Theme.launchButton.runningLabel
            : root.updateAction ? Theme.launchButton.updateLabel
            : root.emulatorBusy ? root.emulatorStep
            : root.emulatorInstall ? qsTr("%1 %2").arg(Theme.launchButton.installLabel).arg(root.emulatorName)
            : root.updateBusy ? (root.updateState === "paused" ? Theme.launchButton.pausedLabel
                                 : root.installed ? Theme.launchButton.updatingLabel : Theme.launchButton.installingLabel)
            : root.installed ? root.idleLabel
            : root.installing ? Theme.launchButton.installLabel
            : root.rom ? Theme.launchButton.missingRomLabel
            : root.gameNotFound ? Theme.launchButton.missingGameLabel : Theme.launchButton.uninstalledLabel
        color: root.running ? Theme.launchButton.runningText
             : root.updateAction ? Theme.launchButton.updateText
             : root.updateBusy ? Theme.launchButton.updatingText
             : root.emulatorInstall ? Theme.launchButton.installText
             : root.installed ? root.idleText
             : root.installing ? Theme.launchButton.installText : Theme.launchButton.uninstalledText
        font.pixelSize: root.installed || root.installing || root.running ? root.buttonFontSize
                                                        : Math.round(root.buttonFontSize * 0.78)
        font.bold: root.buttonBold

        property real shadowShown: (root.launchable || root.running) && mouse.containsMouse ? 1 : 0
        Behavior on shadowShown { enabled: Motion.fades; MotionNumber { fade: true } }
        // Only while it shows: a layer per label would cost a texture for nothing the rest of the time.
        layer.enabled: shadowShown > 0 && Theme.launchButton.hoverTextShadow.a > 0
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Theme.launchButton.hoverTextShadow
            shadowOpacity: label.shadowShown
            shadowBlur: Theme.launchButton.hoverTextShadowBlur
            shadowHorizontalOffset: 0
            shadowVerticalOffset: Theme.launchButton.hoverTextShadowOffset
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.launchable || root.running ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: if (root.running) root.stopRequested(); else if (root.launchable || root.emulatorBusy) root.clicked()
    }

    ToolTip.visible: root.tip !== "" && root.launchable && mouse.containsMouse
    ToolTip.delay: Theme.tooltip.delay
    ToolTip.text: root.tip
}
