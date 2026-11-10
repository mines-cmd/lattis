#include "savesindex.h"

#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>

#include <yaml-cpp/eventhandler.h>
#include <yaml-cpp/yaml.h>

#include <functional>
#include <istream>
#include <streambuf>
#include <string_view>
#include <vector>

namespace {
constexpr quint32 kMagic = 0x4c534958;
constexpr QDataStream::Version kStreamVersion = QDataStream::Qt_6_8;

struct Node
{
    enum Kind { Null, Scalar, Sequence, Map };
    Kind kind = Null;
    std::string scalar;
    std::vector<Node> items; // a map's keys and values, alternating
};

// YAML::Load builds the whole manifest as yaml-cpp nodes, about 800 MB that the allocator keeps
// afterwards. This builds one game at a time from the parser's events and hands it on.
class ManifestReader : public YAML::EventHandler
{
public:
    using Entry = std::function<void(const Node &key, const Node &value)>;

    explicit ManifestReader(Entry entry)
        : m_entry(std::move(entry))
    {
    }

    Node::Kind rootKind() const { return m_rootKind; }

    void OnDocumentStart(const YAML::Mark &) override {}
    void OnDocumentEnd() override {}
    void OnNull(const YAML::Mark &, YAML::anchor_t) override { add(Node()); }
    void OnAlias(const YAML::Mark &, YAML::anchor_t) override { add(Node()); }
    void OnScalar(const YAML::Mark &, const std::string &, YAML::anchor_t, const std::string &value) override
    {
        add(Node{Node::Scalar, value, {}});
    }
    void OnSequenceStart(const YAML::Mark &, const std::string &, YAML::anchor_t, YAML::EmitterStyle::value) override
    {
        m_stack.push_back(Node{Node::Sequence, {}, {}});
    }
    void OnSequenceEnd() override { close(); }
    void OnMapStart(const YAML::Mark &, const std::string &, YAML::anchor_t, YAML::EmitterStyle::value) override
    {
        if (m_stack.empty() && m_rootKind == Node::Null && !m_inRoot) {
            m_inRoot = true;
            m_rootKind = Node::Map;
            return;
        }
        m_stack.push_back(Node{Node::Map, {}, {}});
    }
    void OnMapEnd() override
    {
        if (m_stack.empty())
            m_inRoot = false;
        else
            close();
    }

private:
    void close()
    {
        Node node = std::move(m_stack.back());
        m_stack.pop_back();
        add(std::move(node));
    }

    void add(Node node)
    {
        if (!m_stack.empty()) {
            m_stack.back().items.push_back(std::move(node));
        } else if (!m_inRoot) {
            m_rootKind = node.kind;
        } else if (!m_key) {
            m_key = std::move(node);
        } else {
            m_entry(*m_key, node);
            m_key.reset();
        }
    }

    Entry m_entry;
    std::vector<Node> m_stack;
    std::optional<Node> m_key;
    Node::Kind m_rootKind = Node::Null;
    bool m_inRoot = false;
};

class ByteArrayBuffer : public std::streambuf
{
public:
    explicit ByteArrayBuffer(const QByteArray &bytes)
    {
        char *data = const_cast<char *>(bytes.constData());
        setg(data, data, data + bytes.size());
    }
};

const Node *child(const Node *node, std::string_view key)
{
    if (!node || node->kind != Node::Map)
        return nullptr;
    for (size_t i = 0; i + 1 < node->items.size(); i += 2) {
        if (node->items[i].kind == Node::Scalar && node->items[i].scalar == key)
            return &node->items[i + 1];
    }
    return nullptr;
}

QString text(const Node *node)
{
    return node && node->kind == Node::Scalar ? QString::fromStdString(node->scalar) : QString();
}

int number(const Node *node)
{
    bool ok = false;
    const int value = text(node).toInt(&ok);
    return ok && value > 0 ? value : 0;
}

bool isSave(const Node *tags)
{
    if (!tags || tags->kind != Node::Sequence || tags->items.empty())
        return true;
    for (const Node &tag : tags->items) {
        if (text(&tag) == QLatin1String("save"))
            return true;
    }
    return false;
}

// Entries only for macOS or DOS are left out; Lattis runs on Linux and reads Proton's Windows layout.
bool wantedOs(const QString &os)
{
    return os.isEmpty() || os == QLatin1String("linux") || os == QLatin1String("windows");
}

std::optional<SavesPath> parsePath(const QString &path, const Node &entry)
{
    if (path.isEmpty() || entry.kind != Node::Map || !isSave(child(&entry, "tags")))
        return std::nullopt;
    SavesPath out;
    out.path = path;
    const Node *when = child(&entry, "when");
    bool wanted = true;
    if (when && when->kind == Node::Sequence && !when->items.empty()) {
        wanted = false;
        for (const Node &constraint : when->items) {
            SavesWhen w{text(child(&constraint, "os")), text(child(&constraint, "store"))};
            if (!wantedOs(w.os))
                continue;
            wanted = true;
            out.when.append(w);
        }
    }
    if (!wanted)
        return std::nullopt;
    return out;
}

SavesGame parseGame(const QString &name, const Node &node)
{
    SavesGame game;
    game.name = name;
    game.steamId = number(child(child(&node, "steam"), "id"));
    game.gogId = number(child(child(&node, "gog"), "id"));

    if (const Node *installDir = child(&node, "installDir"); installDir && installDir->kind == Node::Map) {
        for (size_t i = 0; i + 1 < installDir->items.size(); i += 2) {
            const QString dir = text(&installDir->items[i]);
            if (!dir.isEmpty())
                game.installDirs.append(dir);
        }
    }

    if (const Node *files = child(&node, "files"); files && files->kind == Node::Map) {
        for (size_t i = 0; i + 1 < files->items.size(); i += 2) {
            if (auto path = parsePath(text(&files->items[i]), files->items[i + 1]))
                game.paths.append(*path);
        }
    }
    return game;
}
}

namespace SavesIndexFile {
Parsed<SavesIndex> parse(const QByteArray &yaml)
{
    Parsed<SavesIndex> result;
    QHash<QString, QStringList> aliases;
    ManifestReader reader([&](const Node &key, const Node &value) {
        const QString name = text(&key);
        if (name.isEmpty() || value.kind != Node::Map)
            return;
        const QString target = text(child(&value, "alias"));
        if (!target.isEmpty()) {
            aliases[target].append(name);
            return;
        }
        SavesGame game = parseGame(name, value);
        if (!game.paths.isEmpty())
            result.value.games.append(game);
    });

    ByteArrayBuffer buffer(yaml);
    std::istream stream(&buffer);
    try {
        YAML::Parser parser(stream);
        parser.HandleNextDocument(reader);
    } catch (const YAML::Exception &e) {
        result.value.games.clear();
        result.warnings << QStringLiteral("not a manifest: %1").arg(QString::fromStdString(e.msg));
        return result;
    }
    if (reader.rootKind() != Node::Map) {
        result.warnings << QStringLiteral("not a manifest: the top level is not a list of games");
        return result;
    }

    for (SavesGame &game : result.value.games)
        game.aliases = aliases.value(game.name);
    result.ok = !result.value.games.isEmpty();
    if (!result.ok)
        result.warnings << QStringLiteral("not a manifest: it names no games with saves");
    return result;
}

bool save(const QString &path, const SavesIndex &index)
{
    QDir().mkpath(QFileInfo(path).path());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QDataStream s(&file);
    s.setVersion(kStreamVersion);
    s << kMagic << kFormatVersion << quint32(index.games.size());
    for (const SavesGame &g : index.games) {
        s << g.name << g.aliases << qint32(g.steamId) << qint32(g.gogId) << g.installDirs << quint32(g.paths.size());
        for (const SavesPath &p : g.paths) {
            s << p.path << quint32(p.when.size());
            for (const SavesWhen &w : p.when)
                s << w.os << w.store;
        }
    }
    return s.status() == QDataStream::Ok && file.commit();
}

std::optional<SavesIndex> load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    QDataStream s(&file);
    s.setVersion(kStreamVersion);
    quint32 magic = 0;
    quint32 version = 0;
    quint32 count = 0;
    s >> magic >> version >> count;
    if (s.status() != QDataStream::Ok || magic != kMagic || version != kFormatVersion)
        return std::nullopt;

    SavesIndex index;
    index.games.reserve(qsizetype(count));
    for (quint32 i = 0; i < count; ++i) {
        SavesGame g;
        qint32 steam = 0;
        qint32 gog = 0;
        quint32 paths = 0;
        s >> g.name >> g.aliases >> steam >> gog >> g.installDirs >> paths;
        g.steamId = steam;
        g.gogId = gog;
        for (quint32 j = 0; j < paths && s.status() == QDataStream::Ok; ++j) {
            SavesPath p;
            quint32 whens = 0;
            s >> p.path >> whens;
            for (quint32 k = 0; k < whens && s.status() == QDataStream::Ok; ++k) {
                SavesWhen w;
                s >> w.os >> w.store;
                p.when.append(w);
            }
            g.paths.append(p);
        }
        if (s.status() != QDataStream::Ok)
            return std::nullopt;
        index.games.append(g);
    }
    return index;
}
}
