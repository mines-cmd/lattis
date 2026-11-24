#include "tomlpatch.h"
#include "tomlstring.h"

#include <QFile>
#include <QSaveFile>

#include <toml++/toml.hpp>

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

namespace TomlPatch {
namespace {
// `line`/`column` are 1-indexed, `column` counts Unicode scalars (toml++'s convention), so
// we walk `text` decoding UTF-8 rather than indexing bytes directly.
size_t byteOffset(const std::string &text, size_t line, size_t column)
{
    size_t pos = 0;
    size_t curLine = 1;
    while (curLine < line) {
        const size_t nl = text.find('\n', pos);
        if (nl == std::string::npos)
            return text.size();
        pos = nl + 1;
        curLine++;
    }
    size_t col = 1;
    while (col < column && pos < text.size()) {
        const auto c = static_cast<unsigned char>(text[pos]);
        size_t len = 1;
        if ((c & 0xE0) == 0xC0)
            len = 2;
        else if ((c & 0xF0) == 0xE0)
            len = 3;
        else if ((c & 0xF8) == 0xF0)
            len = 4;
        pos += len;
        col++;
    }
    return pos;
}

size_t lineStart(const std::string &text, size_t line)
{
    return byteOffset(text, line, 1);
}

// Start of the line after `line`, i.e. one past its newline; text.size() on the last line.
size_t lineEndInclusive(const std::string &text, size_t line)
{
    const size_t start = lineStart(text, line);
    const size_t nl = text.find('\n', start);
    return nl == std::string::npos ? text.size() : nl + 1;
}

// The file's lines, 1-indexed; line i spans [start(i), start(i + 1)), its newline included.
class Lines
{
public:
    explicit Lines(const std::string &text)
        : m_text(text)
    {
        size_t i = 0;
        while (i < text.size()) {
            m_starts.push_back(i);
            const size_t nl = text.find('\n', i);
            if (nl == std::string::npos)
                break;
            i = nl + 1;
        }
    }

    size_t count() const { return m_starts.size(); }
    size_t start(size_t line) const { return line <= count() ? m_starts[line - 1] : m_text.size(); }

    std::string_view trimmed(size_t line) const
    {
        std::string_view s(m_text);
        s = s.substr(start(line), start(line + 1) - start(line));
        const size_t b = s.find_first_not_of(" \t\r\n");
        if (b == std::string_view::npos)
            return {};
        return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
    }

    bool isBlank(size_t line) const { return trimmed(line).empty(); }
    bool isComment(size_t line) const { return trimmed(line).starts_with('#'); }
    bool isHeader(size_t line) const { return trimmed(line).starts_with('['); }

    // The block's lines [first, last], newline included even on a last line without one.
    std::string text(size_t first, size_t last) const
    {
        std::string out = m_text.substr(start(first), start(last + 1) - start(first));
        if (!out.empty() && out.back() != '\n')
            out += '\n';
        return out;
    }

private:
    const std::string &m_text;
    std::vector<size_t> m_starts;
};

const toml::table *findSection(const toml::table &tbl, const std::string &section)
{
    const toml::node *node = tbl.get(section);
    return node ? node->as_table() : nullptr;
}

Result fail(const std::string &what)
{
    return {false, {}, QString::fromStdString(what)};
}

// Parses `result.text` and compares it with the table the edit meant to produce, so a wrong
// text never reaches a caller as `ok`.
Result verified(Result result, const toml::table &expected)
{
    if (!result.ok)
        return result;
    try {
        if (toml::parse(result.text.toStdString()) != expected)
            return fail("the edit would not produce the intended file");
    } catch (const toml::parse_error &e) {
        return fail("the edit would produce a file that does not parse: "
                    + std::string(e.description()));
    }
    return result;
}

// The value `formatted` as a node to put into an expected table; false when it isn't valid TOML.
bool parseValue(const std::string &formatted, toml::table *holder)
{
    try {
        *holder = toml::parse("v = " + formatted);
    } catch (const toml::parse_error &) {
        return false;
    }
    return holder->get("v") != nullptr;
}

toml::table *tableAt(toml::table &tbl, const std::string &name)
{
    toml::node *node = tbl.get(name);
    return node ? node->as_table() : nullptr;
}

// A key/value written at the top of the file, as opposed to a [section] or [[array]] that
// toml++ also stores in the root table.
bool isRootKeyValue(const toml::node &node)
{
    if (const toml::table *t = node.as_table())
        return t->is_inline();
    return !node.is_array_of_tables();
}

Result setRootValue(const std::string &src, const toml::table &tbl, const std::string &keyName,
                    const std::string &value)
{
    std::string out = src;
    if (const toml::node *existing = tbl.get(keyName)) {
        if (!isRootKeyValue(*existing))
            return fail("'" + keyName + "' is a section, not a value");
        const toml::source_region &region = existing->source();
        const size_t begin = byteOffset(src, region.begin.line, region.begin.column);
        const size_t end = byteOffset(src, region.end.line, region.end.column);
        out.replace(begin, end - begin, value);
        return {true, QString::fromStdString(out), {}};
    }

    // After the last top-level key; never after a [section] header, where the key would
    // land inside that section.
    size_t insertAfterLine = 0;
    for (auto &&[k, v] : tbl) {
        if (isRootKeyValue(v))
            insertAfterLine = std::max(insertAfterLine, size_t(v.source().end.line));
    }
    size_t at = 0;
    if (insertAfterLine > 0) {
        at = lineEndInclusive(src, insertAfterLine);
    } else {
        while (at < src.size() && src[at] == '#') {
            const size_t nl = src.find('\n', at);
            at = nl == std::string::npos ? src.size() : nl + 1;
        }
    }
    std::string line = keyName + " = " + value + "\n";
    if (at == src.size() && !src.empty() && src.back() != '\n')
        line.insert(0, 1, '\n');
    out.insert(at, line);
    return {true, QString::fromStdString(out), {}};
}

std::optional<toml::table> parseText(const std::string &src, Result *failure)
{
    try {
        return toml::parse(src);
    } catch (const toml::parse_error &e) {
        *failure = fail(std::string(e.description()));
        return std::nullopt;
    }
}

bool isHeaderTable(const toml::node &node)
{
    const toml::table *t = node.as_table();
    return t && !t->is_inline();
}

// The lines of every [table] and [[array]] header, sorted. Implicit tables (the `a` of
// `[a.b]`) report their child's line, and dotted keys (`a.b = 1`) their key's line, so
// only lines that start with '[' count.
void collectHeaders(const toml::table &tbl, const Lines &lines, std::vector<size_t> &out)
{
    for (auto &&[k, v] : tbl) {
        if (isHeaderTable(v)) {
            out.push_back(v.source().begin.line);
            collectHeaders(*v.as_table(), lines, out);
        } else if (const toml::array *a = v.as_array()) {
            for (auto &&el : *a) {
                if (isHeaderTable(el)) {
                    out.push_back(el.source().begin.line);
                    collectHeaders(*el.as_table(), lines, out);
                }
            }
        }
    }
}

std::vector<size_t> headerLines(const toml::table &tbl, const Lines &lines)
{
    std::vector<size_t> out;
    collectHeaders(tbl, lines, out);
    std::erase_if(out, [&](size_t line) { return !lines.isHeader(line); });
    std::sort(out.begin(), out.end());
    return out;
}

const toml::table *findBlock(const toml::table &tbl, const QString &array, const QString &key,
                             const QString &value)
{
    const toml::node *node = tbl.get(array.toStdString());
    const toml::array *arr = node ? node->as_array() : nullptr;
    if (!arr)
        return nullptr;
    const std::string keyName = key.toStdString();
    const std::string wanted = value.toStdString();
    for (auto &&el : *arr) {
        if (!isHeaderTable(el))
            continue;
        const auto *v = el.as_table()->get_as<std::string>(keyName);
        if (v && v->get() == wanted)
            return el.as_table();
    }
    return nullptr;
}

// A [table] or [[array]] written with a header line, as opposed to a value, an inline table or
// the table a dotted key makes.
bool isHeaderNode(const toml::node &node, const Lines &lines)
{
    if (const toml::table *t = node.as_table())
        return !t->is_inline() && lines.isHeader(t->source().begin.line);
    if (const toml::array *a = node.as_array())
        return !a->empty() && isHeaderNode(a->front(), lines);
    return false;
}

// The last line of `node` and of everything below it, sub-tables included.
size_t contentEnd(const toml::node &node, const Lines &lines)
{
    if (const toml::table *t = node.as_table(); t && !t->is_inline()) {
        size_t last = t->source().begin.line;
        for (auto &&[k, v] : *t)
            last = std::max(last, contentEnd(v, lines));
        return last;
    }
    if (const toml::array *a = node.as_array(); a && isHeaderNode(*a, lines)) {
        size_t last = 0;
        for (auto &&el : *a)
            last = std::max(last, contentEnd(el, lines));
        return last;
    }
    return node.source().end.line;
}

// The last line of the table's own keys: where a new key goes. A sub-table's lines don't count.
size_t lastKeyLine(const toml::table &table, const Lines &lines)
{
    size_t last = table.source().begin.line;
    for (auto &&[k, v] : table) {
        if (!isHeaderNode(v, lines))
            last = std::max(last, contentEnd(v, lines));
    }
    return last;
}

struct Span
{
    size_t first = 0; // the first comment line above the header, or the header
    size_t last = 0;  // the last line of the block or its last sub-table, or the comment lines below
};

// The block runs to the next header that isn't one of its own sub-tables.
Span spanOf(const toml::table &block, const Lines &lines, const std::vector<size_t> &headers)
{
    const size_t header = block.source().begin.line;
    Span span{header, contentEnd(block, lines)};
    while (span.first > 1 && lines.isComment(span.first - 1))
        span.first--;

    std::vector<size_t> own;
    collectHeaders(block, lines, own);
    size_t nextHeader = lines.count() + 1;
    for (auto it = std::upper_bound(headers.begin(), headers.end(), header); it != headers.end();
         ++it) {
        if (std::find(own.begin(), own.end(), *it) == own.end()) {
            nextHeader = *it;
            break;
        }
    }
    size_t j = span.last + 1;
    while (j < nextHeader && lines.isComment(j))
        j++;
    if (j != nextHeader || nextHeader > lines.count())
        span.last = j - 1;
    return span;
}

std::pair<size_t, size_t> removalRange(const Span &span, const Lines &lines)
{
    size_t after = span.last + 1;
    while (after <= lines.count() && lines.isBlank(after))
        after++;
    size_t before = span.first;
    if (after > lines.count()) {
        while (before > 1 && lines.isBlank(before - 1))
            before--;
    }
    return {lines.start(before), lines.start(after)};
}

const toml::table *lastBlock(const toml::table &tbl, const QString &array)
{
    const toml::node *node = tbl.get(array.toStdString());
    const toml::array *arr = node ? node->as_array() : nullptr;
    const toml::table *last = nullptr;
    if (arr) {
        for (auto &&el : *arr) {
            if (isHeaderTable(el))
                last = el.as_table();
        }
    }
    return last;
}

size_t indexIn(const toml::table &tbl, const QString &array, const toml::table *block)
{
    const toml::array *arr = tbl.get_as<toml::array>(array.toStdString());
    for (size_t i = 0; arr && i < arr->size(); ++i) {
        if (arr->get(i) == block)
            return i;
    }
    return 0;
}

toml::array *arrayAt(toml::table &tbl, const QString &array)
{
    return tbl.get_as<toml::array>(array.toStdString());
}

toml::table &blockAt(toml::table &tbl, const QString &array, size_t index)
{
    return *arrayAt(tbl, array)->get(index)->as_table();
}

std::string insertBlockText(std::string text, size_t at, const std::string &block)
{
    std::string after;
    if (at == std::string::npos) {
        while (text.ends_with("\n\n"))
            text.pop_back();
    } else {
        after = text.substr(at);
        text.resize(at);
    }
    if (!text.empty() && text.back() != '\n')
        text += '\n';
    if (!text.empty())
        text += '\n';
    return text + block + after;
}
}


Result setValue(const QString &text, const QString &section, const QString &key,
                 const QString &formattedValue)
{
    const std::string src = text.toStdString();
    const std::string sectionName = section.toStdString();
    const std::string keyName = key.toStdString();
    const std::string value = formattedValue.toStdString();

    toml::table tbl;
    try {
        tbl = toml::parse(src);
    } catch (const toml::parse_error &e) {
        return fail(std::string(e.description()));
    }

    toml::table holder;
    if (!parseValue(value, &holder))
        return fail("'" + value + "' is not a TOML value");
    toml::table expected = tbl;
    toml::table *expectedSection = &expected;
    if (!sectionName.empty()) {
        toml::node *node = expected.get(sectionName);
        if (!node) {
            expected.insert(sectionName, toml::table{});
            node = expected.get(sectionName);
        }
        if (!(expectedSection = node->as_table()))
            return fail("'" + sectionName + "' is a value, not a section");
    }
    expectedSection->insert_or_assign(keyName, *holder.get("v"));

    if (sectionName.empty())
        return verified(setRootValue(src, tbl, keyName, value), expected);

    const Lines lines(src);
    if (const toml::table *sectionTbl = findSection(tbl, sectionName)) {
        std::string out = src;
        if (const toml::node *existing = sectionTbl->get(keyName)) {
            const toml::source_region &region = existing->source();
            const size_t begin = byteOffset(src, region.begin.line, region.begin.column);
            const size_t end = byteOffset(src, region.end.line, region.end.column);
            out.replace(begin, end - begin, value);
        } else {
            out.insert(lineEndInclusive(src, lastKeyLine(*sectionTbl, lines)),
                       keyName + " = " + value + "\n");
        }
        const Result result = verified({true, QString::fromStdString(out), {}}, expected);
        if (result.ok || sectionTbl->get(keyName))
            return result;
        // A section only made by its sub-tables ([a.b] alone) has no header of its own to
        // insert under; write one.
    }

    std::string out = src;
    if (!out.empty() && out.back() != '\n')
        out += '\n';
    out += "\n[" + sectionName + "]\n" + keyName + " = " + value + "\n";
    return verified({true, QString::fromStdString(out), {}}, expected);
}

Result removeValue(const QString &text, const QString &section, const QString &key)
{
    const std::string src = text.toStdString();
    const std::string sectionName = section.toStdString();
    const std::string keyName = key.toStdString();

    toml::table tbl;
    try {
        tbl = toml::parse(src);
    } catch (const toml::parse_error &e) {
        return fail(std::string(e.description()));
    }

    const toml::table *sectionTbl = sectionName.empty() ? &tbl : findSection(tbl, sectionName);
    const toml::node *existing = sectionTbl ? sectionTbl->get(keyName) : nullptr;
    if (!existing || (sectionName.empty() && !isRootKeyValue(*existing)))
        return {true, text, {}};

    const toml::source_region &region = existing->source();
    const size_t begin = lineStart(src, region.begin.line);
    const size_t end = lineEndInclusive(src, region.end.line);
    std::string out = src;
    out.erase(begin, end - begin);

    toml::table expected = tbl;
    toml::table *expectedSection = sectionName.empty() ? &expected : tableAt(expected, sectionName);
    expectedSection->erase(keyName);
    return verified({true, QString::fromStdString(out), {}}, expected);
}

Result setBlockValue(const QString &text, const Block &block, const QString &key,
                     const QString &formattedValue)
{
    const std::string src = text.toStdString();
    Result failure;
    const std::optional<toml::table> tbl = parseText(src, &failure);
    if (!tbl)
        return failure;

    const toml::table *t = findBlock(*tbl, block.array, block.key, block.value);
    if (!t) {
        return appendBlock(text, block.array,
                           {{block.key, tomlQuote(block.value)}, {key, formattedValue}});
    }

    const std::string keyName = key.toStdString();
    const std::string value = formattedValue.toStdString();
    toml::table holder;
    if (!parseValue(value, &holder))
        return fail("'" + value + "' is not a TOML value");
    toml::table expected = *tbl;
    blockAt(expected, block.array, indexIn(*tbl, block.array, t))
        .insert_or_assign(keyName, *holder.get("v"));

    const Lines lines(src);
    std::string out = src;
    if (const toml::node *existing = t->get(keyName)) {
        if (isHeaderNode(*existing, lines))
            return fail("'" + keyName + "' is a table, not a value");
        const toml::source_region &region = existing->source();
        const size_t begin = byteOffset(src, region.begin.line, region.begin.column);
        const size_t end = byteOffset(src, region.end.line, region.end.column);
        out.replace(begin, end - begin, value);
        return verified({true, QString::fromStdString(out), {}}, expected);
    }

    const size_t at = lineEndInclusive(src, lastKeyLine(*t, lines));
    std::string line = keyName + " = " + value + "\n";
    if (at == src.size() && !src.empty() && src.back() != '\n')
        line.insert(0, 1, '\n');
    out.insert(at, line);
    return verified({true, QString::fromStdString(out), {}}, expected);
}

QStringList blockValues(const QString &text, const QString &array, const QString &key)
{
    Result failure;
    const std::optional<toml::table> tbl = parseText(text.toStdString(), &failure);
    const toml::node *node = tbl ? tbl->get(array.toStdString()) : nullptr;
    const toml::array *arr = node ? node->as_array() : nullptr;
    QStringList out;
    if (!arr)
        return out;
    for (auto &&el : *arr) {
        if (!isHeaderTable(el))
            continue;
        if (const auto *v = el.as_table()->get_as<std::string>(key.toStdString()))
            out.append(QString::fromStdString(v->get()));
    }
    return out;
}

QStringList blockKeys(const QString &text, const Block &block)
{
    Result failure;
    const std::optional<toml::table> tbl = parseText(text.toStdString(), &failure);
    const toml::table *t = tbl ? findBlock(*tbl, block.array, block.key, block.value) : nullptr;
    if (!t)
        return {};
    // toml++ keeps a table's keys sorted; order them by where they're written.
    std::vector<std::pair<toml::source_position, QString>> keys;
    for (auto &&[k, v] : *t)
        keys.emplace_back(k.source().begin, QString::fromStdString(std::string(k.str())));
    std::sort(keys.begin(), keys.end(), [](const auto &a, const auto &b) {
        return std::pair(a.first.line, a.first.column) < std::pair(b.first.line, b.first.column);
    });
    QStringList out;
    for (const auto &key : keys)
        out.append(key.second);
    return out;
}

Result removeBlockValue(const QString &text, const Block &block, const QString &key)
{
    const std::string src = text.toStdString();
    Result failure;
    const std::optional<toml::table> tbl = parseText(src, &failure);
    if (!tbl)
        return failure;

    const toml::table *t = findBlock(*tbl, block.array, block.key, block.value);
    const toml::node *existing = t ? t->get(key.toStdString()) : nullptr;
    if (!existing || isHeaderNode(*existing, Lines(src)))
        return {true, text, {}};

    const toml::source_region &region = existing->source();
    const size_t begin = lineStart(src, region.begin.line);
    const size_t end = lineEndInclusive(src, region.end.line);
    std::string out = src;
    out.erase(begin, end - begin);

    toml::table expected = *tbl;
    blockAt(expected, block.array, indexIn(*tbl, block.array, t)).erase(key.toStdString());
    return verified({true, QString::fromStdString(out), {}}, expected);
}

Result appendBlock(const QString &text, const QString &array,
                   const QList<std::pair<QString, QString>> &values)
{
    const std::string src = text.toStdString();
    Result failure;
    const std::optional<toml::table> tbl = parseText(src, &failure);
    if (!tbl)
        return failure;

    std::string block = "[[" + array.toStdString() + "]]\n";
    toml::table element;
    for (const auto &[key, value] : values) {
        block += key.toStdString() + " = " + value.toStdString() + "\n";
        toml::table holder;
        if (!parseValue(value.toStdString(), &holder))
            return fail("'" + value.toStdString() + "' is not a TOML value");
        element.insert_or_assign(key.toStdString(), *holder.get("v"));
    }
    toml::table expected = *tbl;
    if (toml::array *arr = arrayAt(expected, array)) {
        arr->push_back(element);
    } else if (!expected.get(array.toStdString())) {
        toml::array created;
        created.push_back(element);
        expected.insert(array.toStdString(), std::move(created));
    } else {
        return fail("'" + array.toStdString() + "' is not an array of tables");
    }

    size_t at = std::string::npos;
    if (const toml::table *last = lastBlock(*tbl, array)) {
        const Lines lines(src);
        at = lines.start(spanOf(*last, lines, headerLines(*tbl, lines)).last + 1);
    }
    return verified({true, QString::fromStdString(insertBlockText(src, at, block)), {}}, expected);
}

Result removeBlock(const QString &text, const Block &block)
{
    const std::string src = text.toStdString();
    Result failure;
    const std::optional<toml::table> tbl = parseText(src, &failure);
    if (!tbl)
        return failure;

    const toml::table *t = findBlock(*tbl, block.array, block.key, block.value);
    if (!t)
        return {true, text, {}};

    const Lines lines(src);
    const auto [begin, end] = removalRange(spanOf(*t, lines, headerLines(*tbl, lines)), lines);
    std::string out = src;
    out.erase(begin, end - begin);

    toml::table expected = *tbl;
    toml::array *arr = arrayAt(expected, block.array);
    arr->erase(arr->cbegin() + indexIn(*tbl, block.array, t));
    if (arr->empty())
        expected.erase(block.array.toStdString());
    return verified({true, QString::fromStdString(out), {}}, expected);
}

Result moveBlock(const QString &text, const Block &block, const QString &beforeValue)
{
    const std::string src = text.toStdString();
    Result failure;
    const std::optional<toml::table> tbl = parseText(src, &failure);
    if (!tbl)
        return failure;

    const toml::table *t = findBlock(*tbl, block.array, block.key, block.value);
    if (!t)
        return fail("no [[" + block.array.toStdString() + "]] with " + block.key.toStdString()
                    + " = \"" + block.value.toStdString() + "\"");
    const toml::table *target = nullptr;
    if (!beforeValue.isEmpty()) {
        target = findBlock(*tbl, block.array, block.key, beforeValue);
        if (!target)
            return fail("no [[" + block.array.toStdString() + "]] with " + block.key.toStdString()
                        + " = \"" + beforeValue.toStdString() + "\"");
        if (target == t)
            return {true, text, {}};
    }

    toml::table expected = *tbl;
    {
        toml::array *arr = arrayAt(expected, block.array);
        const size_t from = indexIn(*tbl, block.array, t);
        size_t to = target ? indexIn(*tbl, block.array, target) : arr->size();
        toml::table moving = *arr->get(from)->as_table();
        arr->erase(arr->cbegin() + from);
        if (to > from)
            to--;
        arr->insert(arr->cbegin() + to, std::move(moving));
    }

    const Lines lines(src);
    const std::vector<size_t> headers = headerLines(*tbl, lines);
    const Span span = spanOf(*t, lines, headers);
    const std::string moved = lines.text(span.first, span.last);
    const auto [begin, end] = removalRange(span, lines);

    if (!target) {
        const toml::table *last = lastBlock(*tbl, block.array);
        if (last == t)
            return {true, text, {}};
        const size_t at = lines.start(spanOf(*last, lines, headers).last + 1);
        const std::string rest = src.substr(0, begin) + src.substr(end);
        const std::string out = insertBlockText(rest, at - (end - begin), moved);
        return verified({true, QString::fromStdString(out), {}}, expected);
    }

    // Spans don't overlap, so the target starts either before or after the removed bytes.
    const size_t at = lines.start(spanOf(*target, lines, headers).first);
    std::string out;
    if (at < begin)
        out = src.substr(0, at) + moved + "\n" + src.substr(at, begin - at) + src.substr(end);
    else
        out = src.substr(0, begin) + src.substr(end, at - end) + moved + "\n" + src.substr(at);
    return verified({true, QString::fromStdString(out), {}}, expected);
}

namespace {
bool writeAtomically(const QString &path, const QString &text, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(text.toUtf8());
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

QString readFile(const QString &path, bool *ok)
{
    QFile file(path);
    *ok = file.open(QIODevice::ReadOnly);
    return *ok ? QString::fromUtf8(file.readAll()) : QString();
}
}

bool setValueInFile(const QString &path, const QString &section, const QString &key,
                     const QString &formattedValue, QString *error)
{
    bool ok = false;
    const QString original = readFile(path, &ok);
    if (!ok) {
        if (error)
            *error = QStringLiteral("could not read %1").arg(path);
        return false;
    }
    const Result result = setValue(original, section, key, formattedValue);
    if (!result.ok) {
        if (error)
            *error = result.error;
        return false;
    }
    if (result.text == original)
        return true;
    return writeAtomically(path, result.text, error);
}

bool removeValueInFile(const QString &path, const QString &section, const QString &key,
                        QString *error)
{
    bool ok = false;
    const QString original = readFile(path, &ok);
    if (!ok) {
        if (error)
            *error = QStringLiteral("could not read %1").arg(path);
        return false;
    }
    const Result result = removeValue(original, section, key);
    if (!result.ok) {
        if (error)
            *error = result.error;
        return false;
    }
    if (result.text == original)
        return true;
    return writeAtomically(path, result.text, error);
}
}
