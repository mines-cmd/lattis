.pragma library

// Program arguments as typed in a text field: split on spaces, with "double" or 'single'
// quotes around the parts that have spaces in them (--config="My Config.ini"). A quote
// without its closing one is kept as it is (it's).
function split(text) {
    const out = []
    const arg = /(?:"[^"]*"|'[^']*'|[^\s"']+|["'])+/g
    const quoted = /"([^"]*)"|'([^']*)'/g
    let m
    while ((m = arg.exec(text)) !== null)
        out.push(m[0].replace(quoted, (all, double, single) => double ?? single))
    return out
}

function join(args) {
    return args.map(a => {
        if (a !== "" && !/[\s"']/.test(a))
            return a
        return a.includes('"') ? "'" + a + "'" : '"' + a + '"'
    }).join(" ")
}
