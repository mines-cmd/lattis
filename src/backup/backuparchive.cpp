#include "backuparchive.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QThread>

#include <archive.h>
#include <archive_entry.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <memory>
#include <vector>

Q_LOGGING_CATEGORY(lcBackup, "launcher.backup")

namespace Archive {
namespace {
constexpr qint64 BlockSize = 1024 * 1024;
constexpr qint64 ManifestLimit = 1024 * 1024;

QString tr(const char *text)
{
    return QCoreApplication::translate("Archive", text);
}

QString errorOf(struct archive *a)
{
    const char *text = archive_error_string(a);
    return text ? QString::fromLocal8Bit(text) : tr("Unknown error");
}

QString display(const QByteArray &path)
{
    return QFile::decodeName(path);
}

struct ReadCloser
{
    void operator()(struct archive *a) const { archive_read_free(a); }
};
using ReadArchive = std::unique_ptr<struct archive, ReadCloser>;

ReadArchive openRead(const QString &path, QString *error)
{
    ReadArchive a(archive_read_new());
    archive_read_support_filter_zstd(a.get());
    archive_read_support_format_tar(a.get());
    if (archive_read_open_filename(a.get(), QFile::encodeName(path).constData(), 64 * 1024) != ARCHIVE_OK) {
        // EILSEQ: libarchive's ARCHIVE_ERRNO_FILE_FORMAT on Linux, a file it doesn't recognize.
        *error = archive_errno(a.get()) == EILSEQ
                     ? tr("%1 isn't a backup: %2").arg(path, errorOf(a.get()))
                     : tr("Can't read %1: %2").arg(path, errorOf(a.get()));
        return {};
    }
    return a;
}

bool linkStaysInside(const QByteArray &relative, const QByteArray &target)
{
    if (target.isEmpty() || target.startsWith('/'))
        return false;
    QList<QByteArray> parts = relative.split('/');
    parts.removeLast();
    for (const QByteArray &part : target.split('/')) {
        if (part.isEmpty() || part == ".")
            continue;
        if (part == "..") {
            if (parts.isEmpty())
                return false;
            parts.removeLast();
        } else {
            parts.append(part);
        }
    }
    return true;
}
}

void removeTree(const QString &folder)
{
    QDirIterator it(folder, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo info = it.nextFileInfo();
        if (!info.isSymLink())
            QFile::setPermissions(info.filePath(), info.permissions() | QFile::WriteOwner | QFile::ExeOwner);
    }
    QFile::setPermissions(folder, QFileInfo(folder).permissions() | QFile::WriteOwner | QFile::ExeOwner);
    QDir(folder).removeRecursively();
}

Writer::~Writer()
{
    abort();
}

bool Writer::open(const QString &path, int level)
{
    abort();
    m_error.clear();
    m_bytes = 0;
    m_archive = archive_write_new();
    archive_write_add_filter_zstd(m_archive);
    archive_write_set_filter_option(m_archive, "zstd", "compression-level", QByteArray::number(level).constData());
    // Only a warning where libarchive or zstd can't use threads.
    archive_write_set_filter_option(m_archive, "zstd", "threads",
                                    QByteArray::number(QThread::idealThreadCount()).constData());
    archive_write_set_format_pax_restricted(m_archive);
    if (archive_write_open_filename(m_archive, QFile::encodeName(path).constData()) != ARCHIVE_OK) {
        m_error = tr("Can't write %1: %2").arg(path, errorOf(m_archive));
        archive_write_free(m_archive);
        m_archive = nullptr;
        return false;
    }
    m_path = path;
    return true;
}

bool Writer::addData(const QString &name, const QByteArray &data)
{
    if (!m_archive)
        return false;
    std::unique_ptr<archive_entry, decltype(&archive_entry_free)> entry(archive_entry_new(), archive_entry_free);
    archive_entry_set_pathname(entry.get(), name.toUtf8().constData());
    archive_entry_set_filetype(entry.get(), AE_IFREG);
    archive_entry_set_perm(entry.get(), 0644);
    archive_entry_set_size(entry.get(), data.size());
    archive_entry_set_mtime(entry.get(), QDateTime::currentSecsSinceEpoch(), 0);
    if (archive_write_header(m_archive, entry.get()) < ARCHIVE_WARN)
        return fail(tr("Can't write %1: %2").arg(name, errorOf(m_archive)));
    if (archive_write_data(m_archive, data.constData(), data.size()) != data.size())
        return fail(tr("Can't write %1: %2").arg(name, errorOf(m_archive)));
    m_bytes += data.size();
    return true;
}

bool Writer::addTree(const QString &folder, const QString &prefix, const Skip &skip)
{
    if (!m_archive)
        return false;
    const QByteArray path = QFile::encodeName(folder);
    struct stat st;
    // The folder itself may be a link (a save folder moved elsewhere); what it holds isn't
    // followed.
    if (::stat(path.constData(), &st) != 0 || !S_ISDIR(st.st_mode))
        return fail(tr("%1 isn't a folder").arg(folder));
    return walk(path, prefix.toUtf8(), QString(), skip);
}

bool Writer::addPath(const QString &source, const QString &name)
{
    if (!m_archive)
        return false;
    return addEntry(QFile::encodeName(source), name.toUtf8());
}

bool Writer::walk(const QByteArray &folder, const QByteArray &name, const QString &relative, const Skip &skip)
{
    if (!addEntry(folder, name, relative.isEmpty()))
        return false;
    DIR *dir = ::opendir(folder.constData());
    if (!dir)
        return fail(tr("Can't read %1").arg(display(folder)));
    std::vector<QByteArray> children;
    while (const dirent *child = ::readdir(dir)) {
        const QByteArray childName(child->d_name);
        if (childName != "." && childName != "..")
            children.push_back(childName);
    }
    ::closedir(dir);
    std::sort(children.begin(), children.end());

    for (const QByteArray &child : children) {
        const QString childRelative = relative.isEmpty() ? display(child) : relative + u'/' + display(child);
        if (skip && skip(childRelative))
            continue;
        const QByteArray childPath = folder + '/' + child;
        const QByteArray childName = name + '/' + child;
        struct stat st;
        if (::lstat(childPath.constData(), &st) != 0)
            return fail(tr("Can't read %1").arg(display(childPath)));
        if (S_ISDIR(st.st_mode) ? !walk(childPath, childName, childRelative, skip) : !addEntry(childPath, childName))
            return false;
    }
    return true;
}

bool Writer::addEntry(const QByteArray &source, const QByteArray &name, bool followFolderLink)
{
    if (cancelled())
        return false;
    struct stat st;
    if (::lstat(source.constData(), &st) != 0)
        return fail(tr("Can't read %1").arg(display(source)));
    if (S_ISLNK(st.st_mode) && followFolderLink) {
        struct stat target;
        if (::stat(source.constData(), &target) == 0 && S_ISDIR(target.st_mode))
            st = target;
    }

    std::unique_ptr<archive_entry, decltype(&archive_entry_free)> entry(archive_entry_new(), archive_entry_free);
    archive_entry_set_pathname(entry.get(), name.constData());
    archive_entry_set_perm(entry.get(), st.st_mode & 07777);
    archive_entry_set_mtime(entry.get(), st.st_mtim.tv_sec, st.st_mtim.tv_nsec);
    if (S_ISREG(st.st_mode)) {
        archive_entry_set_filetype(entry.get(), AE_IFREG);
        archive_entry_set_size(entry.get(), st.st_size);
    } else if (S_ISDIR(st.st_mode)) {
        archive_entry_set_filetype(entry.get(), AE_IFDIR);
    } else if (S_ISLNK(st.st_mode)) {
        QByteArray target(qMax<qint64>(st.st_size, 256) + 1, Qt::Uninitialized);
        const ssize_t length = ::readlink(source.constData(), target.data(), target.size());
        if (length < 0 || length >= target.size())
            return fail(tr("Can't read the link %1").arg(display(source)));
        target.truncate(length);
        archive_entry_set_filetype(entry.get(), AE_IFLNK);
        archive_entry_set_symlink(entry.get(), target.constData());
    } else {
        qCInfo(lcBackup) << "Left out" << display(source) << "(not a file, folder or link)";
        return true;
    }

    const int result = archive_write_header(m_archive, entry.get());
    if (result < ARCHIVE_WARN)
        return fail(tr("Can't write %1: %2").arg(display(source), errorOf(m_archive)));
    if (result == ARCHIVE_WARN)
        qCWarning(lcBackup) << display(source) << errorOf(m_archive);
    if (S_ISREG(st.st_mode) && st.st_size > 0 && !writeContents(source, st.st_size))
        return false;
    return true;
}

bool Writer::writeContents(const QByteArray &source, qint64 size)
{
    const int fd = ::open(source.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0)
        return fail(tr("Can't read %1").arg(display(source)));
    QByteArray buffer(qMin(size, BlockSize), Qt::Uninitialized);
    qint64 left = size;
    while (left > 0) {
        if (cancelled()) {
            ::close(fd);
            return false;
        }
        const ssize_t n = ::read(fd, buffer.data(), qMin<qint64>(left, buffer.size()));
        if (n < 0) {
            ::close(fd);
            return fail(tr("Can't read %1").arg(display(source)));
        }
        if (n == 0) {
            // Shrunk since it was listed; the entry is padded with zeros.
            qCWarning(lcBackup) << display(source) << "got shorter while it was being read";
            break;
        }
        if (archive_write_data(m_archive, buffer.constData(), n) != n) {
            ::close(fd);
            return fail(tr("Can't write %1: %2").arg(display(source), errorOf(m_archive)));
        }
        left -= n;
        m_bytes += n;
        if (m_control.progress)
            m_control.progress(m_bytes);
    }
    ::close(fd);
    return true;
}

bool Writer::finish()
{
    if (!m_archive)
        return false;
    const int result = archive_write_close(m_archive);
    if (result != ARCHIVE_OK)
        return fail(tr("Can't write %1: %2").arg(m_path, errorOf(m_archive)));
    archive_write_free(m_archive);
    m_archive = nullptr;
    m_path.clear();
    return true;
}

void Writer::abort()
{
    if (!m_archive)
        return;
    archive_write_free(m_archive);
    m_archive = nullptr;
    QFile::remove(m_path);
    m_path.clear();
}

bool Writer::fail(const QString &error)
{
    m_error = error;
    abort();
    return false;
}

bool Writer::cancelled()
{
    if (!m_control.cancel || !m_control.cancel->load())
        return false;
    fail(tr("Cancelled"));
    return true;
}

std::optional<QByteArray> readFirst(const QString &path, const QString &name, QString *error)
{
    ReadArchive a = openRead(path, error);
    if (!a)
        return std::nullopt;
    archive_entry *entry = nullptr;
    if (archive_read_next_header(a.get(), &entry) < ARCHIVE_WARN) {
        *error = tr("%1 isn't a backup: %2").arg(path, errorOf(a.get()));
        return std::nullopt;
    }
    if (QByteArray(archive_entry_pathname(entry)) != name.toUtf8() || archive_entry_filetype(entry) != AE_IFREG
        || archive_entry_size(entry) > ManifestLimit) {
        *error = tr("%1 isn't a backup: it doesn't start with %2").arg(path, name);
        return std::nullopt;
    }
    QByteArray data(archive_entry_size(entry), Qt::Uninitialized);
    qint64 done = 0;
    while (done < data.size()) {
        const la_ssize_t n = archive_read_data(a.get(), data.data() + done, data.size() - done);
        if (n <= 0) {
            *error = tr("%1 is damaged: %2").arg(path, n < 0 ? errorOf(a.get()) : tr("it ends too soon"));
            return std::nullopt;
        }
        done += n;
    }
    return data;
}

bool extract(const QString &path, const QList<Destination> &destinations, const QStringList &allowed,
             QString *error, const Control &control, QHash<QString, Seen> *seen)
{
    if (seen) {
        seen->clear();
        for (const Destination &d : destinations)
            seen->insert(d.prefix, Seen::Nothing);
    }
    QStringList created;
    const auto failed = [&](const QString &why) {
        *error = why;
        for (const QString &folder : std::as_const(created))
            removeTree(folder);
        return false;
    };

    ReadArchive in = openRead(path, error);
    if (!in)
        return false;

    // Each prefix as bytes, with its folder (canonical, so the check for links on the way
    // there has none to trip over) or none when it's passed over.
    struct Part
    {
        QByteArray prefix;
        QByteArray folder;
        bool anyLinks = false;
    };
    std::vector<Part> parts;
    for (const QString &prefix : allowed)
        parts.push_back({prefix.toUtf8(), {}});
    for (const Destination &d : destinations) {
        if (QFileInfo(d.folder).exists() || QFileInfo(d.folder).isSymLink())
            return failed(tr("%1 exists already").arg(d.folder));
        if (!QDir().mkpath(d.folder))
            return failed(tr("Can't create %1").arg(d.folder));
        created.append(d.folder);
        const QByteArray prefix = d.prefix.toUtf8();
        auto it = std::find_if(parts.begin(), parts.end(), [&](const Part &p) { return p.prefix == prefix; });
        if (it == parts.end())
            it = parts.insert(parts.end(), {prefix, {}});
        it->folder = QFile::encodeName(QFileInfo(d.folder).canonicalFilePath());
        it->anyLinks = d.anyLinks;
    }

    struct WriteCloser
    {
        void operator()(struct archive *a) const { archive_write_free(a); }
    };
    std::unique_ptr<struct archive, WriteCloser> disk(archive_write_disk_new());
    archive_write_disk_set_options(disk.get(), ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM
                                                   | ARCHIVE_EXTRACT_SECURE_SYMLINKS | ARCHIVE_EXTRACT_SPARSE);

    qint64 bytes = 0;
    archive_entry *entry = nullptr;
    while (true) {
        const int result = archive_read_next_header(in.get(), &entry);
        if (result == ARCHIVE_EOF)
            break;
        if (result < ARCHIVE_WARN)
            return failed(tr("%1 is damaged: %2").arg(path, errorOf(in.get())));

        QByteArray name(archive_entry_pathname(entry));
        while (name.endsWith('/'))
            name.chop(1);
        const QString shown = display(name);
        if (name.startsWith('/'))
            return failed(tr("%1 holds an absolute path, %2").arg(path, shown));
        const QList<QByteArray> components = name.split('/');
        if (name.isEmpty() || std::any_of(components.begin(), components.end(), [](const QByteArray &c) {
                return c.isEmpty() || c == "." || c == "..";
            }))
            return failed(tr("%1 holds a path that leads elsewhere, %2").arg(path, shown));

        const Part *part = nullptr;
        for (const Part &p : parts) {
            if ((name == p.prefix || name.startsWith(p.prefix + '/'))
                && (!part || p.prefix.size() > part->prefix.size()))
                part = &p;
        }
        if (!part)
            return failed(tr("%1 holds %2, which isn't part of a backup").arg(path, shown));
        if (seen && !part->folder.isEmpty()) {
            Seen &s = (*seen)[QString::fromUtf8(part->prefix)];
            if (name == part->prefix && archive_entry_filetype(entry) == AE_IFDIR)
                s = Seen::Folder;
            else if (s == Seen::Nothing && name != part->prefix)
                s = Seen::Entries;
        }
        if (part->folder.isEmpty() || name == part->prefix) {
            // Another part, or the part's own folder, which exists already.
            archive_read_data_skip(in.get());
            continue;
        }

        if (archive_entry_hardlink(entry))
            return failed(tr("%1 holds a hard link, %2").arg(path, shown));
        const QByteArray relative = name.mid(part->prefix.size() + 1);
        const auto type = archive_entry_filetype(entry);
        if (type == AE_IFLNK) {
            if (!part->anyLinks && !linkStaysInside(relative, archive_entry_symlink(entry)))
                return failed(tr("%1 holds a link pointing outside its part, %2").arg(path, shown));
        } else if (type != AE_IFREG && type != AE_IFDIR) {
            return failed(tr("%1 holds %2, which isn't a file, folder or link").arg(path, shown));
        }

        archive_entry_set_pathname(entry, (part->folder + '/' + relative).constData());
        const int written = archive_write_header(disk.get(), entry);
        if (written < ARCHIVE_WARN)
            return failed(tr("Can't restore %1: %2").arg(shown, errorOf(disk.get())));
        if (written == ARCHIVE_WARN)
            qCWarning(lcBackup) << shown << errorOf(disk.get());
        if (type == AE_IFREG) {
            const void *block = nullptr;
            size_t size = 0;
            la_int64_t offset = 0;
            while (true) {
                if (control.cancel && control.cancel->load())
                    return failed(tr("Cancelled"));
                const int r = archive_read_data_block(in.get(), &block, &size, &offset);
                if (r == ARCHIVE_EOF)
                    break;
                if (r < ARCHIVE_WARN)
                    return failed(tr("%1 is damaged: %2").arg(path, errorOf(in.get())));
                if (archive_write_data_block(disk.get(), block, size, offset) < ARCHIVE_WARN)
                    return failed(tr("Can't restore %1: %2").arg(shown, errorOf(disk.get())));
                bytes += qint64(size);
                if (control.progress)
                    control.progress(bytes);
            }
        }
        if (archive_write_finish_entry(disk.get()) < ARCHIVE_WARN)
            return failed(tr("Can't restore %1: %2").arg(shown, errorOf(disk.get())));
    }
    // Sets the folders' times and read-only permissions, deferred until their contents are in.
    if (archive_write_close(disk.get()) != ARCHIVE_OK)
        return failed(tr("Can't restore from %1: %2").arg(path, errorOf(disk.get())));
    return true;
}
}
