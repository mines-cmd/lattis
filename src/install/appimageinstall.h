#pragma once

#include "releasesource.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFutureWatcher>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <atomic>

class QNetworkAccessManager;
class QNetworkReply;

// See docs/code/install.md.

namespace AppImageInstall {
QString targetFolder(const QStringList &applicationsDirs, const QString &home);
bool isArchive(const QString &fileName);

struct Extracted
{
    QString name;
    QString error;
};
Extracted extractOne(const QString &archivePath, const QString &inside, const QString &folder,
                     const std::atomic_bool *cancel);
}

class AppImageDownload : public QObject
{
    Q_OBJECT

public:
    enum class Stage { Downloading, Extracting, Installing };

    struct Result
    {
        bool ok = false;
        bool cancelled = false;
        QString path;
        QString error;
        QString sha256;
        // The file that had the target's name, moved aside to be trashed; empty when nothing was replaced.
        QString replaced;
    };

    AppImageDownload(QNetworkAccessManager *network, const ReleaseAsset &asset, const QString &inside,
                     const QString &folder, QObject *parent = nullptr);
    ~AppImageDownload() override;

    // http: and file: URLs are allowed too (tests and the install fixtures).
    void setRelaxed(bool relaxed) { m_relaxed = relaxed; }
    // An update: a file with the target's name is moved aside instead of refusing the install.
    void setReplace(bool replace) { m_replace = replace; }
    // Reads this many bytes every tick instead of all that has arrived (so a screenshot can catch the progress).
    void setPace(qint64 bytes, int tickMs);

    void start();
    void cancel();
    Stage stage() const { return m_stage; }

signals:
    void progress(qint64 done, qint64 total);
    void stageChanged();
    void finished(const AppImageDownload::Result &result);

private:
    void drain();
    void replyFinished();
    void downloaded();
    void extracted();
    void install(const QString &partial, const QString &name);
    void end(const Result &result);
    void fail(const QString &error);
    QString partialPath(const QString &name) const;

    QNetworkAccessManager *m_network;
    ReleaseAsset m_asset;
    QString m_inside;
    QString m_folder;
    bool m_relaxed = false;
    bool m_replace = false;
    qint64 m_paceBytes = 0;
    QTimer m_paceTimer;

    QPointer<QNetworkReply> m_reply;
    QFile m_file;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    qint64 m_done = 0;
    bool m_replyDone = false;
    bool m_cancelled = false;
    bool m_ended = false;
    Stage m_stage = Stage::Downloading;

    std::atomic_bool m_cancelExtract = false;
    QFutureWatcher<AppImageInstall::Extracted> m_extract;
};

Q_DECLARE_METATYPE(AppImageDownload::Result)
