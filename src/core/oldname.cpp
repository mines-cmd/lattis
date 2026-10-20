#include "oldname.h"

#include "apppaths.h"

#include <QDir>
#include <QFileInfo>
#include <QLockFile>

namespace OldName {
namespace {
const QString kOldName = QStringLiteral("game-launcher");

QString sibling(const QString &path)
{
    return QFileInfo(path).dir().filePath(kOldName);
}

bool moveFolder(const QString &from, const QString &to, QStringList *log)
{
    if (from.isEmpty() || !QFileInfo(from).isDir() || QFileInfo::exists(to))
        return false;
    QDir().mkpath(QFileInfo(to).path());
    const bool moved = QDir().rename(from, to);
    log->append(moved ? QStringLiteral("moved %1 to %2").arg(from, to)
                      : QStringLiteral("couldn't move %1 to %2").arg(from, to));
    return moved;
}
}

QStringList move(const Folders &folders)
{
    QStringList log;
    moveFolder(folders.stateFrom, folders.stateTo, &log);
    if (!moveFolder(folders.dataFrom, folders.dataTo, &log))
        return log;

    const QString oldKey = AppPaths::dataDirKey(folders.dataFrom);
    const QString newKey = AppPaths::dataDirKey(folders.dataTo);
    const QDir state(folders.stateTo);
    for (const QFileInfo &entry : state.entryInfoList(QDir::Files)) {
        if (!entry.fileName().contains(oldKey))
            continue;
        const QString to = state.filePath(entry.fileName().replace(oldKey, newKey));
        if (QFileInfo::exists(to))
            continue;
        log.append(QFile::rename(entry.filePath(), to) ? QStringLiteral("renamed %1 to %2").arg(entry.filePath(), to)
                                                       : QStringLiteral("couldn't rename %1 to %2").arg(entry.filePath(), to));
    }
    return log;
}

Folders folders(const QString &runtimeDir)
{
    Folders f;
    f.stateTo = AppPaths::stateDir();
    f.stateFrom = sibling(f.stateTo);

    const QString dataTo = AppPaths::dataDir();
    const QString dataFrom = sibling(dataTo);
    QLockFile running(QStringLiteral("%1/%2-%3.lock").arg(runtimeDir, kOldName, AppPaths::dataDirKey(dataFrom)));
    running.setStaleLockTime(0);
    if (running.tryLock(0)) {
        f.dataFrom = dataFrom;
        f.dataTo = dataTo;
    }
    return f;
}
}
