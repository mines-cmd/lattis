import QtQuick
import QtQuick.Shapes

Rectangle {
    id: thumb
    property string source
    property real aspect: 2 / 3
    property real cornerRadius: Theme.radius.tiny
    property int fillMode: Image.PreserveAspectCrop
    property string failedIcon: Icons.image
    property bool iconWhenEmpty: false
    readonly property alias status: thumbImage.status
    implicitHeight: Theme.gameEditor.bulkThumbnailHeight
    implicitWidth: implicitHeight * aspect
    radius: cornerRadius
    color: Theme.gameEditor.artPreviewBackground
    // Drawn by a Shape filled with the picture rather than masked through a layer: a layer could be drawn once
    // before the picture's texture existed and then stay blank, since nothing marked it for a redraw.
    FadeImage {
        id: thumbImage
        visible: false
        source: thumb.source
        // In steps, so a few pixels of layout change (a scrollbar appearing) don't reload the picture and make it flash.
        sourceSize.height: Math.ceil(thumb.height * 2 / 128) * 128
        readonly property bool loaded: status === Image.Ready && implicitWidth > 0 && implicitHeight > 0
        readonly property real fitScale: !loaded ? 1
            : thumb.fillMode === Image.PreserveAspectFit ? Math.min(thumb.width / implicitWidth, thumb.height / implicitHeight)
            : Math.max(thumb.width / implicitWidth, thumb.height / implicitHeight)
        readonly property real shownWidth: implicitWidth * fitScale
        readonly property real shownHeight: implicitHeight * fitScale
    }
    Shape {
        id: picture
        readonly property real insetX: Math.max(0, (thumb.width - thumbImage.shownWidth) / 2)
        readonly property real insetY: Math.max(0, (thumb.height - thumbImage.shownHeight) / 2)
        anchors.fill: parent
        visible: thumbImage.loaded
        opacity: thumbImage.opacity
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: -1
            fillItem: thumbImage
            // The fill is the picture's texture at its own pixel size, so it's scaled and centered here.
            fillTransform: PlanarTransform.fromAffineMatrix(thumbImage.fitScale, 0, 0, thumbImage.fitScale,
                                                            (thumb.width - thumbImage.shownWidth) / 2,
                                                            (thumb.height - thumbImage.shownHeight) / 2)
            PathRectangle {
                x: picture.insetX
                y: picture.insetY
                width: thumb.width - 2 * picture.insetX
                height: thumb.height - 2 * picture.insetY
                // A fitted picture that doesn't reach the corners keeps its own square corners, as under the old mask.
                radius: picture.insetX === 0 && picture.insetY === 0 ? thumb.radius : 0
            }
        }
    }
    Icon {
        anchors.centerIn: parent
        visible: thumb.source !== "" ? thumbImage.status === Image.Error : thumb.iconWhenEmpty
        path: thumb.failedIcon
        size: Math.min(Theme.controls.iconSize * 1.5, Math.min(thumb.width, thumb.height) / 2)
        color: Theme.gameEditor.hintColor
        opacity: Theme.gameEditor.placeholderIconOpacity
    }
    Icon {
        id: spinner
        readonly property bool waiting: thumb.source !== "" && thumbImage.status === Image.Loading
        anchors.centerIn: parent
        visible: waiting
        path: Icons.loader
        size: Math.min(Theme.controls.iconSize * 1.5, Math.min(thumb.width, thumb.height) / 2)
        color: Theme.gameEditor.hintColor
        opacity: Theme.gameEditor.placeholderIconOpacity
        RotationAnimation on rotation {
            running: spinner.waiting
            from: 0
            to: 360
            duration: Theme.iconButton.spinDuration
            loops: Animation.Infinite
        }
    }
}
