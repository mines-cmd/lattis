#pragma once

#include "apppaths.h"
#include "artimage.h"
#include "binaryvdf.h"
#include "library.h"
#include "librarycache.h"
#include "libraryfiles.h"
#include "listsettings.h"
#include "searchtext.h"
#include "steamdownloads.h"
#include "steamsync.h"
#include "vdf.h"

#include <QDirIterator>
#include <QImage>
#include <QImageReader>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>
#include <QtEndian>

#include <memory>

inline void copyTree(const QString &from, const QString &to)
{
    QDirIterator it(from, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString source = it.next();
        const QString target = to + source.mid(from.size());
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile::copy(source, target);
    }
}

inline void writeFile(const QString &path, const QByteArray &text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(text);
}

inline QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// The demo's settings.toml: [steam] path points at its fake Steam, which has its Steam games
// installed. A test that writes settings.toml keeps it, or those games aren't installed.
inline QByteArray demoSettings()
{
    return readFile(QStringLiteral(DEMO_DATA_DIR "/settings.toml"));
}

inline bool anyContains(const QStringList &list, const QString &needle)
{
    return std::any_of(list.cbegin(), list.cend(), [&](const QString &s) { return s.contains(needle); });
}

// The ids of the game rows, in list order, without the headers.
inline QStringList gameIds(const Library &library)
{
    QStringList ids;
    for (int row = 0; row < library.rowCount(); ++row) {
        if (const QString id = library.gameIdAt(row); !id.isEmpty())
            ids.append(id);
    }
    return ids;
}

// Steam editing a game's launch options in its Properties.
inline void setSteamLaunchOptions(const QString &localConfig, qint64 appId, const QString &options)
{
    const Vdf::EditResult r = Vdf::setValue(readFile(localConfig),
                                            {QStringLiteral("UserLocalConfigStore"), QStringLiteral("Software"),
                                             QStringLiteral("Valve"), QStringLiteral("Steam"), QStringLiteral("apps"),
                                             QString::number(appId), QStringLiteral("LaunchOptions")},
                                            options);
    QVERIFY(r.ok);
    writeFile(localConfig, r.text);
}

inline bool pending(const Library &library, const QString &id)
{
    return library.gameAt(library.rowOfGame(id)).value("pendingSteamEdits").toBool();
}

inline QString launchOptionsOf(const Library &library, const QString &id)
{
    return library.store().snapshot().content.games.value(id).launchOptions;
}

inline const QString kShortcutsVdf = QStringLiteral("/userdata/39734273/config/shortcuts.vdf");

inline QList<SteamShortcut> shortcutsIn(const QString &path)
{
    return SteamShortcuts::read(BinaryVdf::parse(readFile(path)).root);
}

inline SteamShortcut shortcutIn(const QString &path, quint32 appId)
{
    for (const SteamShortcut &s : shortcutsIn(path)) {
        if (s.appId == appId)
            return s;
    }
    return {};
}

// Edits shortcuts.vdf as Steam would.
inline void editShortcuts(const QString &path, const std::function<void(BinaryVdf::Node *)> &edit)
{
    BinaryVdf::Document doc = BinaryVdf::parse(readFile(path));
    QVERIFY(doc.ok);
    edit(&doc.root);
    writeFile(path, BinaryVdf::write(doc.root));
}

// An ELF file's start: the header and, at 64, one program header of `segment`'s type.
inline QByteArray elf(quint16 type, quint16 machine, quint32 segment = 1, bool is64 = true, bool bigEndian = false)
{
    QByteArray b(128, '\0');
    b.replace(0, 4, "\x7f" "ELF");
    b[4] = is64 ? 2 : 1;
    b[5] = bigEndian ? 2 : 1;
    b[6] = 1;
    const auto put16 = [&](int at, quint16 v) {
        bigEndian ? qToBigEndian(v, b.data() + at) : qToLittleEndian(v, b.data() + at);
    };
    const auto put32 = [&](int at, quint32 v) {
        bigEndian ? qToBigEndian(v, b.data() + at) : qToLittleEndian(v, b.data() + at);
    };
    put16(16, type);
    put16(18, machine);
    if (is64) {
        put32(32, 64); // e_phoff (its low half)
        put16(54, 56);
        put16(56, 1);
    } else {
        put32(28, 64);
        put16(42, 32);
        put16(44, 1);
    }
    put32(64, segment);
    return b;
}

class LibraryTestBase : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

protected:
    // A trash in the test's folder: moves what it's given there and lists it; with `works`
    // false, there's no trash.
    Uninstall::Trash fakeTrash(QStringList *taken, bool works = true);
    // Hollow Knight added with "+" (~/Games/Hollow Knight, with a save file), its shortcut in
    // the fake Steam, and a prefix in compatdata. Returns the game id; `appId` its shortcut.
    QString addUninstallGame(Library &library, quint32 *appId);
    // A copy of tests/data/steam as the fake home's native Steam.
    QString installFakeSteam();
    // That Steam, running: a copy of `sleep` named like its binary, in ~/.steam/steam.pid.
    // Killing the process closes it.
    std::unique_ptr<QProcess> startFakeSteam(const QString &root);
    // A Library on the scratch data dir that has finished its first scan.
    std::unique_ptr<Library> openLibrary();
    QString libraryId() const; // library.toml's id
    QString gamePath(const QString &id) const { return m_data + QStringLiteral("/library/games/") + id + ".toml"; }
    QString steamIndexPath() const { return m_data + QStringLiteral("/library/steam.toml"); }
    QString romIndexPath(const QString &platform) const
    {
        return m_data + QStringLiteral("/library/roms/") + platform + ".toml";
    }

    std::unique_ptr<QTemporaryDir> m_dir;
    InstallLocations m_installs; // fake: Ryubing and Azahar as Flatpaks that can read everything
    QString m_data;
    QString m_cache;
    QString m_state; // the state dir, for the Steam sync record; created by the first write
};
