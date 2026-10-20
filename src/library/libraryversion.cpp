#include "libraryversion.h"
#include "libraryfiles.h"
#include "librarystore.h"
#include "tomlpatch.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSaveFile>
#include <QUuid>

namespace LibraryVersion {
namespace {
QString tr(const char *text)
{
    return QCoreApplication::translate("Library", text);
}

QString backUp(const QString &libraryDir, int version)
{
    const QString backup = QStringLiteral("%1/backup-v%2").arg(libraryDir).arg(version);
    if (QFileInfo::exists(backup))
        return tr("%1 already exists; move it away to let the upgrade make a new backup").arg(backup);
    QDirIterator it(libraryDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString source = it.next();
        const QString relative = source.mid(libraryDir.size() + 1);
        if (relative.startsWith(QStringLiteral("backup-v")))
            continue;
        const QString target = backup + u'/' + relative;
        if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !QFile::copy(source, target))
            return tr("could not copy %1 to %2").arg(source, target);
    }
    return {};
}
}

QList<Upgrade> upgrades()
{
    return {};
}

Status prepare(const QString &dataDir, int appVersion, const QList<Upgrade> &steps, bool withSetup)
{
    Status status;
    const QString libraryDir = dataDir + QStringLiteral("/library");
    const QString infoPath = libraryDir + QStringLiteral("/library.toml");

    const QString newId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QFileInfo::exists(infoPath)) {
        QDir().mkpath(libraryDir);
        QSaveFile file(infoPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QStringLiteral("# Library format version and id. Don't edit: the app keeps them.\n"
                                      "version = %1\nid = \"%2\"\n%3")
                           .arg(appVersion)
                           .arg(newId, withSetup ? QStringLiteral("setup = \"pending\"\n") : QString())
                           .toUtf8());
            if (file.commit()) {
                status.id = newId;
                status.created = true;
                status.setupPending = withSetup;
            }
        }
        if (!QFileInfo::exists(infoPath))
            status.problems.append(tr("could not create %1").arg(infoPath));
        status.version = appVersion;
        return status;
    }

    QFile file(infoPath);
    const QByteArray bytes = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    const Parsed<LibraryInfo> info = parseLibraryInfo(bytes, QStringLiteral("library/library.toml"));
    if (!info.ok) {
        // Without the version, saving could damage a library this copy doesn't understand.
        status.readOnly = true;
        status.problems = info.warnings;
        status.notice = tr("Read-only: library/library.toml has an error. Changes won't be saved until it's fixed.");
        status.reason = tr("library/library.toml has an error");
        return status;
    }
    status.version = info.value.version;

    if (status.version > appVersion) {
        status.readOnly = true;
        status.notice = tr("Read-only: a newer version of Lattis saved this library (format %1; this copy knows %2). "
                           "Changes won't be saved.")
                            .arg(status.version)
                            .arg(appVersion);
        status.reason = tr("the library was saved by a newer version of Lattis");
        qCInfo(lcLibrary).noquote() << QStringLiteral("library format %1 is newer than %2; opening read-only")
                                           .arg(status.version)
                                           .arg(appVersion);
        return status;
    }
    // Kept, or made for a library from before ids (a failure only means no Steam sync this run).
    const auto withId = [&] {
        status.setupPending = info.value.setupPending;
        status.id = info.value.id;
        if (!status.id.isEmpty())
            return status;
        QString error;
        if (TomlPatch::setValueInFile(infoPath, {}, QStringLiteral("id"), u'"' + newId + u'"', &error))
            status.id = newId;
        else
            qCWarning(lcLibrary).noquote() << QStringLiteral("could not add an id to %1: %2").arg(infoPath, error);
        return status;
    };
    if (status.version == appVersion)
        return withId();

    qCInfo(lcLibrary).noquote() << QStringLiteral("upgrading the library from format %1 to %2")
                                       .arg(status.version)
                                       .arg(appVersion);
    bool backedUp = false;
    const auto fail = [&](const QString &error) {
        status.readOnly = true;
        status.problems.append(error);
        status.notice = backedUp
            ? tr("Read-only: upgrading the library failed. The library from before the upgrade is in "
                 "library/backup-v%1.")
                  .arg(info.value.version)
            : tr("Read-only: the library needs an upgrade but couldn't be backed up first, so it was left as "
                 "it was.");
        status.reason = tr("the library upgrade failed");
        qCWarning(lcLibrary).noquote() << error;
        return status;
    };

    if (const QString error = backUp(libraryDir, status.version); !error.isEmpty())
        return fail(tr("could not back up the library before upgrading it: %1").arg(error));
    backedUp = true;

    while (status.version < appVersion) {
        const auto step = std::find_if(steps.cbegin(), steps.cend(),
                                       [&](const Upgrade &u) { return u.from == status.version; });
        if (step == steps.cend())
            return fail(tr("no upgrade from library format %1").arg(status.version));
        if (const QString error = step->run(libraryDir); !error.isEmpty())
            return fail(tr("upgrading the library from format %1 failed: %2").arg(status.version).arg(error));
        ++status.version;

        QString error;
        if (!TomlPatch::setValueInFile(infoPath, {}, QStringLiteral("version"), QString::number(status.version), &error))
            return fail(tr("could not write the new version to %1: %2").arg(infoPath, error));
        qCInfo(lcLibrary).noquote() << QStringLiteral("upgraded the library to format %1").arg(status.version);
    }
    return withId();
}
}
