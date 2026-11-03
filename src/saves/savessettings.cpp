#include "savessettings.h"
#include "backupplan.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJSEngine>
#include <QLocale>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

namespace {
constexpr int kFirstCheckMs = 10'000;
constexpr int kRecheckMs = 6 * 60 * 60 * 1000;
SavesSettings *s_instance = nullptr;
}

SavesSettings *SavesSettings::instance()
{
    if (!s_instance) {
        s_instance = new SavesSettings(s_setup, QCoreApplication::instance());
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    }
    return s_instance;
}

void SavesSettings::startDailyCheck(std::function<bool()> anyOn)
{
    auto *timer = new QTimer(QCoreApplication::instance());
    const QString manifestDir = s_setup.manifestDir;
    QObject::connect(timer, &QTimer::timeout, timer, [timer, anyOn = std::move(anyOn), manifestDir] {
        timer->setInterval(kRecheckMs);
        if (SavesManifest::checkDue(SavesManifest::readMeta(manifestDir)) && anyOn())
            instance()->run(false);
    });
    timer->start(kFirstCheckMs);
}

SavesSettings::SavesSettings(const Setup &setup, QObject *parent)
    : QObject(parent)
    , m_setup(setup)
{
    connect(&m_watcher, &QFutureWatcher<SavesManifest::UpdateResult>::finished, this, [this] {
        const SavesManifest::UpdateResult result = m_watcher.result();
        m_error = result.status == SavesManifest::UpdateResult::Failed ? result.error : QString();
        readState();
        emit updatingChanged();
    });
    refresh();
}

SavesSettings::~SavesSettings()
{
    m_watcher.waitForFinished();
}

QString SavesSettings::defaultFolder() const
{
    return QStringLiteral("~/.local/share/lattis-saves");
}

void SavesSettings::refresh()
{
    QFile file(m_setup.dataDir + QStringLiteral("/settings.toml"));
    const BackupPlan::Settings settings =
        file.open(QIODevice::ReadOnly) ? BackupPlan::parseSettings(file.readAll(), file.fileName()).value : BackupPlan::Settings();
    m_folder = settings.autoFolder.isEmpty() ? defaultFolder() : settings.autoFolder;
    m_keep = settings.autoKeep;
    emit settingsChanged();
    readState();
}

void SavesSettings::readState()
{
    const SavesManifest::Meta meta = SavesManifest::readMeta(m_setup.manifestDir);
    m_fetched = meta.fetched;
    if (updating())
        m_status = tr("Downloading");
    else if (!m_error.isEmpty())
        m_status = tr("Couldn't download: %1").arg(m_error);
    else if (!m_fetched.isValid())
        m_status = tr("Not downloaded yet");
    else
        m_status = tr("Downloaded %1").arg(QLocale().toString(m_fetched.toLocalTime(), QLocale::ShortFormat));
    emit stateChanged();
}

void SavesSettings::ensureManifest()
{
    if (!downloaded() && !updating())
        run(false);
}

void SavesSettings::update()
{
    if (!updating())
        run(true);
}

void SavesSettings::run(bool force)
{
    if (updating())
        return;
    const QString dir = m_setup.manifestDir;
    m_error.clear();
    m_watcher.setFuture(QtConcurrent::run([dir, force] { return SavesManifest::update(dir, SavesManifest::url(), force); }));
    readState();
    emit updatingChanged();
}

QString SavesSettings::setFolder(const QString &folder)
{
    QString error;
    if (!BackupPlan::saveAutoFolder(m_setup.dataDir + QStringLiteral("/settings.toml"), folder, &error))
        return tr("Couldn't save auto_folder in settings.toml: %1").arg(error);
    refresh();
    return {};
}

QString SavesSettings::setKeep(int keep)
{
    if (keep < 1 || keep > 100)
        return tr("Keep between 1 and 100 backups");
    QString error;
    if (!BackupPlan::saveAutoKeep(m_setup.dataDir + QStringLiteral("/settings.toml"), keep, &error))
        return tr("Couldn't save auto_keep in settings.toml: %1").arg(error);
    refresh();
    return {};
}
