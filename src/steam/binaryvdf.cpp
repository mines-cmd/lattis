#include "binaryvdf.h"

#include <QtEndian>

namespace BinaryVdf {
namespace {
// Deeper than any file Steam writes; stops a malformed file from exhausting the stack.
constexpr int kMaxDepth = 64;
constexpr char kEnd = 0x08;

int fixedSize(quint8 type)
{
    switch (type) {
    case Int32:
    case Float:
    case Pointer:
    case Color:
        return 4;
    case UInt64:
    case Int64:
        return 8;
    default:
        return 0;
    }
}

class Reader
{
public:
    explicit Reader(const QByteArray &bytes) : m_s(bytes) {}

    bool readFields(Node *map, int depth)
    {
        if (depth > kMaxDepth)
            return fail(QStringLiteral("nested too deeply"));
        while (true) {
            if (m_pos >= m_s.size())
                return fail(depth == 0 ? QStringLiteral("the file ends before its end marker")
                                       : QStringLiteral("the file ends inside \"%1\"").arg(QString::fromUtf8(map->key)));
            const quint8 type = quint8(m_s.at(m_pos));
            if (type == quint8(kEnd)) {
                ++m_pos;
                return true;
            }
            if (type != Map && type != String && type != WideString && fixedSize(type) == 0)
                return fail(QStringLiteral("unknown field type 0x%1").arg(type, 2, 16, QLatin1Char('0')));
            ++m_pos;
            Node node;
            node.type = Type(type);
            if (!readCString(&node.key))
                return false;
            if (type == Map) {
                if (!readFields(&node, depth + 1))
                    return false;
            } else if (type == String) {
                if (!readCString(&node.data))
                    return false;
            } else if (type == WideString) {
                if (!readWideString(&node.data))
                    return false;
            } else {
                const int size = fixedSize(type);
                if (m_pos + size > m_s.size())
                    return fail(QStringLiteral("the file ends inside \"%1\"").arg(QString::fromUtf8(node.key)));
                node.data = m_s.mid(m_pos, size);
                m_pos += size;
            }
            map->children.append(std::move(node));
        }
    }

    bool atEnd() const { return m_pos >= m_s.size(); }
    qsizetype pos() const { return m_pos; }
    QString error() const { return m_error; }

    bool fail(const QString &message)
    {
        m_error = QStringLiteral("byte %1: %2").arg(m_pos).arg(message);
        return false;
    }

private:
    bool readCString(QByteArray *out)
    {
        const qsizetype nul = m_s.indexOf('\0', m_pos);
        if (nul < 0)
            return fail(QStringLiteral("a string isn't ended"));
        *out = m_s.mid(m_pos, nul - m_pos);
        m_pos = nul + 1;
        return true;
    }

    bool readWideString(QByteArray *out)
    {
        for (qsizetype i = m_pos; i + 1 < m_s.size(); i += 2) {
            if (m_s.at(i) == '\0' && m_s.at(i + 1) == '\0') {
                *out = m_s.mid(m_pos, i - m_pos);
                m_pos = i + 2;
                return true;
            }
        }
        return fail(QStringLiteral("a string isn't ended"));
    }

    const QByteArray &m_s;
    qsizetype m_pos = 0;
    QString m_error;
};

void writeFields(const Node &map, QByteArray *out)
{
    for (const Node &node : map.children) {
        out->append(char(node.type));
        out->append(node.key);
        out->append('\0');
        if (node.type == Map) {
            writeFields(node, out);
        } else {
            out->append(node.data);
            if (node.type == String)
                out->append('\0');
            else if (node.type == WideString)
                out->append("\0\0", 2);
        }
    }
    out->append(kEnd);
}

bool sameKey(QByteArrayView a, QByteArrayView b)
{
    return a.compare(b, Qt::CaseInsensitive) == 0;
}
}

const Node *Node::child(QByteArrayView key) const
{
    for (const Node &c : children) {
        if (sameKey(c.key, key))
            return &c;
    }
    return nullptr;
}

Node *Node::child(QByteArrayView key)
{
    for (Node &c : children) {
        if (sameKey(c.key, key))
            return &c;
    }
    return nullptr;
}

QString Node::toString() const
{
    return type == String ? QString::fromUtf8(data) : QString();
}

qint32 Node::toInt32() const
{
    return type == Int32 && data.size() == 4 ? qFromLittleEndian<qint32>(data.constData()) : 0;
}

quint64 Node::toUInt64() const
{
    return type == UInt64 && data.size() == 8 ? qFromLittleEndian<quint64>(data.constData()) : 0;
}

Node Node::string(const QByteArray &key, const QString &value)
{
    return {String, key, value.toUtf8(), {}};
}

Node Node::int32(const QByteArray &key, qint32 value)
{
    QByteArray data(4, Qt::Uninitialized);
    qToLittleEndian(value, data.data());
    return {Int32, key, data, {}};
}

Node Node::uint64(const QByteArray &key, quint64 value)
{
    QByteArray data(8, Qt::Uninitialized);
    qToLittleEndian(value, data.data());
    return {UInt64, key, data, {}};
}

Node Node::map(const QByteArray &key, const QList<Node> &children)
{
    return {Map, key, {}, children};
}

Document parse(const QByteArray &bytes)
{
    Document doc;
    Reader reader(bytes);
    if (!reader.readFields(&doc.root, 0)) {
        doc.error = reader.error();
        doc.root = {};
        return doc;
    }
    if (!reader.atEnd()) {
        doc.error = QStringLiteral("byte %1: more after the end marker").arg(reader.pos());
        doc.root = {};
        return doc;
    }
    doc.ok = true;
    return doc;
}

QByteArray write(const Node &root)
{
    QByteArray out;
    writeFields(root, &out);
    return out;
}
}
