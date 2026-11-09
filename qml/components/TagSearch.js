.pragma library

// The tags whose name matches the search typed, in their order. `matches` is Library.matchesSearch
// (passed in, as a library script can't see the singleton); a tag in `keep` is always shown, so a
// choice that is on never hides.
function matching(tags, query, matches, keep) {
    if (query.trim() === "")
        return tags
    return tags.filter(t => keep.includes(t.name) || matches(t.name, query))
}
