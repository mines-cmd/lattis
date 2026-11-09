import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// See docs/code/qml.md.
OverviewCard {
    id: root

    property var overview: ({})
    property real regrow: 1
    property bool barsGrown: false
    property int hoveredIndex: -1

    signal yearRequested(int year)

    readonly property var years: overview.years ?? []
    readonly property int most: years.reduce((most, y) => Math.max(most, y.count), 0)
    readonly property var busiestYear: years.reduce((best, y) => best === null || y.count > best.count ? y : best, null)

    onOverviewChanged: if (overview.games === undefined) barsGrown = false

    RowLayout {
        Layout.fillWidth: true

        OverviewCardTitle { text: qsTr("Release years") }
        Item { Layout.fillWidth: true }
        OverviewQuiet {
            text: root.busiestYear ? qsTr("Most in %1 · %n game(s)", "", root.busiestYear.count).arg(root.busiestYear.year) : ""
        }
    }

    Item {
        id: chart
        property real grow: root.barsGrown ? 1 : 0

        Layout.fillWidth: true
        Layout.preferredHeight: Theme.overview.yearsHeight

        Component.onCompleted: {
            grow = 1
            root.barsGrown = true
        }
        Behavior on grow { enabled: Motion.on; MotionNumber { speed: "slow" } }

        Row {
            anchors.fill: parent
            spacing: Theme.overview.yearGap

            Repeater {
                model: root.years

                Item {
                    id: column
                    required property var modelData
                    required property int index
                    readonly property bool hovered: root.hoveredIndex === index
                    readonly property string text: qsTr("%1 · %n game(s)", "", modelData.count).arg(modelData.year)

                    width: Math.max(1, (chart.width - (root.years.length - 1) * Theme.overview.yearGap) / root.years.length)
                    height: chart.height

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: root.most > 0 && column.modelData.count > 0
                            ? Math.max(Theme.overview.barRadius, chart.height * chart.grow * root.regrow * column.modelData.count / root.most) : 0
                        radius: Theme.overview.barRadius
                        color: column.hovered ? Theme.overview.barActive : Theme.overview.barFill
                        opacity: column.hovered ? 1 : Theme.overview.barOpacity

                        ToolTip.visible: column.hovered
                        ToolTip.delay: Theme.tooltip.delay
                        ToolTip.text: column.text
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.yearRequested(column.modelData.year)
                    }

                    HoverHandler {
                        onHoveredChanged: {
                            if (hovered)
                                root.hoveredIndex = column.index
                            else if (root.hoveredIndex === column.index)
                                root.hoveredIndex = -1
                        }
                    }
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.years.length > 0

        OverviewQuiet { text: root.years.length > 0 ? String(root.years[0].year) : "" }
        Item { Layout.fillWidth: true }
        OverviewQuiet { text: root.years.length > 1 ? String(root.years[root.years.length - 1].year) : "" }
    }
}
