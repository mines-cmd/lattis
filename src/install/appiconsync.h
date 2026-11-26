#pragma once

#include "appicon.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <functional>

// See docs/code/install.md.
namespace AppIconSync {

// AppUpdates::managedBy, passed in so this doesn't link the updater.
using ManagedBy = std::function<QString(const QString &appImage, const QString &applicationsDir)>;

QString chosenColors(const QString &choice, const QString &activeColors, const QStringList &availableColors);

struct Job
{
    AppIcon::Colors colors;
    QString dataHome;
    QStringList dataDirs;
    QString appImage;
    bool menuEntry = true;
    bool entries = true;
    ManagedBy managedBy;
};

struct Result
{
    QString key;
    QString folder;
    QString icon;
    QString managedBy;
    QString error;
};

Result run(const Job &job);

} // namespace AppIconSync

class AppIconUpdater : public QObject
{
    Q_OBJECT

public:
    struct Setup
    {
        std::function<QString()> choice;
        std::function<bool()> menuEntry;
        std::function<QString()> activeColors;
        std::function<QStringList()> availableColors;
        std::function<QVariantMap(const QString &)> dogColors;
        std::function<void(const QString &icon, const QString &managedBy)> applied;
        QString dataHome;
        QStringList dataDirs;
        bool entries = true;
        AppIconSync::ManagedBy managedBy;
    };

    static constexpr int kSettleMs = 800;

    explicit AppIconUpdater(Setup setup, QObject *parent = nullptr);

    // Nothing runs before begin(), which main.cpp calls after the first frame.
    void begin();
    void request(int delayMs = 0);

private:
    void start();
    void finished();

    Setup m_setup;
    QTimer m_timer;
    QFutureWatcher<AppIconSync::Result> m_watcher;
    bool m_started = false;
    bool m_pending = false;
    QString m_emberKey;
    QString m_windowKey;
};
