#include "applocation.h"

#include "appimageinstall.h"
#include "appupdate.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>

AppLocation::Setup AppLocation::s_setup;

namespace {
QString tr(const char *text)
{
    return QCoreApplication::translate("AppLocation", text);
}

QString targetOf(const QString &home)
{
    return AppImageInstall::targetFolder({home + QStringLiteral("/Applications"), home + QStringLiteral("/AppImages")}, home);
}
}

namespace AppLocations {
QString moveInto(const QString &appImage, const QString &folder, QString *moved)
{
    if (!QDir().mkpath(folder))
        return tr("Couldn't create %1").arg(QDir::toNativeSeparators(folder));
    const QString target = QDir(folder).filePath(QFileInfo(appImage).fileName());
    if (QFileInfo::exists(target))
        return tr("There is already a file called %1 in %2")
            .arg(QFileInfo(target).fileName(), QDir::toNativeSeparators(folder));
    // The running image stays mounted from the old inode, so renaming it under the process is safe.
    QFile file(appImage);
    if (!file.rename(target))
        return tr("Couldn't move it: %1").arg(file.errorString());
    *moved = target;
    return {};
}

QString moveBlocker(const QString &appImage, const QString &folder, const QString &applicationsDir)
{
    const QFileInfo file(appImage);
    if (!file.isFile())
        return tr("The AppImage file isn't there");
    if (!QFileInfo(file.absolutePath()).isWritable())
        return tr("%1 can't be written to").arg(QDir::toNativeSeparators(file.absolutePath()));
    const QString manager = AppUpdates::managedBy(appImage, applicationsDir);
    if (!manager.isEmpty())
        return tr("It is managed by %1").arg(manager);
    if (QFileInfo::exists(QDir(folder).filePath(file.fileName())))
        return tr("There is already a file called %1 in %2")
            .arg(file.fileName(), QDir::toNativeSeparators(folder));
    return {};
}
}

AppLocation *AppLocation::instance()
{
    static AppLocation *location = [] {
        auto *l = new AppLocation(s_setup, QCoreApplication::instance());
        QJSEngine::setObjectOwnership(l, QJSEngine::CppOwnership);
        return l;
    }();
    return location;
}

AppLocation::AppLocation(const Setup &setup, QObject *parent)
    : QObject(parent)
    , m_setup(setup)
{
    refresh();
}

bool AppLocation::available() const
{
    return !m_setup.appImage.isEmpty();
}

bool AppLocation::inFolder() const
{
    const QString dir = QFileInfo(m_setup.appImage).absolutePath();
    return dir == m_setup.home + QStringLiteral("/Applications") || dir == m_setup.home + QStringLiteral("/AppImages");
}

void AppLocation::refresh()
{
    if (!available())
        return;
    m_folder = targetOf(m_setup.home);
    m_blocker = inFolder() ? QString() : AppLocations::moveBlocker(m_setup.appImage, m_folder, m_setup.applicationsDir);
    emit changed();
}

QString AppLocation::move()
{
    refresh();
    if (!canMove())
        return m_blocker.isEmpty() ? tr("Lattis is already there") : m_blocker;
    QString moved;
    const QString error = AppLocations::moveInto(m_setup.appImage, m_folder, &moved);
    if (!error.isEmpty())
        return error;
    m_setup.appImage = moved;
    if (m_setup.moved)
        m_setup.moved(moved);
    refresh();
    return {};
}
