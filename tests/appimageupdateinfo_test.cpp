#include "appimageupdateinfo.h"
#include "gamefile.h"
#include "libraryfiles.h"
#include "releasesource.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>

namespace {

// A 64-bit little-endian ELF with a section table: the null section, .shstrtab and, when `section`
// is given, .upd_info holding it padded to 1024 bytes like AppImage runtimes do.
QByteArray makeElf(const std::optional<QByteArray> &section, bool appImage = true)
{
    const QByteArray names = QByteArray("\0.shstrtab\0.upd_info\0", 21);
    QByteArray info = section.value_or(QByteArray());
    info.resize(1024);
    const quint64 namesAt = 64, infoAt = 128, tableAt = infoAt + 1024;
    const quint16 count = section ? 3 : 2;

    QByteArray b(tableAt + 64 * count, '\0');
    b.replace(0, 4, "\x7f" "ELF");
    b[4] = 2;
    b[5] = 1;
    b[6] = 1;
    if (appImage)
        b.replace(8, 3, QByteArray("AI\x02", 3));
    qToLittleEndian<quint16>(2, b.data() + 16);
    qToLittleEndian<quint16>(62, b.data() + 18);
    qToLittleEndian<quint64>(tableAt, b.data() + 40);
    qToLittleEndian<quint16>(64, b.data() + 58);
    qToLittleEndian<quint16>(count, b.data() + 60);
    qToLittleEndian<quint16>(1, b.data() + 62);
    b.replace(namesAt, names.size(), names);
    b.replace(infoAt, info.size(), info);

    const auto header = [&](int index, quint32 name, quint64 offset, quint64 size) {
        char *at = b.data() + tableAt + 64 * index;
        qToLittleEndian<quint32>(name, at);
        qToLittleEndian<quint64>(offset, at + 24);
        qToLittleEndian<quint64>(size, at + 32);
    };
    header(1, 1, namesAt, quint64(names.size()));
    if (section)
        header(2, 11, infoAt, 1024);
    return b;
}

QByteArray gh(const QByteArray &tag = "latest", const QByteArray &file = "helium-*-x86_64.AppImage.zsync")
{
    return "gh-releases-zsync|imputnet|helium-linux|" + tag + "|" + file;
}

bool anyContains(const QStringList &list, const QString &needle)
{
    for (const QString &s : list) {
        if (s.contains(needle))
            return true;
    }
    return false;
}

QString dump(const QStringList &warnings)
{
    return warnings.join(u'\n');
}

}

class AppImageUpdateInfoTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString write(const QString &name, const QByteArray &bytes, bool executable = true)
    {
        const QString path = m_dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
            qFatal("could not write %s", qPrintable(path));
        file.close();
        if (executable)
            file.setPermissions(file.permissions() | QFileDevice::ExeOwner);
        return path;
    }

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void readsTheSection()
    {
        const QString path = write(QStringLiteral("a.AppImage"), makeElf(gh()));
        QCOMPARE(AppImageUpdateInfo::readSection(path), std::optional<QString>(QString::fromLatin1(gh())));
        const auto info = AppImageUpdateInfo::read(path);
        QVERIFY(info);
        QCOMPARE(info->source, QStringLiteral("github:imputnet/helium-linux"));
        QCOMPARE(info->asset, QStringLiteral("helium-*-x86_64.AppImage"));
        QVERIFY(!info->prereleases);
    }

    void forms()
    {
        const auto parse = [](const QByteArray &text) { return AppImageUpdateInfo::parse(QString::fromLatin1(text)); };

        // A tag other than latest turns pre-releases on.
        const auto tagged = parse(gh("continuous"));
        QVERIFY(tagged && tagged->prereleases);

        const auto direct = parse("gh-releases-direct|o|r|latest|Game-*.AppImage");
        QVERIFY(direct);
        QCOMPARE(direct->source, QStringLiteral("github:o/r"));
        QCOMPARE(direct->asset, QStringLiteral("Game-*.AppImage"));

        const auto plain = parse(gh("latest", "Game.AppImage"));
        QVERIFY(plain);
        QCOMPARE(plain->asset, QStringLiteral("Game.AppImage"));

        // Not a GitHub release: ignored.
        QVERIFY(!parse("zsync|https://example.org/game.AppImage.zsync"));
        QVERIFY(!parse("pling-v1-zsync|123|game.AppImage|game-*.AppImage.zsync"));
        QVERIFY(!parse("gh-releases-zsync|only|three|parts"));
        QVERIFY(!parse("gh-releases-zsync|bad owner|repo|latest|x.zsync"));
        QVERIFY(!parse(""));
    }

    void notReadable()
    {
        QVERIFY(!AppImageUpdateInfo::read(m_dir.filePath(QStringLiteral("missing"))));
        QVERIFY(!AppImageUpdateInfo::read(m_dir.path()));
        QVERIFY(!AppImageUpdateInfo::read(write(QStringLiteral("script"), "#!/bin/sh\necho hi\n")));
        QVERIFY(!AppImageUpdateInfo::read(write(QStringLiteral("text"), "gh-releases-zsync|a|b|latest|c.zsync")));
        QVERIFY(!AppImageUpdateInfo::read(write(QStringLiteral("empty"), "")));

        // An ELF without the section, and with the section empty (a build without update information).
        const QString without = write(QStringLiteral("without"), makeElf(std::nullopt));
        QVERIFY(!AppImageUpdateInfo::readSection(without));
        QVERIFY(!AppImageUpdateInfo::readSection(write(QStringLiteral("blank"), makeElf(QByteArray()))));

        // 32-bit and big-endian ELF files aren't read.
        QByteArray small = makeElf(gh());
        small[4] = 1;
        QVERIFY(!AppImageUpdateInfo::read(write(QStringLiteral("elf32"), small)));
        QByteArray big = makeElf(gh());
        big[5] = 2;
        QVERIFY(!AppImageUpdateInfo::read(write(QStringLiteral("big"), big)));

        // A section table that points outside the file, and one cut short.
        QByteArray wild = makeElf(gh());
        qToLittleEndian<quint64>(quint64(1) << 40, wild.data() + 40);
        QVERIFY(!AppImageUpdateInfo::read(write(QStringLiteral("wild"), wild)));
        const QByteArray whole = makeElf(gh());
        QVERIFY(!AppImageUpdateInfo::read(write(QStringLiteral("cut"), whole.left(whole.size() - 100))));
        QByteArray count = makeElf(gh());
        qToLittleEndian<quint16>(60000, count.data() + 60);
        QVERIFY(!AppImageUpdateInfo::read(write(QStringLiteral("count"), count)));
    }

    void zsyncTrimmed()
    {
        QCOMPARE(ReleaseSources::withoutZsync(QStringLiteral("a-*.AppImage.zsync")), QStringLiteral("a-*.AppImage"));
        QCOMPARE(ReleaseSources::withoutZsync(QStringLiteral("a-*.AppImage.ZSYNC; b*.zsync ;c")),
                 QStringLiteral("a-*.AppImage;b*;c"));
        QCOMPARE(ReleaseSources::withoutZsync(QStringLiteral("a.AppImage")), QStringLiteral("a.AppImage"));
        QCOMPARE(ReleaseSources::withoutZsync(QString()), QString());
    }

    void gameKeys()
    {
        const QString file = QStringLiteral("library/games/x.toml");
        const auto parsed = parseGameFile("title = \"X\"\nplatform = \"linux\"\n"
                                          "update_source = \"github:o/r\"\n"
                                          "update_asset = \"x-*.AppImage.zsync;y-*.AppImage\"\n"
                                          "update_prereleases = true\n",
                                          file);
        QVERIFY2(parsed.warnings.isEmpty(), qPrintable(dump(parsed.warnings)));
        QCOMPARE(parsed.game.updateSource, QStringLiteral("github:o/r"));
        QCOMPARE(parsed.game.updateAsset, QStringLiteral("x-*.AppImage;y-*.AppImage"));
        QVERIFY(parsed.game.updatePrereleases);

        const auto gitlab = parseGameFile("title = \"X\"\nupdate_source = \"gitlab:git.example.org/a/b\"\n"
                                          "update_prereleases = true\n",
                                          file);
        QCOMPARE(gitlab.game.updateSource, QStringLiteral("gitlab:git.example.org/a/b"));
        QVERIFY(!gitlab.game.updatePrereleases);
        QVERIFY2(anyContains(gitlab.warnings, QStringLiteral("update_prereleases works for github: releases only")),
                 qPrintable(dump(gitlab.warnings)));

        const auto bad = parseGameFile("title = \"X\"\nupdate_source = \"owner/repo\"\n"
                                       "update_asset = \"dist/x.AppImage\"\nupdate_prereleases = \"yes\"\n",
                                       file);
        QCOMPARE(bad.warnings.size(), 3);
        QVERIFY2(anyContains(bad.warnings, QStringLiteral("update_source starts with github:, gitlab: or forgejo:")), qPrintable(dump(bad.warnings)));
        QVERIFY(anyContains(bad.warnings, QStringLiteral("update_asset is a file name pattern, without folders")));
        QVERIFY(anyContains(bad.warnings, QStringLiteral("update_prereleases must be true or false")));
        QVERIFY(bad.game.updateSource.isEmpty());
        QVERIFY(bad.game.updateAsset.isEmpty());
        QVERIFY(!bad.game.updatePrereleases);

        QString error;
        QVERIFY(!GameFields::validate(QStringLiteral("update_source"), QStringLiteral("nope"), &error).isValid());
        QVERIFY(GameFields::isCleared(QStringLiteral("update_source"), QString()));
        QVERIFY(GameFields::isCleared(QStringLiteral("update_prereleases"), false));
        QVERIFY(!GameFields::isCleared(QStringLiteral("update_prereleases"), true));
    }

    void scanCarriesTheSource()
    {
        const QString embedded = write(QStringLiteral("embedded.AppImage"), makeElf(gh("continuous")));
        const QString plain = write(QStringLiteral("plain.AppImage"), makeElf(std::nullopt));
        const QString script = write(QStringLiteral("run.sh"), "#!/bin/sh\n");

        const auto game = [](const QString &exe) {
            GameFile g;
            g.title = QStringLiteral("G");
            g.platform = QStringLiteral("linux");
            g.exe = exe;
            return g;
        };
        QMap<QString, GameFile> games;
        games.insert(QStringLiteral("embedded"), game(embedded));
        games.insert(QStringLiteral("plain"), game(plain));
        games.insert(QStringLiteral("script"), game(script));
        GameFile own = game(embedded);
        own.updateSource = QStringLiteral("gitlab:git.example.org/a/b");
        own.updateAsset = QStringLiteral("g-*.AppImage");
        games.insert(QStringLiteral("own"), own);
        GameFile proton = game(embedded);
        proton.platform = QStringLiteral("proton");
        games.insert(QStringLiteral("proton"), proton);

        const auto files = findProgramFiles(games, m_dir.path(), m_dir.path(), {}, false);

        const UpdateSource &fromImage = files.value(QStringLiteral("embedded")).updateSource;
        QCOMPARE(fromImage.source, QStringLiteral("github:imputnet/helium-linux"));
        QCOMPARE(fromImage.asset, QStringLiteral("helium-*-x86_64.AppImage"));
        QVERIFY(fromImage.prereleases);
        QVERIFY(fromImage.fromAppImage);
        QVERIFY(files.value(QStringLiteral("embedded")).appImage);

        const UpdateSource &mine = files.value(QStringLiteral("own")).updateSource;
        QCOMPARE(mine.source, QStringLiteral("gitlab:git.example.org/a/b"));
        QCOMPARE(mine.asset, QStringLiteral("g-*.AppImage"));
        QVERIFY(!mine.fromAppImage);
        // The AppImage's own is still known, for "Use the AppImage's".
        QCOMPARE(files.value(QStringLiteral("own")).embedded.source, QStringLiteral("github:imputnet/helium-linux"));

        QVERIFY(!files.value(QStringLiteral("plain")).updateSource.isSet());
        QVERIFY(files.value(QStringLiteral("plain")).appImage);
        QVERIFY(!files.value(QStringLiteral("script")).updateSource.isSet());
        QVERIFY(!files.value(QStringLiteral("script")).appImage);
        QVERIFY(!files.value(QStringLiteral("proton")).updateSource.isSet());

        // The previous scan's reading is reused while the file's size and time stay, and read again when they change.
        QHash<QString, ProgramFiles> previous = files;
        previous[QStringLiteral("embedded")].embedded.source = QStringLiteral("github:cached/value");
        QCOMPARE(findProgramFiles(games, m_dir.path(), m_dir.path(), previous, false)
                     .value(QStringLiteral("embedded")).updateSource.source,
                 QStringLiteral("github:cached/value"));
        previous[QStringLiteral("embedded")].exeSize += 1;
        QCOMPARE(findProgramFiles(games, m_dir.path(), m_dir.path(), previous, false)
                     .value(QStringLiteral("embedded")).updateSource.source,
                 QStringLiteral("github:imputnet/helium-linux"));
    }

    void lattisOwnAppImage()
    {
        const QDir dir(QStringLiteral(APPIMAGE_DIR));
        const QStringList found = dir.entryList({QStringLiteral("Lattis-*.AppImage")}, QDir::Files, QDir::Time);
        if (found.isEmpty())
            QSKIP("no Lattis AppImage has been built (tools/appimage.sh)");
        // Built with --update-info: the public repo's releases; without: no section content.
        const auto info = AppImageUpdateInfo::read(dir.filePath(found.first()));
        if (!info)
            QSKIP("the newest Lattis AppImage was built without update information");
        QVERIFY(info->source.startsWith(QLatin1String("github:")));
        QVERIFY(!info->asset.isEmpty());
        QVERIFY(!info->asset.endsWith(QLatin1String(".zsync")));
    }
};

QTEST_APPLESS_MAIN(AppImageUpdateInfoTest)
#include "appimageupdateinfo_test.moc"
