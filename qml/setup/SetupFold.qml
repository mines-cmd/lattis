import QtQuick
import QtQuick.Layouts

Unfold {
    required property real gap

    // Out of the layout when folded; the negative margin hides its spacing until it opens, so
    // the gap doesn't snap in at the start of the unfold.
    visible: open || progress > 0
    spacing: Theme.layout.spacing
    Layout.fillWidth: true
    Layout.bottomMargin: -gap * (1 - progress)
}
