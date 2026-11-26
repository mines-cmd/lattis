#pragma once

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTest>

#include <unistd.h>

// Writes `data` to `path`, making its folders first. A failure fails the test that called it.
inline void writeFile(const QString &path, const QByteArray &data = {}, bool executable = false)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(data), data.size());
    file.close();
    if (executable)
        QVERIFY(file.setPermissions(file.permissions() | QFile::ExeOwner));
}

// A file of `size` bytes, for tests that only care how big it is.
inline void writeFile(const QString &path, qint64 size)
{
    writeFile(path, QByteArray(size, 'x'));
}

// Empty when the file is missing or can't be read.
inline QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// Copies a folder with its hidden files and empty folders, keeping links as links.
inline void copyTree(const QString &from, const QString &to)
{
    QVERIFY(QDir().mkpath(to));
    const QFileInfoList entries =
        QDir(from).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &entry : entries) {
        const QString target = to + u'/' + entry.fileName();
        if (entry.isSymLink()) {
            QByteArray link(4096, '\0');
            const ssize_t n = ::readlink(QFile::encodeName(entry.filePath()).constData(), link.data(), link.size());
            QVERIFY(n > 0);
            QCOMPARE(::symlink(link.left(n).constData(), QFile::encodeName(target).constData()), 0);
        } else if (entry.isDir()) {
            copyTree(entry.filePath(), target);
            if (QTest::currentTestFailed())
                return;
        } else {
            QVERIFY(QFile::copy(entry.filePath(), target));
        }
    }
}
