#include "steamshortcuts.h"

#include <array>

using BinaryVdf::Node;

namespace {
const QByteArray kShortcuts = QByteArrayLiteral("shortcuts");

QList<Node> newFields(const SteamShortcut &s)
{
    return {
        Node::int32("appid", SteamShortcuts::signedAppId(s.appId)),
        Node::string("appname", s.name),
        Node::string("exe", SteamShortcuts::exeField(s.exe, s.exeArgs)),
        Node::string("StartDir", s.startDir),
        Node::string("icon", s.icon),
        Node::string("ShortcutPath", {}),
        Node::string("LaunchOptions", s.launchOptions),
        Node::int32("IsHidden", s.hidden ? 1 : 0),
        Node::int32("AllowDesktopConfig", 1),
        Node::int32("AllowOverlay", 1),
        Node::int32("OpenVR", 0),
        Node::int32("Devkit", 0),
        Node::string("DevkitGameID", {}),
        Node::int32("DevkitOverrideAppID", 0),
        Node::int32("LastPlayTime", qint32(s.lastPlayTime)),
        Node::string("FlatpakAppID", {}),
        Node::string("sortas", {}),
        Node::map("tags"),
    };
}

quint32 crc32(const QByteArray &bytes)
{
    static const std::array<quint32, 256> table = [] {
        std::array<quint32, 256> t {};
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    quint32 crc = 0xFFFFFFFFu;
    for (const char b : bytes)
        crc = table[(crc ^ quint8(b)) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

quint32 idFrom(const QString &exeField, const QString &name)
{
    return crc32((exeField + name).toUtf8()) | 0x80000000u;
}

QString stringOf(const Node &shortcut, QByteArrayView key)
{
    const Node *n = shortcut.child(key);
    return n ? n->toString() : QString();
}

// A shortcut's app ID; one without an `appid` field (written by an old Steam) has the one
// Steam derives from its exe and name.
quint32 appIdOf(const Node &shortcut)
{
    if (const Node *n = shortcut.child("appid"); n && n->type == BinaryVdf::Int32)
        return quint32(n->toInt32());
    return idFrom(stringOf(shortcut, "exe"), stringOf(shortcut, "appname"));
}

Node *shortcutsOf(Node *root)
{
    Node *list = root->child(kShortcuts);
    return list && list->type == BinaryVdf::Map ? list : nullptr;
}

qsizetype indexOf(const Node &list, quint32 appId)
{
    for (qsizetype i = 0; i < list.children.size(); ++i) {
        const Node &c = list.children.at(i);
        if (c.type == BinaryVdf::Map && appIdOf(c) == appId)
            return i;
    }
    return -1;
}

bool isQuoted(const QString &s)
{
    return s.size() >= 2 && s.startsWith(u'"') && s.endsWith(u'"');
}

QString quote(const QString &s)
{
    return u'"' + s + u'"';
}

QString withoutTrailingSlash(QString path)
{
    while (path.size() > 1 && path.endsWith(u'/'))
        path.chop(1);
    return path;
}

void setField(Node *shortcut, const Node &value)
{
    if (Node *n = shortcut->child(value.key)) {
        n->type = value.type;
        n->data = value.data;
        return;
    }
    static const QList<QByteArray> order = [] {
        QList<QByteArray> keys;
        for (const Node &n : newFields({}))
            keys.append(n.key.toLower());
        return keys;
    }();
    const qsizetype rank = order.indexOf(value.key.toLower());
    qsizetype at = 0;
    for (qsizetype i = 0; i < shortcut->children.size(); ++i) {
        const qsizetype r = order.indexOf(shortcut->children.at(i).key.toLower());
        if (r >= 0 && r < rank)
            at = i + 1;
    }
    shortcut->children.insert(at, value);
}
}

namespace SteamShortcuts {
QList<SteamShortcut> read(const BinaryVdf::Node &root)
{
    QList<SteamShortcut> out;
    const Node *list = root.child(kShortcuts);
    if (!list || list->type != BinaryVdf::Map)
        return out;
    for (const Node &n : list->children) {
        if (n.type != BinaryVdf::Map)
            continue;
        SteamShortcut s;
        s.appId = appIdOf(n);
        s.name = stringOf(n, "appname");
        splitExe(stringOf(n, "exe"), &s.exe, &s.exeArgs);
        s.startDir = unquoted(stringOf(n, "StartDir"));
        s.icon = unquoted(stringOf(n, "icon"));
        s.launchOptions = stringOf(n, "LaunchOptions");
        if (const Node *t = n.child("LastPlayTime"))
            s.lastPlayTime = quint32(t->toInt32());
        if (const Node *h = n.child("IsHidden"))
            s.hidden = h->toInt32() != 0;
        out.append(s);
    }
    return out;
}

quint64 gameId(quint32 appId)
{
    return (quint64(appId) << 32) | 0x02000000u;
}

QString exeField(const QString &program, const QString &args)
{
    return args.isEmpty() ? quote(program) : quote(program) + u' ' + args;
}

void splitExe(const QString &field, QString *program, QString *args)
{
    const QString s = field.trimmed();
    qsizetype end = -1;
    if (s.startsWith(u'"')) {
        const qsizetype close = s.indexOf(u'"', 1);
        if (close < 0) {
            *program = s.mid(1);
            args->clear();
            return;
        }
        *program = s.mid(1, close - 1);
        end = close + 1;
    } else {
        end = s.indexOf(u' ');
        if (end < 0)
            end = s.size();
        *program = s.left(end);
    }
    *args = s.mid(end).trimmed();
}

QString unquoted(const QString &path)
{
    return isQuoted(path) ? path.mid(1, path.size() - 2) : path;
}

quint32 newAppId(const QString &exeField, const QString &name, const QSet<quint32> &taken)
{
    quint32 id = idFrom(exeField, name);
    while (taken.contains(id))
        id = id == 0xFFFFFFFFu ? 0x80000000u : id + 1;
    return id;
}

bool add(BinaryVdf::Node *root, const SteamShortcut &shortcut)
{
    Node *list = shortcutsOf(root);
    if (!list) {
        if (root->child(kShortcuts))
            return false;
        root->children.append(Node::map(kShortcuts));
        list = &root->children.last();
    }
    if (indexOf(*list, shortcut.appId) >= 0)
        return false;
    qint64 next = list->children.size();
    for (const Node &c : std::as_const(list->children)) {
        bool ok = false;
        const qint64 n = c.key.toLongLong(&ok);
        if (ok && n >= next)
            next = n + 1;
    }
    list->children.append(Node::map(QByteArray::number(next), newFields(shortcut)));
    return true;
}

bool update(BinaryVdf::Node *root, const SteamShortcut &shortcut)
{
    Node *list = shortcutsOf(root);
    const qsizetype i = list ? indexOf(*list, shortcut.appId) : -1;
    if (i < 0)
        return false;
    Node *n = &list->children[i];

    if (stringOf(*n, "appname") != shortcut.name)
        setField(n, Node::string("appname", shortcut.name));

    const QString exe = stringOf(*n, "exe");
    QString program, args;
    splitExe(exe, &program, &args);
    if (program != shortcut.exe || args != shortcut.exeArgs) {
        // Unquoted stays unquoted when it can, as Steam ROM Manager writes it.
        const bool bare = !exe.trimmed().startsWith(u'"') && !exe.isEmpty() && !shortcut.exe.contains(u' ');
        const QString field = bare ? (shortcut.exeArgs.isEmpty() ? shortcut.exe : shortcut.exe + u' ' + shortcut.exeArgs)
                                   : exeField(shortcut.exe, shortcut.exeArgs);
        setField(n, Node::string("exe", field));
    }

    const QString startDir = stringOf(*n, "StartDir");
    if (withoutTrailingSlash(unquoted(startDir)) != withoutTrailingSlash(shortcut.startDir))
        setField(n, Node::string("StartDir", isQuoted(startDir) ? quote(shortcut.startDir) : shortcut.startDir));

    const QString icon = stringOf(*n, "icon");
    if (unquoted(icon) != shortcut.icon)
        setField(n, Node::string("icon", isQuoted(icon) ? quote(shortcut.icon) : shortcut.icon));

    if (stringOf(*n, "LaunchOptions") != shortcut.launchOptions)
        setField(n, Node::string("LaunchOptions", shortcut.launchOptions));

    const Node *hidden = n->child("IsHidden");
    if ((hidden && hidden->toInt32() != 0) != shortcut.hidden)
        setField(n, Node::int32("IsHidden", shortcut.hidden ? 1 : 0));
    return true;
}

bool remove(BinaryVdf::Node *root, quint32 appId)
{
    Node *list = shortcutsOf(root);
    const qsizetype i = list ? indexOf(*list, appId) : -1;
    if (i < 0)
        return false;
    list->children.removeAt(i);
    for (qsizetype k = i; k < list->children.size(); ++k)
        list->children[k].key = QByteArray::number(k);
    return true;
}
}
