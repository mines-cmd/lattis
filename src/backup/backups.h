#pragma once

#include "backup.h"
#include "backupplan.h"
#include "movedaside.h"
#include "restore.h"

#include <QFutureWatcher>
#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <QtQmlIntegration>

#include <functional>
#include <optional>

class QQmlEngine;
class QJSEngine;

// See docs/code/backup.md.
class Backups : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString kind READ kind NOTIFY runningChanged)
    Q_PROPERTY(qint64 bytes READ bytes NOTIFY progressChanged)
    Q_PROPERTY(qint64 total READ total NOTIFY runningChanged)
    Q_PROPERTY(QString part READ part NOTIFY progressChanged)
    Q_PROPERTY(QVariantMap libraryPlan READ libraryPlan NOTIFY libraryPlanChanged)
    Q_PROPERTY(QVariantMap savesPlan READ savesPlan NOTIFY savesPlanChanged)
    Q_PROPERTY(QVariantMap choices READ choices NOTIFY choicesChanged)
    Q_PROPERTY(QVariantMap last READ last NOTIFY lastChanged)
    Q_PROPERTY(QVariantMap notice READ notice NOTIFY importsChanged)
    Q_PROPERTY(QVariantList movedAside READ movedAside NOTIFY importsChanged)
    Q_PROPERTY(QVariantMap importPlan READ importPlan NOTIFY importPlanChanged)
    Q_PROPERTY(bool importing READ importing NOTIFY importingChanged)
    Q_PROPERTY(qint64 importBytes READ importBytes NOTIFY importProgressChanged)
    Q_PROPERTY(qint64 importTotal READ importTotal NOTIFY importingChanged)

public:
    struct Setup
    {
        QString dataDir;
        QString stateDir;
        QString dataDirKey;
        std::function<BackupPlan::Sources()> sources;
        std::function<QStringList()> runningGames;
        // A library Import: the library stops scanning and writing while it runs (true), and
        // goes on after a failure (false).
        std::function<void(bool)> holdLibrary;
        std::function<void()> restart;
    };
    // Called once at startup; nothing is made until QML uses it.
    static void setup(const Setup &setup) { s_setup = setup; }
    static Backups *instance();
    static Backups *create(QQmlEngine *, QJSEngine *) { return instance(); }

    explicit Backups(const Setup &setup, QObject *parent = nullptr);
    ~Backups() override;

    bool running() const { return m_job.running(); }
    QString kind() const { return m_job.kind(); }
    qint64 bytes() const { return m_job.bytes(); }
    qint64 total() const { return m_job.total(); }
    QString part() const { return m_job.part(); }
    QVariantMap libraryPlan() const { return m_libraryMap; }
    QVariantMap savesPlan() const { return m_savesMap; }
    QVariantMap choices() const;
    QVariantMap last() const;
    QVariantMap notice() const;
    QVariantList movedAside() const;
    QVariantMap importPlan() const { return m_importMap; }
    bool importing() const { return m_restore.running(); }
    qint64 importBytes() const { return m_restore.bytes(); }
    qint64 importTotal() const { return m_restore.total(); }

    Q_INVOKABLE QString openImport(const QUrl &file);
    Q_INVOKABLE void checkImportPart(int index, bool checked);
    Q_INVOKABLE QString startImport();
    Q_INVOKABLE void cancelImport() { m_restore.cancel(); }
    Q_INVOKABLE QUrl importFolder() const;
    Q_INVOKABLE QVariantMap screenshotImport() const;

    QString startImport(const Restore::Plan &plan);
    Q_INVOKABLE void dismissNotice();
    Q_INVOKABLE QString deleteMovedAside(int index);
    Q_INVOKABLE QString deleteNoticed();

    Q_INVOKABLE void planLibrary();
    Q_INVOKABLE void planSaves();
    Q_INVOKABLE QString setChoice(const QString &key, bool value);
    Q_INVOKABLE QUrl defaultFile(const QString &kind) const;
    Q_INVOKABLE QString start(const QString &kind, const QUrl &file);
    Q_INVOKABLE void cancel() { m_job.cancel(); }
    Q_INVOKABLE QString gamesRunning() const { return m_job.gamesRunning(); }
    Q_INVOKABLE QString sizeText(qint64 bytes) const;
    Q_INVOKABLE QUrl screenshotTarget() const;

signals:
    void runningChanged();
    void progressChanged();
    void libraryPlanChanged();
    void savesPlanChanged();
    void choicesChanged();
    void lastChanged();
    void importsChanged();
    void importPlanChanged();
    void importingChanged();
    void importProgressChanged();
    void importFinished(const QString &kind, const QString &error);
    void finished(const QString &kind, const QString &path, const QString &error);

private:
    BackupPlan::Settings readSettings() const;
    void updateLibraryMap();
    void updateSavesMap();
    void updateImportMap();
    void onImportFinished(const QString &error);
    void loadImports();
    void saveImports();
    int noticed() const;
    QString removeImport(int index);

    Setup m_setup;
    BackupJob m_job;
    BackupPlan::LibraryPlan m_library;
    BackupPlan::SavesPlan m_saves;
    QVariantMap m_libraryMap;
    QVariantMap m_savesMap;
    QFutureWatcher<BackupPlan::LibraryPlan> m_measureLibrary;
    QFutureWatcher<BackupPlan::SavesPlan> m_measureSaves;
    RestoreJob m_restore;
    QString m_restoreKind;
    QString m_restoreArchive;
    QString m_importsFile;
    QList<MovedAside::Import> m_imports;
    QList<qint64> m_importBytes; // per Import, -1 until measured
    QFutureWatcher<QList<qint64>> m_measureImports;
    std::optional<Restore::Plan> m_import;
    QList<QList<qint64>> m_importNow;      // per part and target: what's there now, -1 until measured
    QVariantMap m_importMap;
    QFutureWatcher<QList<QList<qint64>>> m_measureImport;

    static inline Setup s_setup;
    static inline Backups *s_instance = nullptr;
};
