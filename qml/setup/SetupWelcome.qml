import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Window

// See docs/code/qml.md.
Item {
    id: root

    readonly property string title: ""
    property bool restore: false
    property real renderScale: 1
    readonly property real layerScale: renderScale * Screen.devicePixelRatio
    readonly property string nextText: restore ? qsTr("Choose a backup") : qsTr("Next")

    function next() {
        if (!restore)
            return false
        importDialog.active = true
        importDialog.item.pick()
        return true
    }

    implicitHeight: column.implicitHeight + Theme.onboarding.spacing

    Loader {
        id: importDialog
        active: false
        sourceComponent: ImportDialog {}
    }

    component Choice: AbstractButton {
        id: choice
        property string iconPath
        property string description

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: 1
        implicitHeight: choiceColumn.implicitHeight + topPadding + bottomPadding
        padding: Theme.onboarding.choicePadding
        checkable: true
        autoExclusive: true
        hoverEnabled: true
        HoverHandler { cursorShape: Qt.PointingHandCursor }

        background: Surface {
            radius: Theme.settingsWindow.cardRadius
            fill: Theme.onboarding.panelBackground
            fillEnd: Theme.onboarding.panelBackgroundEnd
            direction: Theme.onboarding.gradientDirection
            border.width: choice.checked || choice.visualFocus ? 2 : 1
            border.color: choice.checked || choice.visualFocus ? Theme.onboarding.choiceSelectedBorder
                                                              : Theme.onboarding.panelBorder

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                visible: choice.hovered && !choice.checked
                color: Theme.palette.hoverOverlay
            }
        }

        contentItem: ColumnLayout {
            id: choiceColumn
            spacing: 10

            Rectangle {
                implicitWidth: Theme.onboarding.choiceIconSize
                implicitHeight: Theme.onboarding.choiceIconSize
                radius: Theme.settingsWindow.cardRadius
                color: choice.checked ? Qt.alpha(Theme.onboarding.choiceIcon, Theme.onboarding.choiceIconOpacity)
                                      : Theme.palette.hoverOverlay

                Icon {
                    anchors.centerIn: parent
                    path: choice.iconPath
                    size: Math.round(Theme.onboarding.choiceIconSize / 2)
                    color: choice.checked ? Theme.onboarding.choiceIcon : Theme.palette.text
                }
            }
            Label {
                Layout.fillWidth: true
                text: choice.text
                color: Theme.palette.text
                font.pixelSize: Theme.onboarding.choiceTitleSize
                font.weight: Theme.font.strongWeight
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                text: choice.description
                textFormat: Text.StyledText
                color: Theme.onboarding.descriptionColor
                lineHeight: 1.2
                wrapMode: Text.Wrap
            }
            Item { Layout.fillHeight: true }
        }
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.bottomMargin: Theme.onboarding.spacing
        spacing: 0

        Item {
            id: panel
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: hero.implicitHeight + Theme.onboarding.welcomeLogoTop + Theme.onboarding.spacing * 2

            Item {
                id: art
                anchors.fill: parent
                layer.enabled: true
                layer.effect: MultiEffect {
                    maskEnabled: true
                    maskSource: artMask
                    maskThresholdMin: 0.5
                    maskSpreadAtMin: 1.0
                }

                SetupCoverWall {
                    anchors.fill: parent
                    // Only the guide's backdrop moves; two moving walls compete.
                    moving: false
                    tint: Theme.onboarding.welcomeArtTint
                    dim: Theme.onboarding.welcomeArtDim
                    vignette: Theme.onboarding.welcomeArtVignette
                }
            }

            // Outside the masked layer: a layer under the card's scale is drawn at the unscaled size
            // and stretched, which blurred the logo, the text and the screenshot on large screens.
            // Nothing here reaches the rounded corners, so a plain clip does.
            Item {
                anchors.fill: parent
                clip: true

                ColumnLayout {
                    id: hero
                    anchors.top: parent.top
                    anchors.topMargin: Theme.onboarding.welcomeLogoTop
                    width: parent.width
                    spacing: Theme.onboarding.welcomeTaglineSpacing

                    Item {
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: wordmark.implicitWidth
                        implicitHeight: wordmark.implicitHeight

                        MultiEffect {
                            anchors.fill: wordmark
                            visible: Theme.onboarding.logoShadowSize > 0
                            source: wordmark
                            shadowEnabled: true
                            shadowColor: Theme.onboarding.logoShadowColor
                            blurMax: Theme.onboarding.logoShadowSize
                            shadowBlur: 1
                            shadowVerticalOffset: Math.round(Theme.onboarding.logoShadowSize / 4)
                            autoPaddingEnabled: true
                        }

                        Wordmark {
                            id: wordmark
                            logoHeight: Theme.onboarding.logoHeight
                            background: Theme.onboarding.welcomeArtTint
                            pixelScale: root.layerScale
                            Accessible.role: Accessible.Heading
                            Accessible.name: qsTr("Welcome to %1").arg(Qt.application.displayName)
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: Math.min(implicitWidth, hero.width * Theme.onboarding.welcomeTaglineWidth)
                        text: qsTr("Your Steam games, emulated games and Linux games in one place.")
                        horizontalAlignment: Text.AlignHCenter
                        color: Theme.onboarding.welcomeTaglineColor
                        font.pixelSize: Theme.onboarding.welcomeTaglineSize
                        lineHeight: 1.2
                        wrapMode: Text.Wrap
                    }
                }

                ShaderEffect {
                    id: shot
                    // Sized and placed on whole device pixels, so the image is drawn 1:1 instead of
                    // resampled: under the card's 1.1 scale on a 1440p screen it was blurry.
                    readonly property real baseX: (parent.width - width) / 2
                    readonly property real baseY: hero.y + hero.height + Theme.onboarding.welcomeTaglineSpacing
                    property point snap: Qt.point(0, 0)
                    function align() {
                        const s = root.layerScale
                        const p = parent.mapToItem(null, baseX, baseY)
                        const d = Screen.devicePixelRatio
                        const next = Qt.point((Math.round(p.x * d) - p.x * d) / s, (Math.round(p.y * d) - p.y * d) / s)
                        if (Math.abs(next.x - snap.x) > 0.001 || Math.abs(next.y - snap.y) > 0.001)
                            snap = next
                    }
                    x: baseX + snap.x
                    y: baseY + snap.y
                    width: Math.round(parent.width * Theme.onboarding.welcomeShotWidth * root.layerScale) / root.layerScale
                    height: Math.round(width * root.layerScale * shotImage.implicitHeight
                                       / Math.max(1, shotImage.implicitWidth)) / root.layerScale
                    // The card's scale and position settle after the guide's opening animation, and
                    // nothing signals a change of scene position.
                    Connections {
                        target: shot.Window.window
                        function onAfterAnimating() { shot.align() }
                    }
                    visible: shotImage.status === Image.Ready
                    property variant source: shotImage
                    property size size: Qt.size(width, height)
                    property real radius: Theme.onboarding.welcomeShotRadius
                    fragmentShader: "qrc:/qt/qml/Launcher/qml/shaders/coverround.frag.qsb"

                    Image {
                        id: shotImage
                        visible: false
                        anchors.fill: parent
                        asynchronous: true
                        sourceSize.width: Math.round(shot.width * root.layerScale)
                        source: "qrc:/qt/qml/Launcher/resources/setup/welcome.webp"
                    }
                }

                Rectangle {
                    readonly property real shown: Math.max(0, parent.height - shot.y)
                    x: shot.x
                    width: shot.width
                    height: Math.round(shown * Theme.onboarding.welcomeShotFade)
                    y: parent.height - height
                    visible: shot.visible && height > 0
                    gradient: Gradient {
                        GradientStop { position: 0; color: Qt.alpha(Theme.onboarding.welcomeArtTint, 0) }
                        GradientStop { position: 1; color: Theme.onboarding.welcomeArtTint }
                    }
                }
            }

            Rectangle {
                id: artMask
                anchors.fill: parent
                radius: Theme.onboarding.welcomeArtRadius
                visible: false
                layer.enabled: true
            }

            Rectangle {
                anchors.fill: parent
                radius: Theme.onboarding.welcomeArtRadius
                color: "transparent"
                border.width: 1
                border.color: Theme.onboarding.panelBorder
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.topMargin: Theme.onboarding.spacing
            text: qsTr("How do you want to start?")
            color: Theme.onboarding.titleColor
            font.pixelSize: Theme.onboarding.welcomeHeadingSize
            font.weight: Theme.font.strongWeight
            wrapMode: Text.Wrap
        }

        Label {
            Layout.fillWidth: true
            Layout.bottomMargin: Theme.onboarding.spacing
            text: qsTr("Setting it up takes a couple of minutes, and everything here can be changed later in Settings.")
            color: Theme.onboarding.descriptionColor
            lineHeight: 1.2
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            // The choices fill the row's height to match each other; the row would inherit that
            // and split the spare height with the spacers.
            Layout.fillHeight: false
            spacing: Theme.onboarding.spacing

            Choice {
                checked: !root.restore
                onToggled: root.restore = !checked
                iconPath: Icons.add
                text: qsTr("Set up a new library")
                description: qsTr("Find Steam and your emulators, add your ROM folders and API keys, and pick a theme.")
            }
            Choice {
                checked: root.restore
                onToggled: root.restore = checked
                iconPath: Icons.upload
                text: qsTr("Restore from a backup")
                description: qsTr("Import a library backup (<tt>.tar.zst</tt>) made with “Back up library”. "
                                  + "The app restarts into it.")
            }
        }
    }
}
