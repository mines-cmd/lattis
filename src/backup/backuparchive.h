#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QLoggingCategory>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <optional>

Q_DECLARE_LOGGING_CATEGORY(lcBackup)

struct archive;

// See docs/code/backup.md.
namespace Archive {
struct Control
{
    std::function<void(qint64 bytes)> progress;
    const std::atomic_bool *cancel = nullptr;
};

using Skip = std::function<bool(const QString &relativePath)>;

class Writer
{
public:
    Writer() = default;
    ~Writer();
    Writer(const Writer &) = delete;
    Writer &operator=(const Writer &) = delete;

    void setControl(const Control &control) { m_control = control; }

    bool open(const QString &path, int level = 3);
    bool addData(const QString &name, const QByteArray &data);
    bool addTree(const QString &folder, const QString &prefix, const Skip &skip = {});
    bool addPath(const QString &source, const QString &name);
    bool finish();
    void abort();

    QString error() const { return m_error; }
    qint64 bytes() const { return m_bytes; }

private:
    bool addEntry(const QByteArray &source, const QByteArray &name, bool followFolderLink = false);
    bool walk(const QByteArray &folder, const QByteArray &name, const QString &relative, const Skip &skip);
    bool writeContents(const QByteArray &source, qint64 size);
    bool fail(const QString &error);
    bool cancelled();

    struct archive *m_archive = nullptr;
    QString m_path;
    QString m_error;
    qint64 m_bytes = 0;
    Control m_control;
};

std::optional<QByteArray> readFirst(const QString &path, const QString &name, QString *error);

struct Destination
{
    QString prefix;
    QString folder;
    // Its links may lead anywhere, kept as they were (the library's: the user's own). Nothing
    // is ever written through a link either way.
    bool anyLinks = false;
};

enum class Seen {
    Nothing,
    Folder,  // the part's own folder entry (a folder added with addTree())
    Entries, // entries under it, without its own (a file added as <prefix>/<its name>)
};

bool extract(const QString &path, const QList<Destination> &destinations, const QStringList &allowed,
             QString *error, const Control &control = {}, QHash<QString, Seen> *seen = nullptr);

void removeTree(const QString &folder);
}
