#include "backup.h"
#include "gamefile.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSaveFile>
#include <QTimeZone>
#include <QtConcurrent/QtConcurrentRun>

#include <toml++/toml.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>

namespace Backup {
namespace {
QString tr(const char *text)
{
    return QCoreApplication::translate("Backup", text);
}

const QString kManifest = QStringLiteral("manifest.toml");

struct PartWriter
{
    Archive::Writer writer;
    QString part;
    Control control;
    qint64 manifestBytes = 0; // not counted: progress is the files' data, as the plan adds it up

    explicit PartWriter(const Control &c)
        : control(c)
    {
        writer.setControl({[this](qint64 bytes) {
                               if (control.progress)
                                   control.progress(bytes - manifestBytes, part);
                           },
                           c.cancel});
    }

    void setPart(const QString &name)
    {
        part = name;
        if (control.progress)
            control.progress(writer.bytes() - manifestBytes, part);
    }

    bool addManifest(const QByteArray &manifest)
    {
        if (!writer.addData(kManifest, manifest))
            return false;
        manifestBytes = manifest.size();
        return true;
    }

    bool addPlace(const BackupPlan::Place &place, const QString &name)
    {
        if (!place.exists)
            return true;
        const QFileInfo info(place.path);
        if (!info.exists()) {
            qCInfo(lcBackup).noquote() << place.path << "is gone; not backed up";
            return true;
        }
        if (info.isDir())
            return writer.addTree(place.path, name);
        return writer.addPath(place.path, name + u'/' + info.fileName());
    }
};

QString finish(PartWriter &w, const QString &partial, const QString &path)
{
    if (!w.writer.finish())
        return w.writer.error();
    if (::rename(QFile::encodeName(partial).constData(), QFile::encodeName(path).constData()) != 0) {
        const QString error = tr("Can't write %1: %2").arg(path, QString::fromLocal8Bit(std::strerror(errno)));
        QFile::remove(partial);
        return error;
    }
    return {};
}

QString partialPath(const QString &path)
{
    return path + QStringLiteral(".partial");
}
}

QString gamesRunning(const QStringList &titles)
{
    if (titles.isEmpty())
        return {};
    return tr("Close %1 first: it may be writing its saves.").arg(QLocale().createSeparatedList(titles));
}

QString placeName(const QString &emulatorId, const QString &table, int n)
{
    return QStringLiteral("emulators/%1/%2/%3").arg(emulatorId, table).arg(n);
}

QString write(BackupPlan::LibraryPlan plan, const QString &path, const QDateTime &created,
              const QString &appVersion, const Control &control)
{
    if (plan.dataBytes < 0)
        BackupPlan::measure(plan);

    const Archive::Skip planSkip = plan.skip();
    const QDir dataDir(plan.dataDir);
    const QString target = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QStringList own = {dataDir.relativeFilePath(target), dataDir.relativeFilePath(partialPath(target))};
    const Archive::Skip skip = [&](const QString &relative) { return planSkip(relative) || own.contains(relative); };

    const QString partial = partialPath(path);
    PartWriter w(control);
    if (!w.writer.open(partial))
        return w.writer.error();
    if (!w.addManifest(BackupPlan::manifest(plan, created, appVersion)))
        return w.writer.error();
    w.setPart(tr("The library"));
    if (!w.writer.addTree(plan.dataDir, QStringLiteral("data"), skip))
        return w.writer.error();
    return finish(w, partial, path);
}

QString write(BackupPlan::SavesPlan plan, const QString &path, const QDateTime &created,
              const QString &appVersion, const Control &control)
{
    if (!plan.measured)
        BackupPlan::measure(plan);

    const QString partial = partialPath(path);
    PartWriter w(control);
    if (!w.writer.open(partial))
        return w.writer.error();
    if (!w.addManifest(BackupPlan::manifest(plan, created, appVersion)))
        return w.writer.error();

    for (const BackupPlan::EmulatorPart &part : std::as_const(plan.emulators)) {
        if (!part.takesAnything(plan.keys))
            continue;
        if (std::any_of(part.saves.cbegin(), part.saves.cend(), [](const BackupPlan::Place &p) { return p.exists; }))
            w.setPart(tr("%1's saves").arg(part.name));
        for (qsizetype i = 0; i < part.saves.size(); ++i) {
            if (!w.addPlace(part.saves.at(i), placeName(part.id, QStringLiteral("saves"), int(i + 1))))
                return w.writer.error();
        }
        if (plan.keys
            && std::any_of(part.keys.cbegin(), part.keys.cend(), [](const BackupPlan::Place &p) { return p.exists; })) {
            w.setPart(tr("%1's keys and firmware").arg(part.name));
            for (qsizetype i = 0; i < part.keys.size(); ++i) {
                if (!w.addPlace(part.keys.at(i), placeName(part.id, QStringLiteral("keys"), int(i + 1))))
                    return w.writer.error();
            }
        }
        if (!part.besideRoms.isEmpty())
            w.setPart(tr("%1's saves next to ROMs").arg(part.name));
        for (const BesideRom::GameSaves &game : part.besideRoms) {
            for (const BesideRom::SaveFile &file : game.files) {
                const QFileInfo info(file.path);
                if (!info.exists())
                    continue;
                const QString name =
                    QStringLiteral("emulators/%1/beside-roms/%2/%3").arg(part.id, game.gameId, info.fileName());
                if (!w.writer.addPath(file.path, name))
                    return w.writer.error();
            }
        }
    }

    if (plan.proton) {
        for (const BackupPlan::ProtonPart &part : std::as_const(plan.protonGames)) {
            if (part.saves.skipped != ProtonSaves::Skipped::No)
                continue;
            const std::optional<ProtonSaves::Prefix> prefix = ProtonSaves::Prefix::read(part.saves.prefix);
            if (!prefix) {
                qCInfo(lcBackup).noquote() << part.saves.prefix << "is gone; not backed up";
                continue;
            }
            w.setPart(part.title);
            if (!w.writer.addTree(prefix->path, QStringLiteral("proton/") + part.gameId, ProtonSaves::skip(*prefix)))
                return w.writer.error();
        }
    }
    return finish(w, partial, path);
}

QString lastFile(const QString &stateDir, const QString &dataDirKey)
{
    return QDir(stateDir).filePath(QStringLiteral("backups-%1.toml").arg(dataDirKey));
}

QHash<QString, Last> readLast(const QString &file)
{
    QHash<QString, Last> out;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return out;
    const QByteArray bytes = f.readAll();
    toml::table root;
    try {
        root = toml::parse(std::string_view(bytes.constData(), size_t(bytes.size())));
    } catch (const toml::parse_error &) {
        qCWarning(lcBackup).noquote() << file << "doesn't parse; the last backups are forgotten";
        return out;
    }
    for (const QString &kind : {kLibrary, kSaves}) {
        const toml::table *table = root[kind.toStdString()].as_table();
        if (!table)
            continue;
        Last last;
        if (const auto *p = table->get_as<std::string>("path"))
            last.path = QString::fromStdString(p->get());
        if (const auto *t = table->get_as<toml::date_time>("time")) {
            const toml::date_time &dt = t->get();
            const QDate date(dt.date.year, dt.date.month, dt.date.day);
            const QTime time(dt.time.hour, dt.time.minute, dt.time.second);
            last.time = dt.offset ? QDateTime(date, time, QTimeZone::fromSecondsAheadOfUtc(dt.offset->minutes * 60))
                                  : QDateTime(date, time);
        }
        if (!last.path.isEmpty())
            out.insert(kind, last);
    }
    return out;
}

bool writeLast(const QString &file, const QString &kind, const Last &last, QString *error)
{
    QHash<QString, Last> all = readLast(file);
    all.insert(kind, last);
    QString text = QStringLiteral("# The last backup of each kind, for Settings → Library.\n");
    for (const QString &k : {kLibrary, kSaves}) {
        if (!all.contains(k))
            continue;
        const Last &l = all.value(k);
        const QDateTime time = l.time.toOffsetFromUtc(l.time.offsetFromUtc());
        text += QStringLiteral("\n[%1]\npath = %2\ntime = %3\n").arg(k, tomlString(l.path), time.toString(Qt::ISODate));
    }
    QDir().mkpath(QFileInfo(file).absolutePath());
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly) || out.write(text.toUtf8()) < 0 || !out.commit()) {
        if (error)
            *error = out.errorString();
        return false;
    }
    return true;
}
}

BackupJob::BackupJob(const QString &lastFile, std::function<QStringList()> runningGames, QObject *parent)
    : QObject(parent)
    , m_lastFile(lastFile)
    , m_runningGames(std::move(runningGames))
    , m_appVersion(QCoreApplication::applicationVersion())
{
    connect(&m_watcher, &QFutureWatcher<QString>::finished, this, &BackupJob::onFinished);
}

BackupJob::~BackupJob()
{
    m_cancel = true;
    m_watcher.waitForFinished();
}

QString BackupJob::gamesRunning() const
{
    return Backup::gamesRunning(m_runningGames ? m_runningGames() : QStringList());
}

QString BackupJob::start(const BackupPlan::LibraryPlan &plan, const QString &path)
{
    return begin(Backup::kLibrary, path, plan.bytes(), [=, this](const Backup::Control &control) {
        return Backup::write(plan, path, m_created, m_appVersion, control);
    });
}

QString BackupJob::start(const BackupPlan::SavesPlan &plan, const QString &path)
{
    if (const QString why = gamesRunning(); !why.isEmpty())
        return why;
    return begin(Backup::kSaves, path, plan.bytes(), [=, this](const Backup::Control &control) {
        return Backup::write(plan, path, m_created, m_appVersion, control);
    });
}

QString BackupJob::begin(const QString &kind, const QString &path, qint64 total,
                         const std::function<QString(const Backup::Control &)> &run)
{
    if (running())
        return tr("A backup is being made already.");
    m_cancel = false;
    m_kind = kind;
    m_path = path;
    m_created = QDateTime::currentDateTime();
    m_bytes = 0;
    m_total = qMax<qint64>(total, 0);
    m_part.clear();

    auto timer = std::make_shared<QElapsedTimer>();
    Backup::Control control;
    control.cancel = &m_cancel;
    control.progress = [this, timer, lastPart = QString()](qint64 bytes, const QString &part) mutable {
        if (part == lastPart && timer->isValid() && timer->elapsed() < 100)
            return;
        timer->start();
        lastPart = part;
        QMetaObject::invokeMethod(this, [this, bytes, part] {
            m_bytes = bytes;
            m_part = part;
            emit progressChanged();
        }, Qt::QueuedConnection);
    };
    m_watcher.setFuture(QtConcurrent::run([run, control] { return run(control); }));
    emit runningChanged();
    emit progressChanged();
    return {};
}

void BackupJob::cancel()
{
    m_cancel = true;
}

void BackupJob::onFinished()
{
    const QString error = m_watcher.result();
    if (error.isEmpty()) {
        m_bytes = m_total;
        QString saveError;
        if (!Backup::writeLast(m_lastFile, m_kind, {m_path, m_created}, &saveError))
            qCWarning(lcBackup).noquote() << "Couldn't save" << m_lastFile << saveError;
        qCInfo(lcBackup).noquote() << "Backed up" << m_kind << "to" << m_path;
    } else {
        qCInfo(lcBackup).noquote() << "Backup of" << m_kind << "failed:" << error;
    }
    const QString kind = m_kind;
    emit runningChanged();
    emit progressChanged();
    emit finished(kind, m_path, error);
}

Backup::Last BackupJob::last(const QString &kind) const
{
    return Backup::readLast(m_lastFile).value(kind);
}
