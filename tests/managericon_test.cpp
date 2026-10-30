#include "managericon.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

namespace {

void put(QByteArray &b, qsizetype at, quint32 value, int bytes)
{
    for (int i = 0; i < bytes; ++i)
        b[at + i] = char((value >> (8 * i)) & 0xff);
}

QByteArray pngOf(const QColor &color, int size)
{
    QImage image(size, size, QImage::Format_ARGB32);
    image.fill(color);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

// A PE32+ file whose only resources are one icon group and the PNG it points at.
QByteArray program(const QByteArray &png)
{
    constexpr quint32 kRva = 0x1000;
    QByteArray r(0xb4, '\0');
    constexpr quint32 high = 0x80000000u;
    const auto dir = [&](qsizetype at, int entries) { put(r, at + 14, quint32(entries), 2); };
    const auto entry = [&](qsizetype dirAt, int n, quint32 id, quint32 target) {
        put(r, dirAt + 16 + 8 * n, id, 4);
        put(r, dirAt + 20 + 8 * n, target, 4);
    };
    dir(0x00, 2);
    entry(0x00, 0, 3, high | 0x20);
    entry(0x00, 1, 14, high | 0x38);
    dir(0x20, 1);
    entry(0x20, 0, 1, high | 0x50);
    dir(0x38, 1);
    entry(0x38, 0, 1, high | 0x68);
    dir(0x50, 1);
    entry(0x50, 0, 0x409, 0x80);
    dir(0x68, 1);
    entry(0x68, 0, 0x409, 0x90);
    put(r, 0x80, kRva + 0xb4, 4);
    put(r, 0x84, quint32(png.size()), 4);
    put(r, 0x90, kRva + 0xa0, 4);
    put(r, 0x94, 20, 4);
    put(r, 0xa0 + 2, 1, 2);
    put(r, 0xa0 + 4, 1, 2);
    r[0xa0 + 6] = char(16);
    r[0xa0 + 7] = char(16);
    put(r, 0xa0 + 6 + 4, 1, 2);
    put(r, 0xa0 + 6 + 6, 32, 2);
    put(r, 0xa0 + 6 + 8, quint32(png.size()), 4);
    put(r, 0xa0 + 6 + 12, 1, 2);
    r.append(png);

    QByteArray file(0x200, '\0');
    file[0] = 'M';
    file[1] = 'Z';
    put(file, 0x3c, 0x40, 4);
    file.replace(0x40, 4, QByteArray("PE\0\0", 4));
    put(file, 0x40 + 4 + 2, 1, 2);
    put(file, 0x40 + 4 + 16, 240, 2);
    const qsizetype opt = 0x40 + 4 + 20;
    put(file, opt, 0x20b, 2);
    put(file, opt + 112 + 16, kRva, 4);
    put(file, opt + 112 + 20, quint32(r.size()), 4);
    const qsizetype section = opt + 240;
    put(file, section + 8, quint32(r.size()), 4);
    put(file, section + 12, kRva, 4);
    put(file, section + 16, quint32(r.size()), 4);
    put(file, section + 20, 0x200, 4);
    return file + r;
}

void write(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(bytes);
}

}

class ManagerIconTest : public QObject
{
    Q_OBJECT

private slots:
    void readsAnExeIcon();
    void aFileWithoutResourcesHasNoIcon();
    void parsesExecLines();
    void matchesPathsCommandsAndFlatpaks();
    void findsADesktopEntryIcon();
    void cachesAndForgets();
};

void ManagerIconTest::readsAnExeIcon()
{
    QTemporaryDir dir;
    const QString exe = dir.filePath(QStringLiteral("Tool.exe"));
    write(exe, program(pngOf(Qt::red, 16)));
    const QImage image = ManagerIcon::fromExe(exe);
    QVERIFY(!image.isNull());
    QCOMPARE(image.size(), QSize(16, 16));
    QCOMPARE(image.pixelColor(3, 3), QColor(Qt::red));
}

void ManagerIconTest::aFileWithoutResourcesHasNoIcon()
{
    QTemporaryDir dir;
    write(dir.filePath(QStringLiteral("a.exe")), QByteArray("MZ not really"));
    write(dir.filePath(QStringLiteral("b.exe")), program(pngOf(Qt::red, 16)).left(0x100));
    QVERIFY(ManagerIcon::fromExe(dir.filePath(QStringLiteral("a.exe"))).isNull());
    QVERIFY(ManagerIcon::fromExe(dir.filePath(QStringLiteral("b.exe"))).isNull());
    QVERIFY(ManagerIcon::fromExe(dir.filePath(QStringLiteral("missing.exe"))).isNull());
}

void ManagerIconTest::parsesExecLines()
{
    const ManagerIcon::Entry e = ManagerIcon::parseDesktop(
        "[Other]\nExec=/wrong\n[Desktop Entry]\nName=X\nExec=\"/opt/My App/run\" --flag %U\nIcon=myapp\nTryExec=/opt/My App/run\n");
    QCOMPARE(e.icon, QStringLiteral("myapp"));
    QCOMPARE(e.exec, (QStringList{QStringLiteral("/opt/My App/run"), QStringLiteral("--flag"),
                                  QStringLiteral("/opt/My App/run")}));
}

void ManagerIconTest::matchesPathsCommandsAndFlatpaks()
{
    const ManagerIcon::Entry path = ManagerIcon::parseDesktop("[Desktop Entry]\nExec=/opt/mo/mo2 %u\nIcon=a\n");
    QVERIFY(ManagerIcon::launches(path, QStringLiteral("/opt/mo/mo2")));
    QVERIFY(ManagerIcon::launches(path, QStringLiteral("mo2")));
    QVERIFY(!ManagerIcon::launches(path, QStringLiteral("/opt/other/mo2")));
    QVERIFY(!ManagerIcon::launches(path, QString()));

    const ManagerIcon::Entry flatpak = ManagerIcon::parseDesktop(
        "[Desktop Entry]\nExec=/usr/bin/flatpak run --branch=stable org.example.Tool @@u %U @@\nIcon=org.example.Tool\n");
    QVERIFY(ManagerIcon::launches(flatpak, QStringLiteral("org.example.Tool")));
    QVERIFY(!ManagerIcon::launches(flatpak, QStringLiteral("org.example.Other")));
}

void ManagerIconTest::findsADesktopEntryIcon()
{
    QTemporaryDir dir;
    const QString apps = dir.filePath(QStringLiteral("applications"));
    const QString icons = dir.filePath(QStringLiteral("icons"));
    write(apps + QStringLiteral("/tool.desktop"), "[Desktop Entry]\nExec=/opt/tool/tool\nIcon=tool\n");
    write(apps + QStringLiteral("/other.desktop"), "[Desktop Entry]\nExec=/opt/other\nIcon=other\n");
    write(icons + QStringLiteral("/hicolor/32x32/apps/tool.png"), pngOf(Qt::green, 32));
    write(icons + QStringLiteral("/hicolor/256x256/apps/tool.png"), pngOf(Qt::blue, 64));

    const QImage image = ManagerIcon::fromDesktop(QStringLiteral("/opt/tool/tool"), {apps}, {icons});
    QVERIFY(!image.isNull());
    QCOMPARE(image.size(), QSize(64, 64));
    QVERIFY(ManagerIcon::fromDesktop(QStringLiteral("/opt/none"), {apps}, {icons}).isNull());
}

void ManagerIconTest::cachesAndForgets()
{
    QTemporaryDir dir;
    const QString exe = dir.filePath(QStringLiteral("Tool.exe"));
    write(exe, program(pngOf(Qt::red, 16)));
    const QString cache = dir.filePath(QStringLiteral("cache"));

    const QString png = ManagerIcon::cached(cache, QStringLiteral("tool"), exe, true);
    QVERIFY(QFileInfo::exists(png));
    QCOMPARE(ManagerIcon::cached(cache, QStringLiteral("tool"), exe, true), png);
    QVERIFY(ManagerIcon::cached(cache, QStringLiteral("tool-2"), dir.filePath(QStringLiteral("nope.exe")), true).isEmpty());

    ManagerIcon::forget(cache, QStringLiteral("tool"));
    QVERIFY(!QFileInfo::exists(png));
    QVERIFY(QDir(cache + QStringLiteral("/mod-manager-icons")).entryList({QStringLiteral("tool-2-*")}, QDir::Files).size() == 1);
}

QTEST_MAIN(ManagerIconTest)

#include "managericon_test.moc"
