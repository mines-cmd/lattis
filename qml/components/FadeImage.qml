import QtQuick

// An Image that fades in when an asynchronous load finishes. Only a picture that had to load
// fades: one that's ready at once (cached, or a reused delegate) shows at once, or fast scrolling
// would flicker.
Image {
    id: root

    property bool wasLoading: false
    // Keeps the shown picture while a new sourceSize decodes, so resizing doesn't blank it. A new
    // source still clears it: a reused delegate mustn't show the previous game's picture. It works
    // through Image's own retainWhileLoading, which must not be redeclared here or it stops working.
    property bool retainOnResize: false
    // Off for a picture that loads out of sight, under something that will reveal it whole.
    property bool fades: true
    // Loading or still fading in: whatever stands in for the picture must stay until this is false,
    // or the moment between the two is a blank.
    readonly property bool arriving: status === Image.Loading || fadeIn.running

    asynchronous: true

    // Both handlers run before the image starts its new load. A resize can change width and height
    // one at a time, so a second change arrives while the first is still loading.
    onSourceChanged: retainWhileLoading = false
    onSourceSizeChanged: retainWhileLoading = retainOnResize && (status === Image.Ready || retainWhileLoading)

    onStatusChanged: {
        if (status === Image.Loading && retainWhileLoading) {
            fadeIn.stop()
            opacity = 1
        } else if (status === Image.Loading && Motion.fades && fades) {
            fadeIn.stop()
            opacity = 0
            wasLoading = true
        } else if (status === Image.Ready && wasLoading && fades) {
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
