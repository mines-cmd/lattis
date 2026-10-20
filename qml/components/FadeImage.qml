import QtQuick

// An Image that fades in when an asynchronous load finishes. Only a picture that had to load
// fades: one that's ready at once (cached, or a reused delegate) shows at once, or fast scrolling
// would flicker.
Image {
    id: root

    property bool wasLoading: false

    asynchronous: true

    onStatusChanged: {
        if (status === Image.Loading && Motion.fades) {
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
