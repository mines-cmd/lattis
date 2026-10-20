import QtQuick
import QtQuick.Effects

Rectangle {
    id: thumb
    property string source
    property real aspect: 2 / 3
    property real cornerRadius: Theme.radius.tiny
    property int fillMode: Image.PreserveAspectCrop
    implicitHeight: Theme.gameEditor.bulkThumbnailHeight
    implicitWidth: implicitHeight * aspect
    radius: cornerRadius
    color: Theme.gameEditor.artPreviewBackground
    Rectangle {
        id: thumbMask
        anchors.fill: parent
        radius: parent.radius
        visible: false
        layer.enabled: true
    }
    FadeImage {
        id: thumbImage
        anchors.fill: parent
        // Masked only once loaded: a layer made before the picture arrived could stay blank until the row was redrawn.
        layer.enabled: status === Image.Ready
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: thumbMask
            // Without a threshold every half-covered edge pixel counts as inside, which eats the curve of the corner.
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }
        source: parent.source
        // In steps, so a few pixels of layout change (a scrollbar appearing) don't reload the picture and make it flash.
        sourceSize.height: Math.ceil(parent.height * 2 / 128) * 128
        fillMode: parent.fillMode
    }
    Icon {
        anchors.centerIn: parent
        visible: thumb.source !== "" && thumbImage.status === Image.Error
        path: Icons.image
        size: Math.min(Theme.settingsWindow.iconSize * 1.5, Math.min(thumb.width, thumb.height) / 2)
        color: Theme.gameEditor.hintColor
        opacity: 0.5
    }
    Icon {
        id: spinner
        readonly property bool waiting: thumb.source !== "" && thumbImage.status === Image.Loading
        anchors.centerIn: parent
        visible: waiting
        path: Icons.loader
        size: Math.min(Theme.settingsWindow.iconSize * 1.5, Math.min(thumb.width, thumb.height) / 2)
        color: Theme.gameEditor.hintColor
        opacity: 0.5
        RotationAnimation on rotation {
            running: spinner.waiting
            from: 0
            to: 360
            duration: Theme.iconButton.spinDuration
            loops: Animation.Infinite
        }
    }
}
