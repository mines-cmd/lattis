import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: root

    spacing: Theme.onboarding.spacing

    function describe(name, extendsName) {
        if (name === "Mythos")
            return qsTr("Near-black, with one blue accent. Inspired by the Playnite theme by bansakai.")
        if (name === "Ember")
            return qsTr("Dark blue-grey slate, with an ember-orange accent from the logo.")
        if (name === "Moss")
            return qsTr("Dark blue-grey slate, with a moss-green accent.")
        if (name === "Catppuccin")
            return qsTr("Soft pastels on a dark base, from the Catppuccin palette.")
        if (name === "Gruvbox")
            return qsTr("Warm browns with a yellow accent, from the Gruvbox palette.")
        if (name === "Rose Pine")
            return qsTr("Deep indigo with a pink accent, from the Rosé Pine palette.")
        if (name === "Nord")
            return qsTr("Arctic blue-greys with a blue accent, from the Nord palette.")
        if (name === "Steam")
            return qsTr("Dark panels with a green play button, like Steam.")
        return extendsName ? qsTr("Your own, in themes/colors/%1.toml, based on %2.").arg(name).arg(extendsName)
                           : qsTr("Your own, in themes/colors/%1.toml.").arg(name)
    }

    component ThemeCard: AbstractButton {
        id: card

        required property string modelData
        readonly property bool active: modelData === Theme.activeColors
        readonly property var colors: {
            void Theme.availableColors
            void Theme.availableStyles
            return Theme.preview(modelData, Theme.activeStyle)
        }
        readonly property real unit: Theme.onboarding.themePreviewHeight / 170

        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.fillHeight: true
        padding: Theme.onboarding.themeCardPadding
        hoverEnabled: true
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        Accessible.checkable: true
        Accessible.checked: active
        onClicked: Theme.setColors(modelData)

        background: Surface {
            radius: Theme.settingsWindow.cardRadius
            fill: Theme.onboarding.panelBackground
            fillEnd: Theme.onboarding.panelBackgroundEnd
            direction: Theme.onboarding.gradientDirection
            border.width: card.active || card.visualFocus ? 2 : 1
            border.color: card.active || card.visualFocus ? Theme.onboarding.choiceSelectedBorder
                                                         : Theme.onboarding.panelBorder
            Behavior on border.color { enabled: Motion.fades; MotionColor {} }

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                opacity: card.hovered && !card.active ? 1 : 0
                visible: opacity > 0
                color: Theme.palette.hoverOverlay
                Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
            }
        }

        contentItem: ColumnLayout {
            spacing: Theme.onboarding.themeCardPadding

            Surface {
                id: preview
                readonly property var c: card.colors
                readonly property real u: card.unit
                readonly property real large: (c.radius?.large ?? 0) * u
                readonly property real medium: (c.radius?.medium ?? 0) * u
                readonly property real small: (c.radius?.small ?? 0) * u
                readonly property real tiny: (c.radius?.tiny ?? 0) * u

                Layout.fillWidth: true
                implicitHeight: Theme.onboarding.themePreviewHeight
                radius: large
                fill: c.background ?? "transparent"
                fillEnd: c.backgroundEnd ?? c.background ?? "transparent"
                direction: c.gradientDirection ?? "down"
                clip: true

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10 * preview.u
                    spacing: 8 * preview.u

                    Surface {
                        Layout.fillHeight: true
                        implicitWidth: 26 * preview.u
                        radius: preview.medium
                        fill: preview.c.rail ?? "transparent"
                        fillEnd: preview.c.railEnd ?? preview.c.rail ?? "transparent"
                        direction: preview.c.gradientDirection ?? "down"

                        Column {
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: 6 * preview.u
                            spacing: 6 * preview.u

                            Rectangle {
                                width: 14 * preview.u; height: width; radius: preview.tiny
                                color: preview.c.accent ?? "transparent"
                            }
                            Repeater {
                                model: 2
                                Rectangle {
                                    x: 1 * preview.u
                                    width: 12 * preview.u; height: width; radius: preview.tiny * 0.75
                                    color: preview.c.surfaceRaised ?? "transparent"
                                }
                            }
                        }
                    }
                    ColumnLayout {
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: 92 * preview.u
                        spacing: 5 * preview.u

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 14 * preview.u
                            radius: preview.tiny
                            color: preview.c.surface ?? "transparent"
                        }
                        Repeater {
                            model: 4
                            Rectangle {
                                required property int index
                                Layout.fillWidth: true
                                implicitHeight: 12 * preview.u
                                radius: preview.tiny * 0.75
                                color: (index === 0 ? preview.c.selectedOverlay : preview.c.surface) ?? "transparent"
                            }
                        }
                    }
                    ColumnLayout {
                        Layout.alignment: Qt.AlignTop
                        Layout.fillWidth: true
                        spacing: 7 * preview.u

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 64 * preview.u
                            radius: preview.small
                            color: preview.c.surface
                                   ? Qt.tint(preview.c.surface, Qt.alpha(preview.c.accent, Theme.onboarding.themePreviewHeroTint))
                                   : "transparent"
                        }
                        Row {
                            spacing: 5 * preview.u
                            Surface {
                                width: 44 * preview.u; height: 14 * preview.u; radius: preview.tiny
                                fill: preview.c.launch ?? "transparent"
                                fillEnd: preview.c.launchEnd ?? preview.c.launch ?? "transparent"
                                direction: preview.c.gradientDirection ?? "down"
                            }
                            Repeater {
                                model: 2
                                Rectangle {
                                    width: 26 * preview.u; height: 14 * preview.u; radius: preview.tiny
                                    color: preview.c.surfaceRaised ?? "transparent"
                                }
                            }
                        }
                        Repeater {
                            model: [0.9, 0.7]
                            Rectangle {
                                required property real modelData
                                Layout.preferredWidth: parent.width * modelData
                                implicitHeight: 6 * preview.u
                                radius: preview.tiny * 0.75
                                color: preview.c.selectedOverlay ?? "transparent"
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: Theme.controls.checkboxSize
                    implicitHeight: Theme.controls.checkboxSize
                    radius: width / 2
                    color: Theme.controls.inputBackground
                    border.color: card.active ? Theme.controls.checkboxOn : Theme.controls.inputBorder
                    border.width: Math.max(1, Theme.controls.inputBorderWidth)
                    Behavior on border.color { enabled: Motion.fades; MotionColor {} }

                    MarkPop {
                        anchors.fill: parent
                        shown: card.active

                        Rectangle {
                            readonly property int inset: Math.round(parent.width / 4)
                            x: inset
                            y: inset
                            width: parent.width - 2 * inset
                            height: width
                            radius: width / 2
                            color: Theme.controls.checkboxOn
                        }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: card.modelData
                        color: Theme.palette.text
                        font.pixelSize: Theme.onboarding.themeNameSize
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: root.describe(card.modelData, card.colors.extends ?? "")
                        color: Theme.onboarding.descriptionColor
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }

    Flickable {
        id: colorsFlick
        readonly property int rowCount: Math.max(1, Math.ceil(Theme.availableColors.length / grid.columns))
        readonly property real rowHeight: (grid.implicitHeight - (rowCount - 1) * grid.rowSpacing) / rowCount
        readonly property int shownRows: Math.min(rowCount, Theme.onboarding.themeRows)

        Layout.fillWidth: true
        Layout.preferredHeight: shownRows * rowHeight + (shownRows - 1) * grid.rowSpacing
        Layout.fillHeight: true
        // A small window shrinks the list, never the styles below it; a big one leaves it at its rows.
        Layout.minimumHeight: Math.min(rowHeight / 2, Layout.preferredHeight)
        Layout.maximumHeight: Layout.preferredHeight
        clip: true
        contentHeight: grid.implicitHeight
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: SettingsScrollBar { id: colorsBar }

        GridLayout {
            id: grid
            // The scrollbar gets its own gutter, so the cards don't shift when it appears.
            width: colorsFlick.width - (colorsBar.visible ? colorsBar.width + Theme.onboarding.spacing : 0)
            columns: 2
            columnSpacing: Theme.onboarding.spacing
            rowSpacing: Theme.onboarding.spacing

            Repeater {
                model: Theme.availableColors
                ThemeCard {}
            }
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: styleRow.implicitHeight

        RowLayout {
            id: styleRow
            width: parent.width
            anchors.verticalCenter: parent.verticalCenter
            // The column's spacing above the Item makes the space above bigger, so it's shared out.
            anchors.verticalCenterOffset: -root.spacing / 2
            spacing: Theme.onboarding.spacing

            Label {
                text: qsTr("Style")
                color: Theme.palette.text
                font.pixelSize: Theme.onboarding.themeNameSize
                font.bold: true
            }
            Flow {
                Layout.fillWidth: true
                spacing: Theme.layout.spacing

                Repeater {
                    model: Theme.availableStyles
                    AbstractButton {
                        id: styleButton
                        required property string modelData
                        readonly property bool active: modelData === Theme.activeStyle

                        text: modelData
                        padding: Theme.onboarding.themeCardPadding
                        leftPadding: padding * 1.5
                        rightPadding: padding * 1.5
                        hoverEnabled: true
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        Accessible.checkable: true
                        Accessible.checked: active
                        onClicked: Theme.setStyle(modelData)

                        background: Surface {
                            radius: Theme.controls.inputRadius
                            fill: Theme.onboarding.panelBackground
                            fillEnd: Theme.onboarding.panelBackgroundEnd
                            direction: Theme.onboarding.gradientDirection
                            border.width: styleButton.active || styleButton.visualFocus ? 2 : 1
                            border.color: styleButton.active || styleButton.visualFocus
                                          ? Theme.onboarding.choiceSelectedBorder : Theme.onboarding.panelBorder
                            Behavior on border.color { enabled: Motion.fades; MotionColor {} }

                            Rectangle {
                                anchors.fill: parent
                                radius: parent.radius
                                opacity: styleButton.hovered && !styleButton.active ? 1 : 0
                                visible: opacity > 0
                                color: Theme.palette.hoverOverlay
                                Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                            }
                        }
                        contentItem: Label {
                            text: styleButton.text
                            color: Theme.palette.text
                            font.bold: styleButton.active
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }
        }
    }
}
