#include "backuparchive.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QMap>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

// Every entry under `folder`: type, permissions, time, size and contents (or link target).
QMap<QString, QString> describe(const QString &folder)
{
    QMap<QString, QString> tree;
    QDirIterator it(folder, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo info = it.nextFileInfo();
        struct stat st;
        lstat(QFile::encodeName(info.filePath()).constData(), &st);
        QString text = QStringLiteral("%1 %2").arg(st.st_mode & 07777, 0, 8).arg(qint64(st.st_mtim.tv_sec));
        if (info.isSymLink()) {
            text = QStringLiteral("link -> ") + info.readSymLink();
        } else if (info.isDir()) {
            text += QStringLiteral(" dir");
        } else {
            QFile file(info.filePath());
            if (!file.open(QIODevice::ReadOnly))
                text += QStringLiteral(" unreadable");
            text += QStringLiteral(" %1 %2").arg(info.size()).arg(
                QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha1).toHex()));
        }
        tree.insert(QDir(folder).relativeFilePath(info.filePath()), text);
    }
    return tree;
}

void writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

// An archive written by hand, for what the Writer never writes.
struct RawEntry
{
    QByteArray name;
    unsigned type = AE_IFREG;
    QByteArray data; // or the link's target
    QByteArray hardlink;
};

void writeRaw(const QString &path, const QList<RawEntry> &entries)
{
    struct archive *a = archive_write_new();
    archive_write_add_filter_zstd(a);
    archive_write_set_format_pax_restricted(a);
    QCOMPARE(archive_write_open_filename(a, QFile::encodeName(path).constData()), ARCHIVE_OK);
    for (const RawEntry &e : entries) {
        archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, e.name.constData());
        archive_entry_set_filetype(entry, e.type);
        archive_entry_set_perm(entry, 0644);
        if (!e.hardlink.isEmpty())
            archive_entry_set_hardlink(entry, e.hardlink.constData());
        if (e.type == AE_IFLNK)
            archive_entry_set_symlink(entry, e.data.constData());
        if (e.type == AE_IFREG)
            archive_entry_set_size(entry, e.data.size());
        archive_write_header(a, entry);
        if (e.type == AE_IFREG)
            archive_write_data(a, e.data.constData(), e.data.size());
        archive_entry_free(entry);
    }
    archive_write_close(a);
    archive_write_free(a);
}

const QString Manifest = QStringLiteral("manifest.toml");

} // namespace

class ArchiveTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void roundTrip();
    void namesAsTheFilesystemHasThem();
    void linkedFolderIsTakenAsAFolder();
    void skipLeavesOut();
    void specialFilesAreLeftOut();
    void largeSparseFile();
    void manifestAlone();
    void manifestMissing();
    void otherPartsArePassedOver();
    void refusals_data();
    void refusals();
    void linksLeadingAnywhere();
    void destinationThatExistsIsKept();
    void truncatedArchive();
    void garbageFile();
    void cancelWriting();
    void cancelExtracting();
    void unreadableFileFailsTheWrite();
    void unwritableTarget();

private:
    QString path(const QString &name) const { return m_dir->filePath(name); }
    // A source tree with a bit of everything.
    QString makeTree();

    std::unique_ptr<QTemporaryDir> m_dir;
};

void ArchiveTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
}

QString ArchiveTest::makeTree()
{
    const QString root = path(QStringLiteral("source"));
    writeFile(root + QStringLiteral("/settings.toml"), "[list]\nsort = \"title\"\n");
    writeFile(root + QStringLiteral("/library/games/switch-mario.toml"), "title = \"Mario\"\n");
    writeFile(root + QStringLiteral("/art/switch-mario/hero.png"), QByteArray(300000, 'x'));
    writeFile(root + QStringLiteral("/empty.txt"), {});
    // Non-ASCII, and a name and a path longer than ustar's 100 bytes.
    writeFile(root + QStringLiteral("/saves/Pokémon Ultra Sun — セーブ.sav"), "save");
    const QString longFolder = root + u'/' + QString(120, u'd') + u'/' + QString(120, u'e');
    writeFile(longFolder + u'/' + QString(200, u'f') + QStringLiteral(".sav"), "long");
    QDir().mkpath(root + QStringLiteral("/empty folder"));
    QFile::link(QStringLiteral("library/games"), root + QStringLiteral("/games link"));
    QFile::link(QStringLiteral("../settings.toml"), root + QStringLiteral("/saves/settings link"));
    const QString script = root + QStringLiteral("/run.sh");
    writeFile(script, "#!/bin/sh\n");
    QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup
                                      | QFile::ExeGroup);
    const QString readOnly = root + QStringLiteral("/read-only");
    QDir().mkpath(readOnly);
    writeFile(readOnly + QStringLiteral("/kept.txt"), "kept");
    QFile::setPermissions(readOnly, QFile::ReadOwner | QFile::ExeOwner);
    // Times that aren't "now", to see them kept.
    const QByteArray old = QFile::encodeName(root + QStringLiteral("/settings.toml"));
    const struct timespec times[2] = {{1600000000, 0}, {1600000000, 0}};
    utimensat(AT_FDCWD, old.constData(), times, 0);
    return root;
}

void ArchiveTest::roundTrip()
{
    const QString source = makeTree();
    const QString file = path(QStringLiteral("backup.tar.zst"));
    Archive::Writer writer;
    QVERIFY(writer.open(file));
    QVERIFY(writer.addData(Manifest, "format = 1\n"));
    QVERIFY2(writer.addTree(source, QStringLiteral("data")), qPrintable(writer.error()));
    QVERIFY(writer.finish());
    QVERIFY(QFileInfo::exists(file));

    const QString target = path(QStringLiteral("restored"));
    QString error;
    QVERIFY2(Archive::extract(file, {{QStringLiteral("data"), target}}, {Manifest}, &error), qPrintable(error));
    const auto before = describe(source);
    QCOMPARE(describe(target), before);
    QVERIFY(before.contains(QStringLiteral("empty folder")));
    QVERIFY(before.value(QStringLiteral("games link")).startsWith(QStringLiteral("link -> library/games")));
    QVERIFY(before.value(QStringLiteral("run.sh")).startsWith(QStringLiteral("750 ")));
    QVERIFY(before.value(QStringLiteral("settings.toml")).startsWith(QStringLiteral("644 1600000000 ")));
    QVERIFY(before.value(QStringLiteral("read-only")).startsWith(QStringLiteral("500 ")));

    // Restoring by hand: GNU tar gives back the same tree.
    const QString byHand = path(QStringLiteral("by hand"));
    QDir().mkpath(byHand);
    QProcess tar;
    tar.start(QStringLiteral("tar"), {QStringLiteral("--zstd"), QStringLiteral("-xpf"), file, QStringLiteral("-C"), byHand});
    if (!tar.waitForStarted())
        QSKIP("no tar");
    QVERIFY(tar.waitForFinished());
    QVERIFY2(tar.exitCode() == 0, tar.readAllStandardError().constData());
    QCOMPARE(describe(byHand + QStringLiteral("/data")), before);
    QVERIFY(QFileInfo::exists(byHand + QStringLiteral("/manifest.toml")));

    for (const QString &root : {source, target, byHand + QStringLiteral("/data")})
        QFile::setPermissions(root + QStringLiteral("/read-only"), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

// Names are bytes on Linux: UTF-8 ones stay plain pax names (GNU tar reads them without a
// word), and ones that aren't UTF-8 are kept byte for byte (libarchive marks just those).
void ArchiveTest::namesAsTheFilesystemHasThem()
{
    const QByteArray source = QFile::encodeName(path(QStringLiteral("names")));
    const QList<QByteArray> names = {"Pok\xc3\xa9mon.sav", "caf\xe9.sav", "dir\xff"};
    QVERIFY(::mkdir(source.constData(), 0755) == 0);
    for (const QByteArray &name : names) {
        const QByteArray file = source + '/' + name;
        if (name.startsWith("dir")) {
            QVERIFY(::mkdir(file.constData(), 0755) == 0);
            continue;
        }
        const int fd = ::open(file.constData(), O_WRONLY | O_CREAT | O_EXCL, 0644);
        QVERIFY(fd >= 0);
        QCOMPARE(::write(fd, name.constData(), size_t(name.size())), ssize_t(name.size()));
        ::close(fd);
    }
    const auto listing = [](const QByteArray &folder) {
        QList<QByteArray> out;
        DIR *dir = ::opendir(folder.constData());
        while (dir) {
            const dirent *entry = ::readdir(dir);
            if (!entry)
                break;
            if (qstrcmp(entry->d_name, ".") != 0 && qstrcmp(entry->d_name, "..") != 0)
                out.append(entry->d_name);
        }
        if (dir)
            ::closedir(dir);
        std::sort(out.begin(), out.end());
        return out;
    };

    const QString file = path(QStringLiteral("names.tar.zst"));
    Archive::Writer writer;
    QVERIFY(writer.open(file));
    QVERIFY2(writer.addTree(QFile::decodeName(source), QStringLiteral("data")), qPrintable(writer.error()));
    QVERIFY(writer.finish());

    const QString target = path(QStringLiteral("names restored"));
    QString error;
    QVERIFY2(Archive::extract(file, {{QStringLiteral("data"), target}}, {}, &error), qPrintable(error));
    const QByteArray restored = QFile::encodeName(target);
    QCOMPARE(listing(restored), listing(source));
    const int fd = ::open((restored + "/caf\xe9.sav").constData(), O_RDONLY);
    QVERIFY(fd >= 0);
    char contents[16] = {};
    QCOMPARE(::read(fd, contents, sizeof contents), ssize_t(8));
    ::close(fd);
    QCOMPARE(QByteArray(contents), QByteArray("caf\xe9.sav"));

    // GNU tar: the same names, and a warning only for the two that aren't UTF-8.
    const QString byHand = path(QStringLiteral("names by hand"));
    QDir().mkpath(byHand);
    QProcess tar;
    tar.start(QStringLiteral("tar"), {QStringLiteral("--zstd"), QStringLiteral("-xf"), file, QStringLiteral("-C"), byHand});
    if (!tar.waitForStarted())
        QSKIP("no tar");
    QVERIFY(tar.waitForFinished());
    QCOMPARE(tar.exitCode(), 0);
    QCOMPARE(listing(QFile::encodeName(byHand) + "/data"), listing(source));
    const QList<QByteArray> warnings = tar.readAllStandardError().trimmed().split('\n');
    QCOMPARE(warnings.size(), 2);
}

void ArchiveTest::linkedFolderIsTakenAsAFolder()
{
    const QString real = path(QStringLiteral("real saves"));
    writeFile(real + QStringLiteral("/a.sav"), "a");
    const QString link = path(QStringLiteral("saves"));
    QVERIFY(QFile::link(real, link));

    const QString file = path(QStringLiteral("backup.tar.zst"));
    Archive::Writer writer;
    QVERIFY(writer.open(file));
    QVERIFY(writer.addTree(link, QStringLiteral("emulators/mgba/saves")));
    QVERIFY(writer.finish());
    const QString target = path(QStringLiteral("restored"));
    QString error;
    QVERIFY2(Archive::extract(file, {{QStringLiteral("emulators/mgba/saves"), target}}, {}, &error),
             qPrintable(error));
    QVERIFY(QFileInfo(target).isDir() && !QFileInfo(target).isSymLink());
    QVERIFY(QFileInfo::exists(target + QStringLiteral("/a.sav")));
}

void ArchiveTest::skipLeavesOut()
{
    const QString source = makeTree();
    const QString file = path(QStringLiteral("backup.tar.zst"));
    Archive::Writer writer;
    QVERIFY(writer.open(file));
    QVERIFY(writer.addTree(source, QStringLiteral("data"), [](const QString &relative) {
        return relative == QLatin1String("art") || relative == QLatin1String("library/games/switch-mario.toml");
    }));
    QVERIFY(writer.finish());
    const QString target = path(QStringLiteral("restored"));
    QString error;
    QVERIFY(Archive::extract(file, {{QStringLiteral("data"), target}}, {}, &error));
    QVERIFY(!QFileInfo::exists(target + QStringLiteral("/art")));
    QVERIFY(QFileInfo(target + QStringLiteral("/library/games")).isDir());
    QVERIFY(!QFileInfo::exists(target + QStringLiteral("/library/games/switch-mario.toml")));
    QVERIFY(QFileInfo::exists(target + QStringLiteral("/settings.toml")));
    QFile::setPermissions(source + QStringLiteral("/read-only"), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QFile::setPermissions(target + QStringLiteral("/read-only"), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

void ArchiveTest::specialFilesAreLeftOut()
{
    const QString source = path(QStringLiteral("source"));
    writeFile(source + QStringLiteral("/a.sav"), "a");
    QCOMPARE(mkfifo(QFile::encodeName(source + QStringLiteral("/pipe")).constData(), 0644), 0);

    const QString file = path(QStringLiteral("backup.tar.zst"));
    Archive::Writer writer;
    QVERIFY(writer.open(file));
    QVERIFY(writer.addTree(source, QStringLiteral("data")));
    QVERIFY(writer.finish());
    const QString target = path(QStringLiteral("restored"));
    QString error;
    QVERIFY2(Archive::extract(file, {{QStringLiteral("data"), target}}, {}, &error), qPrintable(error));
    QVERIFY(QFileInfo::exists(target + QStringLiteral("/a.sav")));
    QVERIFY(!QFileInfo::exists(target + QStringLiteral("/pipe")));
}

void ArchiveTest::largeSparseFile()
{
    // Over 4 GB (and ustar's 8 GB limit is pax's job): sparse, so it costs no disk, with
    // a marker at the end to check the offsets.
    const qint64 size = 9LL * 1024 * 1024 * 1024;
    const QString source = path(QStringLiteral("source"));
    QDir().mkpath(source);
    {
        QFile big(source + QStringLiteral("/big.img"));
        QVERIFY(big.open(QIODevice::WriteOnly));
        QVERIFY(big.resize(size - 3));
        QVERIFY(big.seek(size - 3));
        big.write("end");
    }
    const QString file = path(QStringLiteral("backup.tar.zst"));
    Archive::Writer writer;
    QVERIFY(writer.open(file));
    QVERIFY(writer.addTree(source, QStringLiteral("data")));
    QVERIFY(writer.finish());
    QCOMPARE(writer.bytes(), size);
    QVERIFY(QFileInfo(file).size() < 10 * 1024 * 1024);

    const QString target = path(QStringLiteral("restored"));
    QString error;
    qint64 progress = 0;
    QVERIFY2(Archive::extract(file, {{QStringLiteral("data"), target}}, {}, &error,
                              {[&](qint64 bytes) { progress = bytes; }, nullptr}),
             qPrintable(error));
    QFile big(target + QStringLiteral("/big.img"));
    QCOMPARE(big.size(), size);
    QVERIFY(big.open(QIODevice::ReadOnly));
    QVERIFY(big.seek(size - 3));
    QCOMPARE(big.read(3), QByteArray("end"));
    QVERIFY(progress >= 3);
    struct stat st;
    QCOMPARE(stat(QFile::encodeName(big.fileName()).constData(), &st), 0);
    QVERIFY2(st.st_blocks * 512 < 100 * 1024 * 1024, "the restored file should stay sparse");
}

void ArchiveTest::manifestAlone()
{
    const QString source = path(QStringLiteral("source"));
    writeFile(source + QStringLiteral("/big.bin"), QCryptographicHash::hash("seed", QCryptographicHash::Sha512)
                                                      .repeated(200000)); // ~12 MB that doesn't compress much
    const QString file = path(QStringLiteral("backup.tar.zst"));
    Archive::Writer writer;
    QVERIFY(writer.open(file));
    QVERIFY(writer.addData(Manifest, "format = 1\nlibrary_version = 1\n"));
    QVERIFY(writer.addTree(source, QStringLiteral("data")));
    QVERIFY(writer.finish());

    // Cut the file short: the manifest still reads, as the rest isn't needed.
    QFile archive(file);
    QVERIFY(archive.resize(archive.size() / 2));
    QString error;
    const auto manifest = Archive::readFirst(file, Manifest, &error);
    QVERIFY2(manifest, qPrintable(error));
    QCOMPARE(*manifest, QByteArray("format = 1\nlibrary_version = 1\n"));
}

void ArchiveTest::manifestMissing()
{
    const QString file = path(QStringLiteral("other.tar.zst"));
    writeRaw(file, {{"notes.txt", AE_IFREG, "hi", {}}, {"manifest.toml", AE_IFREG, "format = 1\n", {}}});
    QString error;
    QVERIFY(!Archive::readFirst(file, Manifest, &error));
    QVERIFY(error.contains(QLatin1String("isn't a backup")));
}

void ArchiveTest::otherPartsArePassedOver()
{
    const QString file = path(QStringLiteral("backup.tar.zst"));
    writeRaw(file, {{"manifest.toml", AE_IFREG, "format = 1\n", {}},
                    {"data", AE_IFDIR, {}, {}},
                    {"data/settings.toml", AE_IFREG, "x", {}},
                    {"emulators/mgba/saves", AE_IFDIR, {}, {}},
                    {"emulators/mgba/saves/a.sav", AE_IFREG, "a", {}},
                    {"emulators/mgba/keys/prod.keys", AE_IFREG, "k", {}}});
    const QString saves = path(QStringLiteral("saves"));
    QString error;
    QVERIFY2(Archive::extract(file, {{QStringLiteral("emulators/mgba/saves"), saves}},
                              {Manifest, QStringLiteral("data"), QStringLiteral("emulators/mgba/keys")}, &error),
             qPrintable(error));
    QCOMPARE(QDir(saves).entryList(QDir::Files), QStringList{QStringLiteral("a.sav")});
    QCOMPARE(QDir(m_dir->path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot),
             (QStringList{QStringLiteral("backup.tar.zst"), QStringLiteral("saves")}));
}

void ArchiveTest::refusals_data()
{
    QTest::addColumn<QList<RawEntry>>("entries");
    QTest::addColumn<QString>("why");
    QTest::newRow("absolute") << QList<RawEntry>{{"/data/x", AE_IFREG, "x", {}}} << QStringLiteral("absolute");
    QTest::newRow("dot dot") << QList<RawEntry>{{"data/../x", AE_IFREG, "x", {}}} << QStringLiteral("elsewhere");
    QTest::newRow("dot dot at the end") << QList<RawEntry>{{"data/a/..", AE_IFDIR, {}, {}}}
                                        << QStringLiteral("elsewhere");
    QTest::newRow("link out") << QList<RawEntry>{{"data/a/out", AE_IFLNK, "../../x", {}}}
                              << QStringLiteral("outside");
    QTest::newRow("absolute link") << QList<RawEntry>{{"data/root", AE_IFLNK, "/", {}}} << QStringLiteral("outside");
    QTest::newRow("unknown part") << QList<RawEntry>{{"other/x", AE_IFREG, "x", {}}}
                                  << QStringLiteral("isn't part");
    QTest::newRow("prefix look-alike") << QList<RawEntry>{{"database/x", AE_IFREG, "x", {}}}
                                       << QStringLiteral("isn't part");
    QTest::newRow("hard link") << QList<RawEntry>{{"data/a", AE_IFREG, "a", {}}, {"data/b", AE_IFREG, {}, "data/a"}}
                               << QStringLiteral("hard link");
    QTest::newRow("device") << QList<RawEntry>{{"data/dev", AE_IFCHR, {}, {}}} << QStringLiteral("isn't a file");
    // A link inside its part is fine, but a file can't be written through it.
    QTest::newRow("through a link")
        << QList<RawEntry>{{"data/sub", AE_IFDIR, {}, {}}, {"data/l", AE_IFLNK, "sub", {}}, {"data/l/x", AE_IFREG, "x", {}}}
        << QStringLiteral("Can't restore");
}

void ArchiveTest::refusals()
{
    QFETCH(QList<RawEntry>, entries);
    QFETCH(QString, why);
    // Good entries first, so there's something to clean up.
    entries.prepend({"data/good.txt", AE_IFREG, "good", {}});
    const QString file = path(QStringLiteral("bad.tar.zst"));
    writeRaw(file, entries);
    const QString target = path(QStringLiteral("restored"));
    QString error;
    QVERIFY(!Archive::extract(file, {{QStringLiteral("data"), target}}, {}, &error));
    QVERIFY2(error.contains(why), qPrintable(error));
    QVERIFY(!QFileInfo::exists(target));
    QVERIFY(!QFileInfo::exists(path(QStringLiteral("x"))));
}

void ArchiveTest::linksLeadingAnywhere()
{
    const QString outside = path(QStringLiteral("outside"));
    QVERIFY(QDir().mkpath(outside));
    const QString file = path(QStringLiteral("backup.tar.zst"));
    const QByteArray absolute = QFile::encodeName(outside);
    writeRaw(file, {{"data/art", AE_IFLNK, absolute, {}}, {"data/up", AE_IFLNK, "../../x", {}}});
    const QString target = path(QStringLiteral("restored"));
    QString error;
    QVERIFY2(Archive::extract(file, {{QStringLiteral("data"), target, true}}, {}, &error), qPrintable(error));
    QCOMPARE(QFileInfo(target + QStringLiteral("/art")).symLinkTarget(), outside);
    QCOMPARE(QFileInfo(target + QStringLiteral("/up")).readSymLink(), QStringLiteral("../../x"));

    // Nothing is written through one even so.
    writeRaw(file, {{"data/art", AE_IFLNK, absolute, {}}, {"data/art/x", AE_IFREG, "x", {}}});
    const QString again = path(QStringLiteral("again"));
    QVERIFY(!Archive::extract(file, {{QStringLiteral("data"), again, true}}, {}, &error));
    QVERIFY2(error.contains(QLatin1String("Can't restore")), qPrintable(error));
    QVERIFY(!QFileInfo::exists(outside + QStringLiteral("/x")));
    QVERIFY(!QFileInfo::exists(again));
}

void ArchiveTest::destinationThatExistsIsKept()
{
    const QString file = path(QStringLiteral("backup.tar.zst"));
    writeRaw(file, {{"data/x", AE_IFREG, "x", {}}});
    const QString target = path(QStringLiteral("restored"));
    writeFile(target + QStringLiteral("/mine.txt"), "mine");
    QString error;
    QVERIFY(!Archive::extract(file, {{QStringLiteral("data"), target}}, {}, &error));
    QVERIFY(error.contains(QLatin1String("exists already")));
    QVERIFY(QFileInfo::exists(target + QStringLiteral("/mine.txt")));
}

void ArchiveTest::truncatedArchive()
{
    const QString source = path(QStringLiteral("source"));
    for (int i = 0; i < 20; ++i)
        writeFile(source + QStringLiteral("/file%1.bin").arg(i),
                  QCryptographicHash::hash(QByteArray::number(i), QCryptographicHash::Sha512).repeated(5000));
    const QString file = path(QStringLiteral("backup.tar.zst"));
    Archive::Writer writer;
    QVERIFY(writer.open(file));
    QVERIFY(writer.addData(Manifest, "format = 1\n"));
    QVERIFY(writer.addTree(source, QStringLiteral("data")));
    QVERIFY(writer.finish());
    QFile archive(file);
    QVERIFY(archive.resize(archive.size() * 2 / 3));

    const QString target = path(QStringLiteral("restored"));
    QString error;
    QVERIFY(!Archive::extract(file, {{QStringLiteral("data"), target}}, {Manifest}, &error));
    QVERIFY2(error.contains(QLatin1String("damaged")), qPrintable(error));
    QVERIFY(!QFileInfo::exists(target));
}

void ArchiveTest::garbageFile()
{
    const QString file = path(QStringLiteral("garbage.tar.zst"));
    writeFile(file, QCryptographicHash::hash("garbage", QCryptographicHash::Sha512).repeated(100));
    QString error;
    QVERIFY(!Archive::readFirst(file, Manifest, &error));
    QVERIFY(!error.isEmpty());
    const QString target = path(QStringLiteral("restored"));
    QVERIFY(!Archive::extract(file, {{QStringLiteral("data"), target}}, {}, &error));
    QVERIFY(!QFileInfo::exists(target));

    QVERIFY(!Archive::readFirst(path(QStringLiteral("missing.tar.zst")), Manifest, &error));
    QVERIFY(!error.isEmpty());
}

void ArchiveTest::cancelWriting()
{
    const QString source = path(QStringLiteral("source"));
    writeFile(source + QStringLiteral("/a.bin"), QByteArray(3 * 1024 * 1024, 'a'));
    writeFile(source + QStringLiteral("/b.bin"), QByteArray(3 * 1024 * 1024, 'b'));
    const QString file = path(QStringLiteral("backup.tar.zst"));
    std::atomic_bool cancel = false;
    Archive::Writer writer;
    writer.setControl({[&](qint64) { cancel = true; }, &cancel});
    QVERIFY(writer.open(file));
    QVERIFY(!writer.addTree(source, QStringLiteral("data")));
    QCOMPARE(writer.error(), QStringLiteral("Cancelled"));
    QVERIFY(!QFileInfo::exists(file));
    QVERIFY(!writer.finish());
}

void ArchiveTest::cancelExtracting()
{
    const QString source = path(QStringLiteral("source"));
    writeFile(source + QStringLiteral("/a.bin"), QByteArray(3 * 1024 * 1024, 'a'));
    writeFile(source + QStringLiteral("/b.bin"), QByteArray(3 * 1024 * 1024, 'b'));
    const QString file = path(QStringLiteral("backup.tar.zst"));
    Archive::Writer writer;
    QVERIFY(writer.open(file));
    QVERIFY(writer.addTree(source, QStringLiteral("data")));
    QVERIFY(writer.finish());

    std::atomic_bool cancel = false;
    const QString target = path(QStringLiteral("restored"));
    QString error;
    QVERIFY(!Archive::extract(file, {{QStringLiteral("data"), target}}, {}, &error,
                              {[&](qint64) { cancel = true; }, &cancel}));
    QCOMPARE(error, QStringLiteral("Cancelled"));
    QVERIFY(!QFileInfo::exists(target));
}

void ArchiveTest::unreadableFileFailsTheWrite()
{
    if (geteuid() == 0)
        QSKIP("root reads everything");
    const QString source = path(QStringLiteral("source"));
    writeFile(source + QStringLiteral("/a.sav"), "a");
    writeFile(source + QStringLiteral("/secret.sav"), "b");
    QFile::setPermissions(source + QStringLiteral("/secret.sav"), {});
    const QString file = path(QStringLiteral("backup.tar.zst"));
    Archive::Writer writer;
    QVERIFY(writer.open(file));
    QVERIFY(!writer.addTree(source, QStringLiteral("data")));
    QVERIFY2(writer.error().contains(QLatin1String("secret.sav")), qPrintable(writer.error()));
    QVERIFY(!QFileInfo::exists(file));
}

void ArchiveTest::unwritableTarget()
{
    Archive::Writer writer;
    QVERIFY(!writer.open(path(QStringLiteral("missing folder/backup.tar.zst"))));
    QVERIFY(!writer.error().isEmpty());
}

QTEST_GUILESS_MAIN(ArchiveTest)
#include "archive_test.moc"
