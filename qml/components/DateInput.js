.pragma library

// Years are always four digits and day and month may be one or two: the locale's short format
// ("dd/MM/yy") would read "25" as 1925 and refuse "5/3".
function dateFormat(shortFormat) {
    return shortFormat.replace(/y+/g, "yyyy").replace(/d+/g, "dd").replace(/M+/g, "MM")
}

function dateParseFormat(shortFormat) {
    return dateFormat(shortFormat).replace(/dd/g, "d").replace(/MM/g, "M")
}

// Without the system locale the app is en_US, whose 12 h clock isn't wanted.
function timeFormat(locale, system) {
    return system ? locale.timeFormat(1) : "HH:mm" // 1 = Locale.ShortFormat, not in scope in a library
}

// A Date at local midnight, or null.
function parseDate(locale, text, shortFormat) {
    const trimmed = text.trim()
    if (trimmed === "")
        return null
    try {
        const date = Date.fromLocaleDateString(locale, trimmed, dateParseFormat(shortFormat))
        if (isNaN(date) || date.getUTCFullYear() < 1900)
            return null
        // The parsed date is midnight UTC, which is the day before in zones west of it.
        return new Date(date.getUTCFullYear(), date.getUTCMonth(), date.getUTCDate())
    } catch (e) {
        return null
    }
}

// {hours, minutes} or null. The locale's own format first, then the other common ways of typing a time.
function parseTime(locale, text, format) {
    const trimmed = text.trim()
    if (trimmed === "")
        return null
    const formats = [format, "H:mm", "h:mm AP", "h:mmAP", "H.mm", "h.mm AP"]
    for (const f of formats) {
        try {
            const time = Date.fromLocaleTimeString(locale, trimmed, f)
            if (!isNaN(time))
                return { hours: time.getHours(), minutes: time.getMinutes() }
        } catch (e) {
        }
    }
    return null
}

function formatDate(locale, date, shortFormat) {
    return date.toLocaleDateString(locale, dateFormat(shortFormat))
}

function formatTime(locale, date, format) {
    return date.toLocaleTimeString(locale, format)
}
