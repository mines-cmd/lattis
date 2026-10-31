import QtQuick

// See docs/code/qml.md.
IconWall {
    moving: Theme.onboarding.backdropMotion
    tint: Theme.onboarding.background

    readonly property var coverIds: [
        271590, 1245620, 1145360, 367520, 504230, 292030, 620, 220, 1091500, 1174180, 413150, 105600,
        374320, 814380, 782330, 1086940, 632470, 1145350, 753640, 1092790, 588650, 268910, 1057090,
        264710, 1850570, 1151640, 1593500, 1817070, 1687950, 582010, 2050650, 524220, 870780, 1649240,
        1328670, 976730, 584400, 250760, 1627720, 1888160, 1244090, 553420, 2138710, 1817230
    ]

    sources: coverIds.map(id => "qrc:/qt/qml/Launcher/resources/covers/" + id + ".webp")
}
