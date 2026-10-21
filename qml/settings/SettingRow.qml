import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root

    required property string section
    required property string key
    required property string title
    required property string type
    required property string doc
    required property real min
    required property real max
    required property var choices
    property bool onRelease: false
    required property string defaultText
    required property var value
    required property var raw
    required property string sourceLabel
    required property bool isCustom
    required property string linkTarget
    required property string linkTitle
    required property int followers
    required property bool ownValue
    required property string revertText
    property bool firstInCard: false
    property bool lastInCard: false
    readonly property real cardCornerRadius: Math.max(Theme.settingsWindow.rowRadius,
                                                      Theme.settingsWindow.cardRadius - Theme.settingsWindow.cardPadding)

    signal resetClicked()
    signal followersClicked()

    color: hover.containsMouse ? Theme.controls.hover : Qt.alpha(Theme.controls.hover, 0)

    Behavior on color { enabled: Motion.fades; MotionColor {} }
    radius: Theme.settingsWindow.rowRadius
    // Corners against the card's follow its curve, so the gap around them is even.
    topLeftRadius: firstInCard ? cardCornerRadius : radius
    topRightRadius: topLeftRadius
    bottomLeftRadius: lastInCard ? cardCornerRadius : radius
    bottomRightRadius: bottomLeftRadius
    implicitHeight: content.implicitHeight + Theme.settingsWindow.rowPadding * 2

    HoverHandler { id: hover }

    RowLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: Theme.settingsWindow.rowPadding
        spacing: Theme.settingsWindow.rowLabelSpacing

        // Takes the space the editor doesn't, down to label_min_width; preferredWidth 0 so its
        // unwrapped description doesn't claim space the editor needs.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            // At least as wide as the name, which doesn't wrap. Not the source tag: it appears on the
            // first change, and widening the column then would move the editor while it's in use;
            // it overflows the column instead.
            Layout.minimumWidth: Math.max(Theme.settingsWindow.labelMinWidth, titleLabel.implicitWidth)
            Layout.alignment: Qt.AlignVCenter
            spacing: 2

            Item {
                id: titleRow
                Layout.fillWidth: true
                // The tag's height even while it's hidden, so it appearing doesn't move the row.
                implicitHeight: Math.max(titleLabel.implicitHeight, sourceTag.implicitHeight)

                Label {
                    id: titleLabel
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.title
                    color: Theme.controls.labelColor
                    font.pixelSize: Theme.controls.labelFontSize

                    HoverHandler { id: titleHover }
                    ToolTip.visible: titleHover.hovered
                    ToolTip.delay: Theme.tooltip.delay
                    ToolTip.text: "[%1] %2".arg(root.section).arg(root.key)
                }

                Row {
                    id: tags
                    x: titleLabel.width + Theme.controls.controlSpacing
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.controls.controlSpacing

                    Row {
                        id: extraTags
                        spacing: tags.spacing

                        SettingTag {
                            visible: root.linkTarget !== ""
                            text: "→ " + root.linkTitle
                            tint: Theme.settingsWindow.linkTagColor
                        }

                        SettingTag {
                            visible: root.followers > 0
                            filled: false
                            clickable: true
                            text: qsTr("%1 follow").arg(root.followers)
                            tint: Theme.settingsWindow.followTagColor
                            tip: qsTr("Show the settings that follow this one")
                            onClicked: root.followersClicked()
                        }

                        SettingTag {
                            visible: root.ownValue
                            text: qsTr("own value")
                            tint: Theme.settingsWindow.ownValueTagColor
                            tip: qsTr("Follows a shared value by default, but this theme gives it its own")
                        }
                    }

                    Rectangle {
                        id: sourceTag
                        readonly property color tint: root.isCustom ? Theme.settingsWindow.sourceTagCustom
                                                                    : Theme.settingsWindow.sourceTagInherited

                        // Kept after the label goes back to "default" so the text doesn't change while it fades out.
                        property string shownLabel: root.sourceLabel

                        visible: opacity > 0
                        opacity: root.sourceLabel !== "default" ? 1 : 0
                        implicitWidth: sourceText.implicitWidth + Theme.settingsWindow.sourceTagPadding * 2
                        implicitHeight: sourceText.implicitHeight + 2
                        width: Math.max(0, Math.min(implicitWidth, content.width - tags.x - extraTags.width - (extraTags.width > 0 ? tags.spacing : 0)))
                        radius: Theme.settingsWindow.sourceTagRadius
                        color: Qt.alpha(tint, Theme.settingsWindow.sourceTagFillOpacity)
                        border.color: tint
                        border.width: Theme.settingsWindow.sourceTagBorderWidth

                        Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }
                        Connections {
                            target: root
                            function onSourceLabelChanged() {
                                if (root.sourceLabel !== "default") sourceTag.shownLabel = root.sourceLabel
                            }
                        }

                        Label {
                            id: sourceText
                            anchors.centerIn: parent
                            width: Math.min(implicitWidth, parent.width - Theme.settingsWindow.sourceTagPadding * 2)
                            text: sourceTag.shownLabel
                            elide: Text.ElideRight
                            color: parent.tint
                            font.pixelSize: Theme.settingsWindow.sourceTagFontSize

                            HoverHandler { id: sourceHover }
                            ToolTip.visible: sourceHover.hovered && truncated
                            ToolTip.delay: Theme.tooltip.delay
                            ToolTip.text: root.sourceLabel
                        }
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                text: root.doc
                color: Theme.controls.descriptionColor
                font.pixelSize: Theme.controls.descriptionFontSize
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.settingsWindow.resetButtonSpacing

            Loader {
                id: editorLoader
                Layout.fillWidth: true
                Layout.preferredWidth: Theme.settingsWindow.editorWidth
                Layout.maximumWidth: Theme.settingsWindow.editorWidth
                Layout.minimumWidth: Math.min(Theme.settingsWindow.editorWidth,
                                              item?.minimumWidth ?? item?.implicitWidth ?? 0)
                Layout.alignment: Qt.AlignVCenter
                sourceComponent: {
                    switch (root.type) {
                    case "Color": return colorC
                    case "Int": case "Real": return numberC
                    case "Bool": return boolC
                    case "String": return stringC
                    case "Enum": return enumC
                    case "StringList": return stringListC
                    case "ColorList": return colorListC
                    }
                    return null
                }
                onLoaded: {
                    // Order matters: editors read min/max/choices/type from their value-changed
                    // handler, so those must already be set before value triggers it.
                    item.section = root.section
                    item.key = root.key
                    if ("raw" in item) item.raw = root.raw
                    if ("min" in item) item.min = root.min
                    if ("max" in item) item.max = root.max
                    if ("type" in item) item.type = root.type
                    if ("choices" in item) item.choices = root.choices
                    if ("onRelease" in item) item.onRelease = root.onRelease
                    item.value = root.value
                }
                Connections {
                    target: root
                    function onValueChanged() { if (editorLoader.item) editorLoader.item.value = root.value }
                    function onRawChanged() { if (editorLoader.item && "raw" in editorLoader.item) editorLoader.item.raw = root.raw }
                }
            }

            // Both slots are kept on every row: a button appearing in the middle of a slider drag
            // must not move the editor under the pointer.
            Row {
                spacing: Theme.settingsWindow.resetButtonSpacing

                Item {
                    width: Theme.settingsWindow.smallButtonSize
                    height: Theme.settingsWindow.smallButtonSize
                    opacity: root.revertText !== "" ? 1 : 0

                    Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }

                    IconButton {
                        anchors.fill: parent
                        enabled: root.revertText !== ""
                        plain: true
                        iconSize: Theme.settingsWindow.smallIconSize
                        icon: Icons.history
                        iconColor: Theme.settingsWindow.resetButtonColor
                        tip: qsTr("Revert to %1, the value when Settings opened").arg(root.revertText)
                        onClicked: {
                            root.forceActiveFocus() // the editor shows the new value only when it has let go of focus
                            Theme.revertValue(root.section, root.key)
                        }
                    }
                }

                Item {
                    width: Theme.settingsWindow.smallButtonSize
                    height: Theme.settingsWindow.smallButtonSize
                    opacity: root.isCustom ? 1 : 0

                    Behavior on opacity { enabled: Motion.fades; MotionNumber { fade: true } }

                    IconButton {
                        anchors.fill: parent
                        enabled: root.isCustom
                        plain: true
                        iconSize: Theme.settingsWindow.smallIconSize
                        icon: Icons.close
                        iconColor: Theme.settingsWindow.resetButtonColor
                        tip: qsTr("Reset (remove from %1)").arg(Theme.activeFile(root.section, root.key))
                        onClicked: {
                            root.forceActiveFocus() // the editor shows the new value only when it has let go of focus
                            // First: with "Changed" on, the reset removes this row.
                            root.resetClicked()
                            Theme.resetValue(root.section, root.key)
                        }
                    }
                }
            }
        }
    }

    Component { id: colorC; ColorEditor {} }
    Component { id: numberC; NumberEditor {} }
    Component { id: boolC; BoolEditor {} }
    Component { id: stringC; StringEditor {} }
    Component { id: enumC; EnumEditor {} }
    Component { id: stringListC; StringListEditor {} }
    Component { id: colorListC; ColorListEditor {} }
}
