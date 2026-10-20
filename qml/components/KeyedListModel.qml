import QtQuick

// See docs/code/qml.md.
ListModel {
    id: root

    property var source: []
    property string leaveSpeed: "fast"
    // What the entries belong to (another game's tags). A `source` of another scope replaces
    // everything, like a new order. Set it before `source`: only a new `source` applies it, since
    // applying it alone would make the old entries again only for the new ones to animate in.
    property var scope
    property var syncedScope

    property bool leaving: false
    // Briefly true after the order changed and every entry was made again, so the delegates can
    // skip their entrance: they are not new.
    property bool rebuilt: false

    dynamicRoles: true

    function indexOfKey(key, from = 0) {
        for (let i = from; i < count; ++i) {
            if (get(i).key === key)
                return i
        }
        return -1
    }

    function hasLeaving() {
        for (let i = 0; i < count; ++i) {
            if (get(i).leaving)
                return true
        }
        return false
    }

    function sync() {
        const items = source ?? []
        const wanted = new Set(items.map(item => item.key))
        const present = new Set()
        const stayingNow = []
        for (let i = 0; i < count; ++i) {
            const key = get(i).key
            present.add(key)
            if (wanted.has(key))
                stayingNow.push(key)
        }
        const stayingNext = items.map(item => item.key).filter(key => present.has(key))
        const rescoped = scope !== syncedScope
        syncedScope = scope
        if (rescoped || stayingNow.length !== stayingNext.length || stayingNow.some((key, i) => key !== stayingNext[i])) {
            clear()
            append(items.map(item => Object.assign({ leaving: false }, item)))
            rebuilt = true
            rebuiltTimer.restart()
            leaving = false
            return
        }

        let marked = false
        for (let i = 0; i < count; ++i) {
            if (!wanted.has(get(i).key) && !get(i).leaving) {
                setProperty(i, "leaving", true)
                marked = true
            }
        }

        let at = 0
        for (const item of items) {
            while (at < count && get(at).leaving && get(at).key !== item.key)
                ++at
            if (at >= count || get(at).key !== item.key)
                insert(at, Object.assign({ leaving: false }, item))
            const entry = get(at)
            for (const name in item) {
                if (entry[name] !== item[name])
                    setProperty(at, name, item[name])
            }
            if (entry.leaving)
                setProperty(at, "leaving", false)
            ++at
        }

        if (marked)
            leaveTimer.restart()
        leaving = hasLeaving()
    }

    onSourceChanged: sync()
    Component.onCompleted: sync()

    readonly property Timer leaveTimer: Timer {
        interval: Motion.duration(Motion.speedMs(root.leaveSpeed))
        onTriggered: {
            for (let i = root.count - 1; i >= 0; --i) {
                if (root.get(i).leaving)
                    root.remove(i)
            }
            root.leaving = root.hasLeaving()
        }
    }

    readonly property Timer rebuiltTimer: Timer {
        interval: Motion.duration(Motion.speedMs("normal")) * 2
        onTriggered: root.rebuilt = false
    }
}
