#pragma once

#include "appimageinstall.h"

#include <QFutureWatcher>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QUrl>
#include <QtQmlIntegration>

#include <atomic>
#include <functional>

class QJSEngine;
class QNetworkAccessManager;
class QNetworkReply;
class QQmlEngine;

// See docs/code/install.md.

namespace FfmpegInstall {
// The asset in BtbN's release for this CPU (QSysInfo::currentCpuArchitecture()); empty when there is none.
QString assetName(const QString &cpuArchitecture);

struct Extracted
{
    bool cancelled = false;
    QString error;
};
Extracted extract(const QString &archivePath, const QString &folder, const std::atomic_bool *cancel);
}

class FfmpegTool : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // "idle", "checking", "downloading", "installing" or "failed".
    Q_PROPERTY(QString state READ state NOTIFY changed)
    // 0 to 1 while downloading, -1 when unknown.
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    // The ffmpeg to run: Lattis's own copy, else the one on PATH; empty when there is none.
    Q_PROPERTY(QString path READ path NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    // Lattis installed the copy in use.
    Q_PROPERTY(bool ownCopy READ ownCopy NOTIFY changed)
    // The download's size in bytes once the release is known, else 0.
    Q_PROPERTY(qint64 size READ size NOTIFY changed)

public:
    struct Setup
    {
        QString toolsDir;
        // The environment the program runs with (QProcess::environment() form); empty: the app's own.
        std::function<QStringList()> environment;
    };
    // Called once at startup; nothing is made until QML uses it.
    static void setup(const Setup &setup) { s_setup = setup; }
    static FfmpegTool *instance();
    static FfmpegTool *create(QQmlEngine *, QJSEngine *) { return instance(); }

    // The own copy if it is there, else ffmpeg on PATH, else empty.
    static QString resolve(const QString &toolsDir);
    static QString ownPath(const QString &toolsDir);

    explicit FfmpegTool(const Setup &setup, QObject *parent = nullptr);
    ~FfmpegTool() override;

    QString state() const;
    double progress() const { return m_progress; }
    QString error() const { return m_error; }
    QString path() const { return m_path; }
    bool available() const { return !m_path.isEmpty(); }
    bool ownCopy() const { return m_ownCopy; }
    qint64 size() const { return m_size; }

    Q_INVOKABLE void install();
    // Stops a running install and deletes what it wrote; also clears a failure.
    Q_INVOKABLE void cancel();
    // Deletes Lattis's own copy.
    Q_INVOKABLE void remove();
    // Looks for ffmpeg again.
    Q_INVOKABLE void refresh();

    // http: and file: URLs, and a release answered from another address (tests).
    void setReleaseUrl(const QUrl &url) { m_releaseUrl = url; }
    void setAssetName(const QString &name) { m_assetName = name; }
    void setRelaxed(bool relaxed) { m_relaxed = relaxed; }

signals:
    void changed();

private:
    enum class State { Idle, Checking, Downloading, Installing, Failed };

    QNetworkAccessManager *network();
    void gotRelease(quint64 serial, const QByteArray &json, const QString &networkError);
    void downloadFinished(quint64 serial, const AppImageDownload::Result &result);
    void extracted(quint64 serial);
    void verify(quint64 serial);
    void verified(quint64 serial);
    void fail(const QString &error);
    void abandon();
    void setState(State state);
    QString ownFolder() const;
    QString partialFolder() const;

    static Setup s_setup;
    Setup m_setup;
    QNetworkAccessManager *m_network = nullptr;
    QUrl m_releaseUrl;
    QString m_assetName;
    bool m_relaxed = false;

    State m_state = State::Idle;
    double m_progress = -1;
    QString m_error;
    QString m_path;
    bool m_ownCopy = false;
    qint64 m_size = 0;
    quint64 m_serial = 0;

    QPointer<QNetworkReply> m_reply;
    QPointer<AppImageDownload> m_download;
    QString m_archive;
    std::atomic_bool m_cancelExtract = false;
    QFutureWatcher<FfmpegInstall::Extracted> m_extract;
    QProcess *m_check = nullptr;
};
