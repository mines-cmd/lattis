import QtQuick

SpringAnimation {
    spring: Theme.motion.spring
    damping: Theme.motion.damping
    // The spring stops once it is within epsilon of the target, so a huge one lands at once.
    epsilon: Motion.on ? 0.25 : 1e9
}
