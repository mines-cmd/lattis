import QtQuick

// An Image that fades in when an asynchronous load finishes. Only a picture that had to load
// fades: one that's ready at once (cached, or a reused delegate) shows at once, or fast scrolling
// would flicker.
Image {
    id: root

    property bool wasLoading: false
    // Keeps the shown picture while a new sourceSize decodes, so resizing doesn't blank it. A new
    // source still clears it: a reused delegate mustn't show the previous game's picture.
    property bool retainOnResize: false

    asynchronous: true

    // Both handlers run before the image starts its new load. A resize can change width and height
    // one at a time, so a second change arrives while the first is still loading.
    onSourceChanged: retainWhileLoading = false
    onSourceSizeChanged: retainWhileLoading = retainOnResize && (status === Image.Ready || retainWhileLoading)

    onStatusChanged: {
        if (status === Image.Loading && retainWhileLoading) {
            fadeIn.stop()
            opacity = 1
        } else if (status === Image.Loading && Motion.fades) {
            fadeIn.stop()
            opacity = 0
            wasLoading = true
        } else if (status === Image.Ready && wasLoading) {
            wasLoading = false
            fadeIn.restart()
        } else if (status !== Image.Loading) {
            wasLoading = false
            fadeIn.stop()
            opacity = 1
        }
    }

    MotionNumber {
        id: fadeIn
        target: root
        property: "opacity"
        from: 0
        to: 1
        linear: true
    }
}
