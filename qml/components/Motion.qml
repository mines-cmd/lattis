pragma Singleton
import QtQuick

QtObject {
    // Full motion: false when the style's duration_scale is 0 or the person turned on Reduce motion.
    // Behaviors that move something set `enabled: Motion.on`; springs have no duration to zero, so they need it.
    readonly property bool on: Theme.motion.durationScale > 0 && !General.reduceMotion
    // Reduce motion keeps fades and color changes; only duration_scale 0 turns those off.
    readonly property bool fades: Theme.motion.durationScale > 0
    readonly property bool reduced: General.reduceMotion

    function duration(ms) {
        return on ? Math.round(ms * Theme.motion.durationScale) : 0
    }

    function fadeDuration(ms) {
        return fades ? Math.round(ms * Theme.motion.durationScale) : 0
    }

    function speedMs(speed) {
        return speed === "slow" ? Theme.motion.slow : speed === "normal" ? Theme.motion.normal : Theme.motion.fast
    }

    readonly property string preset: Theme.motion.easing
    readonly property int easingType: preset === "gentle" ? Easing.InOutSine
                                      : preset === "linear" ? Easing.Linear : Easing.BezierSpline
    readonly property var easingCurve: preset === "smooth" ? [0.23, 1, 0.32, 1, 1, 1]
                                       : preset === "snappy" ? [0.3, 0, 0.4, 1, 1, 1] : []
}
