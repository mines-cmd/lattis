#include "savesindex.h"

#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>

#include <yaml-cpp/yaml.h>

namespace {
constexpr quint32 kMagic = 0x4c534958;
constexpr QDataStream::Version kStreamVersion = QDataStream::Qt_6_8;

// Indexing a missing key gives a node that throws when asked its type; this gives a null one.
YAML::Node child(const YAML::Node &node, const char *key)
{
    if (!node.IsMap())
        return YAML::Node();
    const YAML::Node value = node[key];
    return value.IsDefined() ? value : YAML::Node();
}

QString text(const YAML::Node &node)
{
    return node.IsScalar() ? QString::fromStdString(node.Scalar()) : QString();
}

int number(const YAML::Node &node)
{
    bool ok = false;
    const int value = text(node).toInt(&ok);
    return ok && value > 0 ? value : 0;
}

bool isSave(const YAML::Node &tags)
{
    if (!tags.IsSequence() || tags.size() == 0)
        return true;
    for (const YAML::Node &tag : tags) {
        if (text(tag) == QLatin1String("save"))
            return true;
    }
    return false;
}

// Entries only for macOS or DOS are left out; Lattis runs on Linux and reads Proton's Windows layout.
bool wantedOs(const QString &os)
{
    return os.isEmpty() || os == QLatin1String("linux") || os == QLatin1String("windows");
}

std::optional<SavesPath> parsePath(const QString &path, const YAML::Node &entry)
{
    if (path.isEmpty() || !entry.IsMap() || !isSave(child(entry, "tags")))
        return std::nullopt;
    SavesPath out;
    out.path = path;
    const YAML::Node when = child(entry, "when");
    bool wanted = true;
    if (when.IsSequence() && when.size() > 0) {
        wanted = false;
        for (const YAML::Node &constraint : when) {
            SavesWhen w{text(child(constraint, "os")), text(child(constraint, "store"))};
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

SavesGame parseGame(const QString &name, const YAML::Node &node)
{
    SavesGame game;
    game.name = name;
    game.steamId = number(child(child(node, "steam"), "id"));
    game.gogId = number(child(child(node, "gog"), "id"));

    const YAML::Node installDir = child(node, "installDir");
    if (installDir.IsMap()) {
        for (auto it = installDir.begin(); it != installDir.end(); ++it) {
            const QString dir = text(it->first);
            if (!dir.isEmpty())
                game.installDirs.append(dir);
        }
    }

    const YAML::Node files = child(node, "files");
    if (files.IsMap()) {
        for (auto it = files.begin(); it != files.end(); ++it) {
            if (auto path = parsePath(text(it->first), it->second))
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
    YAML::Node root;
    try {
        root = YAML::Load(std::string(yaml.constData(), size_t(yaml.size())));
    } catch (const YAML::Exception &e) {
        result.warnings << QStringLiteral("not a manifest: %1").arg(QString::fromStdString(e.msg));
        return result;
    }
    if (!root.IsMap()) {
        result.warnings << QStringLiteral("not a manifest: the top level is not a list of games");
        return result;
    }

    QHash<QString, QStringList> aliases;
    int skipped = 0;
    for (auto it = root.begin(); it != root.end(); ++it) {
        try {
            const QString name = text(it->first);
            if (name.isEmpty() || !it->second.IsMap())
                continue;
            const QString target = text(child(it->second, "alias"));
            if (!target.isEmpty()) {
                aliases[target].append(name);
                continue;
            }
            SavesGame game = parseGame(name, it->second);
            if (!game.paths.isEmpty())
                result.value.games.append(game);
        } catch (const YAML::Exception &) {
            ++skipped;
        }
    }
    if (skipped > 0)
        result.warnings << QStringLiteral("%1 games in the manifest could not be read").arg(skipped);

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
