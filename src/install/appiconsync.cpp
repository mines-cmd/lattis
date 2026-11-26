#include "appiconsync.h"

#include "desktopentry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QtConcurrent/QtConcurrentRun>

Q_LOGGING_CATEGORY(lcAppIconSync, "lattis.appicon")

namespace AppIconSync {

QString chosenColors(const QString &choice, const QString &activeColors, const QStringList &availableColors)
{
    return availableColors.contains(choice) ? choice : activeColors;
}

Result run(const Job &job)
{
    Result out;
    out.folder = QDir(job.dataHome).filePath(QStringLiteral("icons/lattis"));
    out.key = AppIcon::key(job.colors);
    out.icon = AppIcon::write(out.folder, job.colors);
    if (out.icon.isEmpty()) {
        out.error = QStringLiteral("couldn't write the app icon into %1").arg(out.folder);
        return out;
    }

    if (job.entries && !job.appImage.isEmpty()) {
        if (!job.menuEntry) {
            DesktopEntry::removeOwn(job.dataHome);
        } else {
            if (job.managedBy)
                out.managedBy = job.managedBy(job.appImage, QDir(job.dataHome).filePath(QStringLiteral("applications")));
            if (out.managedBy.isEmpty())
                DesktopEntry::ensureOwn(job.appImage, job.dataHome, out.icon, &out.error);
        }
    } else if (job.entries) {
        // The system entry can't be changed; a copy in the user's applications folder overrides it,
        // but only while it has something to add. A `cmake --install --prefix ~/.local` puts the
        // installed entry itself there; only its Icon changes, and any other entry there stays as it is.
        const QString copy = DesktopEntry::userCopyPath(job.dataHome);
        const bool installedHere = QFileInfo::exists(copy) && !DesktopEntry::isUserCopy(copy);
        const bool ember = out.key == AppIcon::key(AppIcon::ember());
        if (installedHere && DesktopEntry::isInstalledEntry(copy)) {
            DesktopEntry::setInstalledIcon(copy, ember ? QString() : out.icon, &out.error);
        } else if (ember) {
            DesktopEntry::removeUserCopy(job.dataHome);
        } else if (const QString system = DesktopEntry::systemEntry(job.dataDirs); !system.isEmpty() && !installedHere) {
            DesktopEntry::ensureUserCopy(system, job.dataHome, out.icon, &out.error);
        }
    }

    AppIcon::cleanUp(out.folder, {out.key});
    return out;
}

} // namespace AppIconSync

AppIconUpdater::AppIconUpdater(Setup setup, QObject *parent)
    : QObject(parent), m_setup(std::move(setup))
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &AppIconUpdater::start);
    connect(&m_watcher, &QFutureWatcher<AppIconSync::Result>::finished, this, &AppIconUpdater::finished);
}

void AppIconUpdater::begin()
{
    m_started = true;
    request();
}

void AppIconUpdater::request(int delayMs)
{
    if (m_started)
        m_timer.start(delayMs);
}

void AppIconUpdater::start()
{
    if (m_watcher.isRunning()) {
        m_pending = true;
        return;
    }
    // Not in the constructor: it reads the templates, which waits until after the first frame.
    if (m_emberKey.isEmpty()) {
        m_emberKey = AppIcon::key(AppIcon::ember());
        m_windowKey = m_emberKey;
    }
    const QString name = AppIconSync::chosenColors(m_setup.choice(), m_setup.activeColors(), m_setup.availableColors());
    AppIconSync::Job job;
    job.colors = AppIcon::fromMap(m_setup.dogColors(name));
    job.dataHome = m_setup.dataHome;
    job.dataDirs = m_setup.dataDirs;
    job.appImage = qEnvironmentVariable("APPIMAGE");
    job.menuEntry = m_setup.menuEntry();
    job.entries = m_setup.entries;
    job.managedBy = m_setup.managedBy;
    m_watcher.setFuture(QtConcurrent::run(&AppIconSync::run, job));
}

void AppIconUpdater::finished()
{
    const AppIconSync::Result result = m_watcher.result();
    if (!result.error.isEmpty())
        qCWarning(lcAppIconSync).noquote() << "can't apply the app icon:" << result.error;
    if (!result.icon.isEmpty()) {
        m_setup.applied(result.icon, result.managedBy);
        if (result.key != m_windowKey) {
            m_windowKey = result.key;
            QIcon icon;
            for (int size : AppIcon::sizes()) {
                const QString path = result.key == m_emberKey
                                         ? QStringLiteral(":/qt/qml/Launcher/resources/logo/icons/lattis-%1.png").arg(size)
                                         : QDir(result.folder).filePath(AppIcon::fileName(result.key, size));
                icon.addFile(path, QSize(size, size));
            }
            QGuiApplication::setWindowIcon(icon);
        }
    }
    if (m_pending) {
        m_pending = false;
        start();
    }
}
