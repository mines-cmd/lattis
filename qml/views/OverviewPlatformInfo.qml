import QtQuick

// A platform keeps its color however rows are sorted: its own from platforms.toml, or else its place in the
// library's platform list.
QtObject {
    property var platformList: []

    function colorOf(platform: string): color {
        const own = platformList.find(p => p.id === platform)?.color ?? ""
        if (own !== "")
            return own
        const colors = Theme.overview.platformColors
        const index = Math.max(0, platformList.findIndex(p => p.id === platform))
        return colors.length > 0 ? colors[index % colors.length] : Theme.overview.storageOther
    }

    function nameOf(platform: string): string {
        return platformList.find(p => p.id === platform)?.name ?? platform
    }
}
