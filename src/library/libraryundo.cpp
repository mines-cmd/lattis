#include "libraryundo.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

namespace {
bool copyRecursive(const QString &from, const QString &to)
{
    if (QFileInfo(from).isDir()) {
        if (!QDir().mkpath(to))
            return false;
        const QStringList names = QDir(from).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
        for (const QString &name : names) {
            if (!copyRecursive(from + u'/' + name, to + u'/' + name))
                return false;
        }
        return true;
    }
    QDir().mkpath(QFileInfo(to).absolutePath());
    QFile::remove(to);
    return QFile::copy(from, to);
}

bool removeAny(const QString &path)
{
    if (QFileInfo(path).isDir() && !QFileInfo(path).isSymLink())
        return QDir(path).removeRecursively();
    return !QFileInfo::exists(path) || QFile::remove(path);
}

bool sameContent(const QString &a, const QString &b)
{
    const QFileInfo ia(a);
    const QFileInfo ib(b);
    if (ia.isDir() != ib.isDir())
        return false;
    if (!ia.isDir()) {
        QFile fa(a);
        QFile fb(b);
        return fa.open(QIODevice::ReadOnly) && fb.open(QIODevice::ReadOnly) && fa.readAll() == fb.readAll();
    }
    const QStringList na = QDir(a).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
    if (na != QDir(b).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name))
        return false;
    return std::all_of(na.cbegin(), na.cend(), [&](const QString &n) { return sameContent(a + u'/' + n, b + u'/' + n); });
}
}

LibraryUndo::LibraryUndo(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        const quint64 token = m_token;
        dropCurrent();
        emit expired(token);
    });
}

LibraryUndo::~LibraryUndo()
{
    clear();
}

void LibraryUndo::setFolders(const QString &dataDir, const QString &undoDir)
{
    m_dataDir = dataDir;
    m_undoDir = undoDir;
}

QString LibraryUndo::folderOf(quint64 n) const
{
    return m_undoDir + u'/' + QString::number(n);
}

void LibraryUndo::start(const QString &subject)
{
    discard();
    if (!enabled())
        return;
    m_recording = true;
    m_subject = subject;
    m_pendingNumber = ++m_counter;
    QDir().mkpath(folderOf(m_pendingNumber));
}

void LibraryUndo::keepCopy(const QString &relative)
{
    keep(relative, false);
}

void LibraryUndo::keepMove(const QString &relative)
{
    keep(relative, true);
}

void LibraryUndo::keep(const QString &relative, bool move)
{
    if (!m_recording)
        return;
    const bool seen = std::any_of(m_pending.cbegin(), m_pending.cend(), [&](const Item &i) { return i.relative == relative; });
    if (seen)
        return;
    const QString source = m_dataDir + u'/' + relative;
    const QString saved = folderOf(m_pendingNumber) + QStringLiteral("/data/") + relative;
    Item item;
    item.relative = relative;
    item.existed = QFileInfo::exists(source);
    if (item.existed) {
        QDir().mkpath(QFileInfo(saved).absolutePath());
        // Another drive than the state dir's can't rename, so it's copied and then removed.
        bool kept = move && QFile::rename(source, saved);
        item.moved = kept;
        if (!kept) {
            kept = copyRecursive(source, saved);
            if (kept && move)
                item.moved = kept = removeAny(source);
        }
        if (!kept) {
            // The entry would put back half of it, so there is none.
            discard();
            return;
        }
    }
    m_pending.append(item);
}

quint64 LibraryUndo::commit()
{
    if (!m_recording)
        return 0;
    const bool changed = std::any_of(m_pending.cbegin(), m_pending.cend(), [&](const Item &i) {
        const QString now = m_dataDir + u'/' + i.relative;
        if (i.moved || i.existed != QFileInfo::exists(now))
            return true;
        return i.existed && !sameContent(now, folderOf(m_pendingNumber) + QStringLiteral("/data/") + i.relative);
    });
    if (!changed) {
        discard();
        return 0;
    }
    dropCurrent();
    m_token = m_pendingNumber;
    m_items = m_pending;
    m_pending.clear();
    m_recording = false;
    m_subject.clear();
    m_timer.start(m_lifetime);
    return m_token;
}

void LibraryUndo::discard()
{
    if (!m_recording)
        return;
    for (const Item &i : std::as_const(m_pending)) {
        if (!i.moved)
            continue;
        const QString target = m_dataDir + u'/' + i.relative;
        if (!QFileInfo::exists(target)) {
            QDir().mkpath(QFileInfo(target).absolutePath());
            QFile::rename(folderOf(m_pendingNumber) + QStringLiteral("/data/") + i.relative, target);
        }
    }
    QDir(folderOf(m_pendingNumber)).removeRecursively();
    m_pending.clear();
    m_recording = false;
    m_subject.clear();
}

void LibraryUndo::dropCurrent()
{
    m_timer.stop();
    if (m_token != 0)
        QDir(folderOf(m_token)).removeRecursively();
    m_token = 0;
    m_items.clear();
}

QStringList LibraryUndo::restore(quint64 token, QString *error)
{
    if (!valid(token)) {
        *error = tr("Nothing to undo");
        return {};
    }
    QStringList put;
    QStringList failed;
    for (const Item &i : std::as_const(m_items)) {
        const QString target = m_dataDir + u'/' + i.relative;
        const QString saved = folderOf(m_token) + QStringLiteral("/data/") + i.relative;
        if (!removeAny(target)) {
            failed.append(i.relative);
            continue;
        }
        if (i.existed) {
            QDir().mkpath(QFileInfo(target).absolutePath());
            if (!QFile::rename(saved, target) && !copyRecursive(saved, target)) {
                failed.append(i.relative);
                continue;
            }
        }
        put.append(i.relative);
    }
    dropCurrent();
    if (!failed.isEmpty())
        *error = tr("Could not put back %1").arg(failed.join(QLatin1String(", ")));
    return put;
}

void LibraryUndo::clearLeftovers()
{
    if (enabled() && !m_recording && m_token == 0)
        QDir(m_undoDir).removeRecursively();
}

void LibraryUndo::clear()
{
    discard();
    dropCurrent();
    if (enabled())
        QDir(m_undoDir).removeRecursively();
}
