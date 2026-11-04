#pragma once

#include "savesbackup.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQmlIntegration>

#include <functional>
#include <memory>
#include <optional>

class QQmlEngine;
class QJSEngine;

// See docs/code/saves.md.
class GameSaves : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap last READ last NOTIFY lastChanged)
    Q_PROPERTY(bool working READ working NOTIFY workingChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)

public:
    struct Setup
    {
        // The backup input for a game, whatever its automatic setting; nothing for a game that can't be backed up.
        std::function<std::optional<SavesBackup::Input>(const QString &gameId)> input;
        // Writes `saves_game` (empty clears it); returns an error or nothing.
        std::function<QString(const QString &gameId, const QString &name)> choose;
    };
    // Called once at startup; nothing is read until QML uses it.
    static void setup(const Setup &setup) { s_setup = setup; }
    static GameSaves *instance();
    static GameSaves *create(QQmlEngine *, QJSEngine *) { return instance(); }

    struct Loaded
    {
        QVariantMap state;
        std::shared_ptr<const SavesIndex> index;
    };
    // What a game's page shows; blocks (reads the index and the disk), so it runs on a worker thread.
    static Loaded read(const SavesBackup::Input &input, std::shared_ptr<const SavesIndex> index);

    explicit GameSaves(const Setup &setup, QObject *parent = nullptr);
    ~GameSaves() override;

    QVariantMap state() const { return m_state; }
    QVariantMap last() const { return m_last; }
    bool working() const { return m_working; }
    QString message() const { return m_message; }

    // load() for a game the editor just showed: a backup's message belongs to the game it was made for.
    Q_INVOKABLE void open(const QString &gameId);
    Q_INVOKABLE void load(const QString &gameId);
    // Only the newest backup (`requested`, `when`, `size`, `name`), for the page's chip: no index is read.
    Q_INVOKABLE void loadLast(const QString &gameId);
    // Reads `last` again for the game it was last read for; for a backup made while its page is open.
    void refreshLast();
    // Empty `name` clears it; returns an error or nothing. Reloads.
    Q_INVOKABLE QString rename(const QString &gameId, const QString &archive, const QString &name);
    // Moves one of the game's backups (and its name) to the trash; returns an error or nothing. Reloads.
    Q_INVOKABLE QString remove(const QString &gameId, const QString &archive);
    Q_INVOKABLE void backUpNow(const QString &gameId);
    Q_INVOKABLE QVariantList search(const QString &text) const;
    Q_INVOKABLE QString choose(const QString &gameId, const QString &name);

signals:
    void stateChanged();
    void lastChanged();
    void workingChanged();
    void messageChanged();

private:
    void setMessage(const QString &text);

    inline static Setup s_setup;
    Setup m_setup;
    std::shared_ptr<const SavesIndex> m_index;
    mutable QStringList m_folded;
    mutable const SavesIndex *m_foldedFor = nullptr;
    QVariantMap m_state;
    QVariantMap m_last;
    int m_lastToken = 0;
    QString m_message;
    QString m_openGame;
    bool m_working = false;
    int m_token = 0;
};
