#pragma once

#include "savesmanifest.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QtQmlIntegration>

#include <functional>

class QQmlEngine;
class QJSEngine;

// See docs/code/saves.md.
class SavesSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool updating READ updating NOTIFY updatingChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(bool downloaded READ downloaded NOTIFY stateChanged)
    Q_PROPERTY(QString folder READ folder NOTIFY settingsChanged)
    Q_PROPERTY(QString defaultFolder READ defaultFolder CONSTANT)
    Q_PROPERTY(int keep READ keep NOTIFY settingsChanged)

public:
    struct Setup
    {
        QString dataDir;
        QString manifestDir;
    };
    // Called once at startup; nothing is read or fetched until QML uses it.
    static void setup(const Setup &setup) { s_setup = setup; }
    static SavesSettings *instance();
    static SavesSettings *create(QQmlEngine *, QJSEngine *) { return instance(); }
    // Ten seconds after the first frame's own timer: fetches only when the feature is on
    // (`anyOn`) and the last check is more than a day old.
    static void startDailyCheck(std::function<bool()> anyOn);

    explicit SavesSettings(const Setup &setup, QObject *parent = nullptr);
    ~SavesSettings() override;

    bool updating() const { return m_watcher.isRunning(); }
    QString status() const { return m_status; }
    bool downloaded() const { return m_fetched.isValid(); }
    QString folder() const { return m_folder; }
    QString defaultFolder() const;
    int keep() const { return m_keep; }

    // The first time the feature is turned on: fetches the manifest if there is none.
    Q_INVOKABLE void ensureManifest();
    Q_INVOKABLE void update();
    // An empty folder is the default one. Returns an error, or nothing.
    Q_INVOKABLE QString setFolder(const QString &folder);
    Q_INVOKABLE QString setKeep(int keep);
    Q_INVOKABLE void refresh();

signals:
    void updatingChanged();
    void stateChanged();
    void settingsChanged();

private:
    void run(bool force);
    void readState();

    inline static Setup s_setup;
    Setup m_setup;
    QFutureWatcher<SavesManifest::UpdateResult> m_watcher;
    QDateTime m_fetched;
    QString m_status;
    QString m_error;
    QString m_folder;
    int m_keep = 5;
};
