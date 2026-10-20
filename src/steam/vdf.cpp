#include "vdf.h"

#include <optional>

Q_LOGGING_CATEGORY(lcSteam, "launcher.steam")

namespace Vdf {
namespace {
// Deeper than any file Steam writes; stops a malformed file from exhausting the stack.
constexpr int kMaxDepth = 64;

struct Token
{
    enum Kind { String, Open, Close, Conditional, End, Error };
    Kind kind = End;
    qsizetype begin = 0;
    qsizetype end = 0;
    int line = 0;
    bool quoted = false;
    QString text;
};

class Lexer
{
public:
    explicit Lexer(const QByteArray &text) : m_s(text)
    {
        if (m_s.startsWith("\xEF\xBB\xBF"))
            m_pos = 3;
    }

    Token next()
    {
        if (m_peeked) {
            Token t = std::move(*m_peeked);
            m_peeked.reset();
            return t;
        }
        return read();
    }

    const Token &peek()
    {
        if (!m_peeked)
            m_peeked = read();
        return *m_peeked;
    }

private:
    Token read()
    {
        skipSpace();
        Token t;
        t.begin = m_pos;
        t.line = m_line;
        if (m_pos >= m_s.size()) {
            t.kind = Token::End;
            t.end = m_pos;
            return t;
        }
        const char c = m_s.at(m_pos);
        if (c == '{' || c == '}') {
            t.kind = c == '{' ? Token::Open : Token::Close;
            t.end = ++m_pos;
            return t;
        }
        if (c == '[') {
            const qsizetype close = m_s.indexOf(']', m_pos);
            const qsizetype newline = m_s.indexOf('\n', m_pos);
            if (close < 0 || (newline >= 0 && newline < close))
                return error(t, QStringLiteral("a condition ([…]) isn't closed"));
            t.kind = Token::Conditional;
            m_pos = close + 1;
            t.end = m_pos;
            return t;
        }
        if (c == '"')
            return quotedString(t);

        while (m_pos < m_s.size()) {
            const char b = m_s.at(m_pos);
            if (b == ' ' || b == '\t' || b == '\r' || b == '\n' || b == '"' || b == '{'
                || b == '}')
                break;
            ++m_pos;
        }
        t.kind = Token::String;
        t.end = m_pos;
        t.text = QString::fromUtf8(m_s.constData() + t.begin, t.end - t.begin);
        return t;
    }

    Token quotedString(Token &t)
    {
        QByteArray bytes;
        ++m_pos;
        while (true) {
            if (m_pos >= m_s.size())
                return error(t, QStringLiteral("a string isn't closed"));
            const char c = m_s.at(m_pos++);
            if (c == '"')
                break;
            if (c == '\n')
                ++m_line;
            if (c != '\\' || m_pos >= m_s.size()) {
                bytes += c;
                continue;
            }
            const char e = m_s.at(m_pos++);
            switch (e) {
            case 'n': bytes += '\n'; break;
            case 't': bytes += '\t'; break;
            case 'v': bytes += '\v'; break;
            case 'b': bytes += '\b'; break;
            case 'r': bytes += '\r'; break;
            case 'f': bytes += '\f'; break;
            case 'a': bytes += '\a'; break;
            case '\\': case '?': case '\'': case '"': bytes += e; break;
            default:
                // Not an escape Steam knows: kept as written.
                bytes += '\\';
                --m_pos;
                break;
            }
        }
        t.kind = Token::String;
        t.quoted = true;
        t.end = m_pos;
        t.text = QString::fromUtf8(bytes);
        return t;
    }

    Token error(Token &t, const QString &message)
    {
        t.kind = Token::Error;
        t.text = message;
        m_pos = m_s.size();
        return t;
    }

    void skipSpace()
    {
        while (m_pos < m_s.size()) {
            const char c = m_s.at(m_pos);
            if (c == '\n') {
                ++m_line;
                ++m_pos;
            } else if (c == ' ' || c == '\t' || c == '\r') {
                ++m_pos;
            } else if (c == '/' && m_pos + 1 < m_s.size() && m_s.at(m_pos + 1) == '/') {
                while (m_pos < m_s.size() && m_s.at(m_pos) != '\n')
                    ++m_pos;
            } else {
                break;
            }
        }
    }

    const QByteArray &m_s;
    qsizetype m_pos = 0;
    int m_line = 1;
    std::optional<Token> m_peeked;
};

QString lineError(int line, const QString &message)
{
    return QStringLiteral("line %1: %2").arg(line).arg(message);
}

bool parseBlock(Lexer &lexer, Node &parent, const Token *open, int depth, QString *error)
{
    if (depth > kMaxDepth) {
        *error = lineError(open->line, QStringLiteral("blocks nested too deeply"));
        return false;
    }
    while (true) {
        Token key = lexer.next();
        switch (key.kind) {
        case Token::Error:
            *error = lineError(key.line, key.text);
            return false;
        case Token::End:
            if (!open)
                return true;
            *error = lineError(key.line, QStringLiteral("the block opened on line %1 isn't closed")
                                             .arg(open->line));
            return false;
        case Token::Close:
            if (!open) {
                *error = lineError(key.line, QStringLiteral("a '}' without its '{'"));
                return false;
            }
            parent.end = key.end;
            return true;
        case Token::Open:
            *error = lineError(key.line, QStringLiteral("a '{' without a key"));
            return false;
        case Token::Conditional:
            continue;
        case Token::String:
            break;
        }

        Token value = lexer.next();
        if (!key.quoted && key.text.startsWith(u'#')) {
            // #include / #base "file": Steam merges another file in; we don't follow them.
            if (value.kind != Token::String) {
                *error = lineError(key.line, QStringLiteral("%1 without a file").arg(key.text));
                return false;
            }
            continue;
        }

        Node node;
        node.key = key.text;
        node.line = key.line;
        node.begin = key.begin;
        node.valueBegin = value.begin;
        if (value.kind == Token::Open) {
            node.isBlock = true;
            if (!parseBlock(lexer, node, &value, depth + 1, error))
                return false;
        } else if (value.kind == Token::String) {
            node.value = value.text;
            node.end = value.end;
            if (lexer.peek().kind == Token::Conditional)
                lexer.next();
        } else if (value.kind == Token::Error) {
            *error = lineError(value.line, value.text);
            return false;
        } else {
            *error = lineError(key.line, QStringLiteral("\"%1\" has no value").arg(key.text));
            return false;
        }
        parent.children.append(std::move(node));
    }
}

bool isBlank(char c)
{
    return c == ' ' || c == '\t';
}

qsizetype lineStart(const QByteArray &text, qsizetype pos)
{
    while (pos > 0 && text.at(pos - 1) != '\n')
        --pos;
    return pos;
}

std::optional<QByteArray> indentBefore(const QByteArray &text, qsizetype pos)
{
    const qsizetype start = lineStart(text, pos);
    for (qsizetype i = start; i < pos; ++i) {
        if (!isBlank(text.at(i)))
            return std::nullopt;
    }
    return text.mid(start, pos - start);
}

QByteArray newlineOf(const QByteArray &text)
{
    return text.contains("\r\n") ? QByteArrayLiteral("\r\n") : QByteArrayLiteral("\n");
}

EditResult failure(const QString &error)
{
    EditResult result;
    result.error = error;
    return result;
}
}

const Node *Node::child(QStringView name) const
{
    for (const Node &c : children) {
        if (c.key.compare(name, Qt::CaseInsensitive) == 0)
            return &c;
    }
    return nullptr;
}

const Node *Node::find(const QStringList &path) const
{
    const Node *node = this;
    for (const QString &key : path) {
        node = node->child(key);
        if (!node)
            return nullptr;
    }
    return node;
}

QString Node::valueOf(QStringView name, const QString &fallback) const
{
    const Node *c = child(name);
    return c && !c->isBlock ? c->value : fallback;
}

Document parse(const QByteArray &text)
{
    Document doc;
    doc.root.isBlock = true;
    doc.root.valueBegin = -1;
    doc.root.end = text.size();
    Lexer lexer(text);
    doc.ok = parseBlock(lexer, doc.root, nullptr, 0, &doc.error);
    if (!doc.ok)
        doc.root.children.clear();
    return doc;
}

QByteArray quoted(const QString &value)
{
    QByteArray out;
    out.reserve(value.size() + 2);
    out += '"';
    for (const char c : value.toUtf8()) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    out += '"';
    return out;
}

EditResult setValue(const QByteArray &text, const QStringList &path, const QString &value)
{
    if (path.isEmpty())
        return failure(QStringLiteral("no key given"));
    const Document doc = parse(text);
    if (!doc.ok)
        return failure(doc.error);

    const Node *block = &doc.root;
    qsizetype i = 0;
    for (; i < path.size(); ++i) {
        const Node *c = block->child(path.at(i));
        if (!c)
            break;
        const bool last = i == path.size() - 1;
        if (last && c->isBlock)
            return failure(QStringLiteral("\"%1\" is a block, not a value").arg(path.mid(0, i + 1).join(u'/')));
        if (!last && !c->isBlock)
            return failure(QStringLiteral("\"%1\" is a value, not a block").arg(path.mid(0, i + 1).join(u'/')));
        if (last) {
            EditResult result;
            result.ok = true;
            result.text = text;
            const QByteArray token = quoted(value);
            if (c->value != value || text.mid(c->valueBegin, c->end - c->valueBegin) != token)
                result.text.replace(c->valueBegin, c->end - c->valueBegin, token);
            return result;
        }
        block = c;
    }

    const QByteArray nl = newlineOf(text);
    const bool isRoot = block->valueBegin < 0;
    qsizetype pos;
    QByteArray indent;
    if (!block->children.isEmpty()) {
        const Node &last = block->children.constLast();
        pos = last.end;
        const auto blockIndent = isRoot ? std::optional<QByteArray>(QByteArray())
                                        : indentBefore(text, block->begin);
        indent = indentBefore(text, last.begin)
                     .value_or(blockIndent.value_or(QByteArray()) + (isRoot ? "" : "\t"));
    } else if (isRoot) {
        pos = text.size();
    } else {
        pos = block->valueBegin + 1;
        indent = indentBefore(text, block->begin).value_or(QByteArray()) + '\t';
    }

    QByteArray added;
    QByteArray closing;
    for (qsizetype j = i; j < path.size(); ++j) {
        const QByteArray key = quoted(path.at(j));
        if (j == path.size() - 1) {
            added += nl + indent + key + "\t\t" + quoted(value);
        } else {
            added += nl + indent + key + nl + indent + '{';
            closing.prepend(nl + indent + '}');
            indent += '\t';
        }
    }
    added += closing;
    if (isRoot && pos == 0)
        added.remove(0, nl.size());
    else if (isRoot && block->children.isEmpty() && text.endsWith('\n'))
        added = added.mid(nl.size()) + nl; // after the file's last line, keeping it ending in one

    EditResult result;
    result.text = text;
    result.text.insert(pos, added);

    const Document check = parse(result.text);
    const Node *set = check.ok ? check.root.find(path) : nullptr;
    if (!set || set->isBlock || set->value != value)
        return failure(QStringLiteral("the edited file doesn't read back"));
    result.ok = true;
    return result;
}

EditResult remove(const QByteArray &text, const QStringList &path)
{
    const Document doc = parse(text);
    if (!doc.ok)
        return failure(doc.error);
    EditResult result;
    result.ok = true;
    result.text = text;
    const Node *node = path.isEmpty() ? nullptr : doc.root.find(path);
    if (!node)
        return result;

    qsizetype begin = node->begin;
    qsizetype end = node->end;
    // A condition after the value belongs to it.
    qsizetype after = end;
    while (after < text.size() && isBlank(text.at(after)))
        ++after;
    if (after < text.size() && text.at(after) == '[') {
        const qsizetype close = text.indexOf(']', after);
        if (close >= 0)
            end = close + 1;
    }

    after = end;
    while (after < text.size() && isBlank(text.at(after)))
        ++after;
    if (after + 1 < text.size() && text.at(after) == '/' && text.at(after + 1) == '/')
        after = text.indexOf('\n', after) < 0 ? text.size() : text.indexOf('\n', after);
    if (after < text.size() && text.at(after) == '\r')
        ++after;
    const bool lineEnds = after >= text.size() || text.at(after) == '\n';
    if (indentBefore(text, begin) && lineEnds) {
        begin = lineStart(text, begin);
        end = after < text.size() ? after + 1 : after;
    } else {
        while (begin > 0 && isBlank(text.at(begin - 1)))
            --begin;
    }
    result.text.remove(begin, end - begin);

    const Document check = parse(result.text);
    if (!check.ok) {
        result.ok = false;
        result.error = QStringLiteral("the edited file doesn't read back");
    }
    return result;
}
}
