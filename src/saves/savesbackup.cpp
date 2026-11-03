#include "savesbackup.h"
#include "backuparchive.h"
#include "movedaside.h"
#include "protonsaves.h"
#include "savesmanifest.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <toml++/toml.hpp>

#include <algorithm>
#include <memory>

#include <cstdio>

namespace SavesBackup {
namespace {
struct Source
{
    QString path;
    Archive::Skip skip;
    bool wholePrefix = false;
};

struct Entry
{
    QString name;
    qint64 size = 0;
    qint64 mtime = 0;
};

bool isFolder(const QFileInfo &info)
{
    return !info.isSymLink() && info.isDir();
}

void walk(const QString &folder, const QString &name, const QString &relative, const Archive::Skip &skip,
          QList<Entry> &out)
{
    const QFileInfoList children =
        QDir(folder).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &child : children) {
        const QString childRelative = relative.isEmpty() ? child.fileName() : relative + u'/' + child.fileName();
        if (skip && skip(childRelative))
            continue;
        if (isFolder(child))
            walk(child.absoluteFilePath(), name, childRelative, skip, out);
        else
            out.append({name + u'/' + childRelative, child.isSymLink() ? 0 : child.size(),
                        child.lastModified().toMSecsSinceEpoch()});
    }
}

QList<Entry> collect(const QList<Source> &sources)
{
    QList<Entry> out;
    for (qsizetype n = 0; n < sources.size(); ++n) {
        const Source &source = sources.at(n);
        const QFileInfo info(source.path);
        const QString name = QString::number(n);
        if (isFolder(info))
            walk(source.path, name, {}, source.skip, out);
        else if (info.exists() || info.isSymLink())
            out.append({name + u'/' + info.fileName(), info.isSymLink() ? 0 : info.size(),
                        info.lastModified().toMSecsSinceEpoch()});
    }
    std::sort(out.begin(), out.end(), [](const Entry &a, const Entry &b) { return a.name < b.name; });
    return out;
}

QString signature(const QList<Entry> &entries)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    for (const Entry &e : entries)
        hash.addData(QStringLiteral("%1\t%2\t%3\n").arg(e.name).arg(e.size).arg(e.mtime).toUtf8());
    return QString::fromLatin1(hash.result().toHex());
}

QString quote(const QString &text)
{
    QString out = QStringLiteral("\"");
    for (const QChar c : text) {
        if (c == u'\\' || c == u'"')
            out += u'\\' + QString(c);
        else if (c.unicode() < 0x20 || c.unicode() == 0x7f)
            out += QStringLiteral("\\u") + QString::number(c.unicode(), 16).rightJustified(4, u'0');
        else
            out += c;
    }
    return out + u'"';
}

QString tilde(const QString &path, const QString &home)
{
    if (!home.isEmpty() && path == home)
        return QStringLiteral("~");
    if (!home.isEmpty() && path.startsWith(home + u'/'))
        return u'~' + path.mid(home.size());
    return path;
}

QByteArray manifest(const Input &input, const QString &matched, const QList<Source> &sources,
                    const QList<Entry> &entries, qint64 bytes, const QString &sig)
{
    QString out = QStringLiteral(
        "# A Lattis game saves backup. Restore it with Import in the app, or by hand:\n"
        "# tar --zstd -xf <this file>, then copy each files/<n>/ to the place its [[path]] says it came from.\n");
    out += QStringLiteral("kind = %1\nformat = %2\n").arg(quote(kKind)).arg(kFormat);
    out += QStringLiteral("app_version = %1\n").arg(quote(input.appVersion));
    out += QStringLiteral("created = %1\n")
               .arg(input.now.toOffsetFromUtc(input.now.offsetFromUtc()).toString(Qt::ISODate));
    out += QStringLiteral("game = %1\ntitle = %2\n").arg(quote(input.gameId), quote(input.title));
    if (!matched.isEmpty())
        out += QStringLiteral("matched = %1   # the game in the Ludusavi manifest\n").arg(quote(matched));
    out += QStringLiteral("files = %1\nbytes = %2\nsignature = %3\n")
               .arg(entries.size())
               .arg(bytes)
               .arg(quote(sig));
    for (qsizetype n = 0; n < sources.size(); ++n) {
        const Source &source = sources.at(n);
        out += QStringLiteral("\n[[path]]\nn = %1\nfrom = %2\ntype = %3\n")
                   .arg(n)
                   .arg(quote(tilde(source.path, input.env.home)),
                        quote(isFolder(QFileInfo(source.path)) ? QStringLiteral("folder") : QStringLiteral("file")));
        if (source.wholePrefix)
            out += QStringLiteral("prefix = true   # a Proton prefix without Proton's own files\n");
    }
    return out.toUtf8();
}

QString newestSignature(const QString &folder, const QString &gameId)
{
    const QStringList existing = backups(folder, gameId);
    if (existing.isEmpty())
        return {};
    QString error;
    const std::optional<QByteArray> bytes = Archive::readFirst(existing.first(), QStringLiteral("manifest.toml"), &error);
    if (!bytes)
        return {};
    try {
        const toml::table table = toml::parse(std::string_view(bytes->constData(), size_t(bytes->size())));
        if (const auto kind = table["kind"].value<std::string>(); !kind || QString::fromStdString(*kind) != kKind)
            return {};
        if (const auto sig = table["signature"].value<std::string>())
            return QString::fromStdString(*sig);
    } catch (const toml::parse_error &) {
    }
    return {};
}

bool writeArchive(const QString &path, const QByteArray &manifestText, const QList<Source> &sources,
                  QString *error)
{
    Archive::Writer writer;
    if (!writer.open(path)) {
        *error = writer.error();
        return false;
    }
    if (!writer.addData(QStringLiteral("manifest.toml"), manifestText)) {
        *error = writer.error();
        return false;
    }
    for (qsizetype n = 0; n < sources.size(); ++n) {
        const Source &source = sources.at(n);
        const QFileInfo info(source.path);
        const QString prefix = QStringLiteral("files/%1").arg(n);
        const bool ok = isFolder(info) ? writer.addTree(source.path, prefix, source.skip)
                                       : writer.addPath(source.path, prefix + u'/' + info.fileName());
        if (!ok) {
            *error = writer.error();
            return false;
        }
    }
    if (!writer.finish()) {
        *error = writer.error();
        return false;
    }
    return true;
}
}

QString archiveName(const QDateTime &when)
{
    return when.toString(QStringLiteral("yyyy-MM-dd-HHmmss")) + QStringLiteral(".tar.zst");
}

QStringList backups(const QString &folder, const QString &gameId)
{
    const QDir dir(folder + u'/' + gameId);
    QStringList names = dir.entryList({QStringLiteral("*.tar.zst")}, QDir::Files, QDir::Name);
    std::reverse(names.begin(), names.end());
    QStringList out;
    for (const QString &name : std::as_const(names))
        out.append(dir.filePath(name));
    return out;
}

QString backupName(const QString &archive)
{
    QFile file(archive + QStringLiteral(".name"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

QString setBackupName(const QString &archive, const QString &name)
{
    const QString path = archive + QStringLiteral(".name");
    const QString text = name.simplified();
    if (text.isEmpty())
        return !QFile::exists(path) || QFile::remove(path) ? QString() : QObject::tr("Can't remove %1").arg(path);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(text.toUtf8()) < 0)
        return QObject::tr("Can't write %1").arg(path);
    return {};
}

Result run(const Input &input)
{
    Result result;
    result.gameId = input.copies.isEmpty() ? input.gameId : input.copies.first().id;
    result.title = input.title;

    SavesIndex index;
    if (const std::optional<SavesIndex> loaded = SavesManifest::loadIndex(input.manifestDir))
        index = *loaded;
    const SavesFinder::Matcher matcher(index);
    const SavesFinder::Found found = SavesFinder::find(matcher, input.copies, input.env);

    QList<Source> sources;
    for (const QString &path : found.paths)
        sources.append({path, {}, false});
    if (sources.isEmpty() && !input.protonPrefix.isEmpty()) {
        if (const std::optional<ProtonSaves::Prefix> prefix = ProtonSaves::Prefix::read(input.protonPrefix))
            sources.append({prefix->path, ProtonSaves::skip(*prefix), true});
    }
    if (sources.isEmpty())
        return result;

    const QList<Entry> entries = collect(sources);
    if (entries.isEmpty())
        return result;
    for (const Entry &e : entries)
        result.bytes += e.size;
    result.files = int(entries.size());

    if (result.bytes > input.options.maxBytes) {
        result.status = Status::TooBig;
        return result;
    }

    const QString sig = signature(entries);
    if (newestSignature(input.options.folder, result.gameId) == sig) {
        result.status = Status::Unchanged;
        return result;
    }

    const QString folder = input.options.folder + u'/' + result.gameId;
    const QString path = folder + u'/' + archiveName(input.now);
    const QString partial = path + QStringLiteral(".partial");
    if (!QDir().mkpath(folder)) {
        result.status = Status::Failed;
        result.error = QObject::tr("Can't create %1").arg(folder);
        return result;
    }
    QString error;
    Input named = input;
    named.gameId = result.gameId;
    if (!writeArchive(partial, manifest(named, found.matched, sources, entries, result.bytes, sig), sources, &error)) {
        QFile::remove(partial);
        result.status = Status::Failed;
        result.error = error;
        return result;
    }
    if (::rename(QFile::encodeName(partial).constData(), QFile::encodeName(path).constData()) != 0) {
        QFile::remove(partial);
        result.status = Status::Failed;
        result.error = QObject::tr("Can't write %1").arg(path);
        return result;
    }
    result.status = Status::Written;
    result.archive = path;

    const Trash trash = input.trash ? input.trash : Trash(MovedAside::moveToTrash);
    const QStringList all = backups(input.options.folder, result.gameId);
    // A backup the user named is one they chose to keep, so it neither counts toward `keep` nor goes.
    int kept = 0;
    for (const QString &archive : all) {
        if (!backupName(archive).isEmpty() || ++kept <= qMax(input.options.keep, 1))
            continue;
        QString trashError;
        if (!trash(archive, &trashError))
            qCWarning(lcBackup).noquote() << QStringLiteral("Can't trash the old backup %1: %2").arg(archive, trashError);
    }
    return result;
}

Service::Service(Gather gather, Busy busy, std::function<int()> minSessionSeconds, QObject *parent)
    : QObject(parent), m_gather(std::move(gather)), m_busy(std::move(busy)), m_minSeconds(std::move(minSessionSeconds))
{
    qRegisterMetaType<SavesBackup::Result>();
}

Service::~Service()
{
    for (QFuture<Result> &future : m_futures)
        future.waitForFinished();
}

void Service::started(const QString &gameId)
{
    m_started[gameId].start();
}

void Service::ended(const QString &gameId)
{
    const auto it = m_started.find(gameId);
    if (it != m_started.end()) {
        const qint64 seconds = it->elapsed() / 1000;
        m_started.erase(it);
        if (seconds >= m_minSeconds())
            m_pending.insert(gameId);
    }
    flush();
}

void Service::flush()
{
    const QStringList ids = m_pending.values();
    for (const QString &id : ids) {
        if (m_busy && m_busy(id))
            continue;
        m_pending.remove(id);
        start(id);
    }
}

void Service::start(const QString &gameId)
{
    std::optional<Input> input = m_gather(gameId);
    if (!input)
        return;
    auto *watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher] {
        const Result result = watcher->result();
        watcher->deleteLater();
        --m_running;
        switch (result.status) {
        case Status::Failed:
            qCWarning(lcBackup).noquote() << QStringLiteral("Saves backup of %1 failed: %2").arg(result.gameId, result.error);
            emit message(tr("Couldn't back up %1's saves: %2").arg(result.title, result.error));
            break;
        case Status::TooBig:
            qCWarning(lcBackup).noquote() << QStringLiteral("Saves of %1 are %2 bytes; over the limit").arg(result.gameId).arg(result.bytes);
            emit message(tr("Skipped %1's saves backup: they come to %2 MB, over the limit")
                             .arg(result.title)
                             .arg(result.bytes >> 20));
            break;
        case Status::Written:
            qCInfo(lcBackup).noquote() << QStringLiteral("Backed up %1's saves (%2 files) to %3").arg(result.gameId).arg(result.files).arg(result.archive);
            break;
        case Status::Unchanged:
            qCInfo(lcBackup).noquote() << QStringLiteral("%1's saves haven't changed since the last backup").arg(result.gameId);
            break;
        case Status::Nothing:
            qCInfo(lcBackup).noquote() << QStringLiteral("No saves found for %1; nothing backed up").arg(result.gameId);
            break;
        }
        emit finished(result);
    });
    ++m_running;
    QFuture<Result> future = QtConcurrent::run([in = std::move(*input)] { return run(in); });
    m_futures.append(future);
    watcher->setFuture(future);
}
}
