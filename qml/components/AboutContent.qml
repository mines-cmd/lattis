import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
Item {
    id: root

    property color backgroundColor
    property color labelColor
    property int logoHeight: Theme.about.logoHeight
    property bool showUpdate: true
    property int noticeRightMargin: 0
    // How far the footer's line reaches past the content on each side, to meet the host's edges.
    property int footerInset: 0
    // The host's own padding around the content: taken off the footer's bottom padding, and side by
    // side the gap above the footer, so the credits have the same space above and below.
    property int footerBottomInset: 0
    readonly property bool wide: width >= Theme.about.wideWidth

    implicitHeight: col.implicitHeight

    signal updateRequested()

    readonly property var credits: [
        { label: qsTr("Built with"), links: [["Qt", "https://www.qt.io"],
                                             ["toml++", "https://marzer.github.io/tomlplusplus/"],
                                             ["libarchive", "https://www.libarchive.org"],
                                             ["yaml-cpp", "https://github.com/jbeder/yaml-cpp"]] },
        { label: qsTr("Game info"), links: [["IGDB", "https://www.igdb.com"]] },
        { label: qsTr("Artwork"), links: [["SteamGridDB", "https://www.steamgriddb.com"]] },
        { label: qsTr("Game saves"), links: [["Ludusavi manifest", "https://github.com/mtkennerly/ludusavi-manifest"],
                                             ["PCGamingWiki", "https://www.pcgamingwiki.com"]] },
        { label: qsTr("Icons"), links: [["Tabler Icons", "https://tabler.io/icons"], ["Remix Icon", "https://remixicon.com"]].concat(LicensedIcons.credits) },
    ]

    component Divider: Rectangle {
        implicitHeight: 1
        color: Theme.about.divider
    }

    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 0

        NoticeBar {
            visible: root.showUpdate && AppUpdate.notify
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.layout.toolbarHeight
            Layout.bottomMargin: Theme.about.logoSpacing
            Layout.rightMargin: root.noticeRightMargin
            parts: [{ text: qsTr("Update available") + "  " },
                    { text: Qt.application.version + " → " + AppUpdate.latestVersion, chip: true }]

            SettingsButton {
                Layout.preferredHeight: Theme.notice.buttonHeight
                text: AppUpdate.canReplace ? qsTr("Update") : qsTr("Open release page")
                flat: true
                textColor: Theme.notice.actionText
                font.weight: Theme.font.emphasisWeight
                onClicked: root.updateRequested()
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.wide ? 3 : 1
            columnSpacing: Theme.about.columnSpacing
            rowSpacing: Theme.about.textSpacing

            ColumnLayout {
                Layout.fillWidth: !root.wide
                Layout.preferredWidth: root.wide ? Theme.about.introWidth : -1
                Layout.alignment: root.wide ? Qt.AlignVCenter : Qt.AlignHCenter | Qt.AlignTop
                Layout.leftMargin: root.wide ? Theme.about.introIndent : 0
                spacing: Theme.about.logoSpacing

                Wordmark {
                    Layout.alignment: root.wide ? Qt.AlignLeft : Qt.AlignHCenter
                    logoHeight: root.logoHeight
                    text: Theme.about.logoText
                    background: root.backgroundColor
                }

                Label {
                    Layout.alignment: root.wide ? Qt.AlignLeft : Qt.AlignHCenter
                    Layout.preferredWidth: root.wide ? Theme.about.introWidth
                                                     : Math.min(implicitWidth, Theme.about.descriptionWidth, col.width)
                    horizontalAlignment: root.wide ? Text.AlignLeft : Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: root.labelColor
                    text: qsTr("A game launcher for Linux: emulated games, Steam, Epic and Proton in one library.")
                }
            }

            Divider {
                Layout.fillWidth: !root.wide
                Layout.fillHeight: root.wide
                implicitWidth: 1
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                Layout.leftMargin: root.wide ? Theme.about.wideCreditsIndent : Theme.about.creditsIndent
                columns: 2
                columnSpacing: Theme.about.labelSpacing
                rowSpacing: Theme.about.rowSpacing

                Repeater {
                    model: root.credits

                    delegate: Label {
                        required property var modelData
                        required property int index
                        Layout.row: index
                        Layout.column: 0
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: Theme.about.labelWidth
                        text: modelData.label
                        color: Theme.about.labelColor
                    }
                }
                Repeater {
                    model: root.credits

                    delegate: Flow {
                        id: links
                        required property var modelData
                        required property int index
                        Layout.row: index
                        Layout.column: 1
                        Layout.fillWidth: true

                        Repeater {
                            model: links.modelData.links

                            delegate: Row {
                                id: link
                                required property var modelData
                                required property int index

                                Label {
                                    visible: link.index > 0
                                    text: "·"
                                    leftPadding: Theme.about.separatorSpacing
                                    rightPadding: Theme.about.separatorSpacing
                                    color: Theme.about.separatorColor
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                TextLink {
                                    padding: 0
                                    leftPadding: 0
                                    rightPadding: 0
                                    text: link.modelData[0]
                                    textColor: Theme.about.linkColor
                                    hoverColor: Theme.about.linkHoverColor
                                    underlined: true
                                    underlineColor: Theme.about.linkUnderline
                                    underlineHoverColor: Theme.about.linkHoverUnderline
                                    onClicked: GameRunner.openUrl(link.modelData[1])
                                }
                            }
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            // Side by side, the credits start at the host's padding, so the gap under them matches it.
            Layout.topMargin: root.wide ? root.footerBottomInset : Theme.about.footerSpacing
            implicitHeight: footer.implicitHeight + Theme.about.footerPadding
                            + Math.max(0, Theme.about.footerPadding - root.footerBottomInset)

            Divider {
                x: -root.footerInset
                width: parent.width + 2 * root.footerInset
            }

            RowLayout {
                id: footer
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: Theme.about.footerPadding
                spacing: Theme.layout.spacing

                TextLink {
                    padding: 0
                    leftPadding: 0
                    rightPadding: 0
                    font.pixelSize: Theme.about.footerFontSize
                    textFormat: Text.StyledText
                    text: qsTr("Version %1").arg("<font face=\"" + Theme.font.monoFamily + "\">"
                                                 + Qt.application.version + "</font>")
                    textColor: Theme.about.versionColor
                    hoverColor: Theme.about.versionHoverColor
                    underlined: true
                    underlineHoverColor: Theme.about.linkHoverUnderline
                    trailingIcon: Icons.openExternal
                    ToolTip.visible: hovered
                    ToolTip.delay: Theme.tooltip.delay
                    ToolTip.text: qsTr("Release notes")
                    onClicked: GameRunner.openUrl(AppUpdate.installedReleasePage)
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                    font.pixelSize: Theme.about.footerFontSize
                    color: Theme.about.footerColor
                    text: qsTr("Game covers © their respective owners.")
                }
            }
        }
    }
}
