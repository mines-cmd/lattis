#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>

// See docs/code/library.md.
class LibraryUndo : public QObject
{
    Q_OBJECT

public:
    explicit LibraryUndo(QObject *parent = nullptr);
    ~LibraryUndo() override;

    // An empty undo folder turns Undo off: nothing is kept and commit() returns 0.
    void setFolders(const QString &dataDir, const QString &undoDir);
    bool enabled() const { return !m_undoDir.isEmpty(); }
    void setLifetime(int ms) { m_lifetime = ms; }

    void start(const QString &subject = {});
    bool pending() const { return m_recording; }
    QString subject() const { return m_subject; }
    // `relative` (a file or a folder under the data dir) as it is now. Only the first call
    // for a path counts, so a recording that spans several steps keeps the original.
    void keepCopy(const QString &relative);
    void keepMove(const QString &relative);
    // Makes the pending entry the current one, replacing the older. 0 when Undo is off or
    // nothing it kept differs from what's there now.
    quint64 commit();
    void discard();

    bool valid(quint64 token) const { return token != 0 && token == m_token; }
    QStringList restore(quint64 token, QString *error);

    void clearLeftovers();
    void clear();

signals:
    void expired(quint64 token);

private:
    struct Item
    {
        QString relative;
        bool existed = false;
        bool moved = false;
    };
    void keep(const QString &relative, bool move);
    QString folderOf(quint64 n) const;
    void dropCurrent();

    QString m_dataDir;
    QString m_undoDir;
    int m_lifetime = 60000;
    quint64 m_counter = 0;
    quint64 m_token = 0;
    QList<Item> m_items;
    QString m_subject;

    bool m_recording = false;
    quint64 m_pendingNumber = 0;
    QList<Item> m_pending;
    QTimer m_timer;
};
