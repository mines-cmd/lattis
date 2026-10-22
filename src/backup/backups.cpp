#include "backups.h"
#include "dateformat.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QLocale>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace {
QString skippedName(BackupPlan::EmulatorPart::Skipped skipped)
{
    switch (skipped) {
    case BackupPlan::EmulatorPart::Skipped::No: return {};
    case BackupPlan::EmulatorPart::Skipped::NotInstalled: return QStringLiteral("notInstalled");
    case BackupPlan::EmulatorPart::Skipped::NoSaves: return QStringLiteral("noSaves");
    }
    return {};
}

QString skippedName(ProtonSaves::Skipped skipped)
{
    switch (skipped) {
    case ProtonSaves::Skipped::No: return {};
    case ProtonSaves::Skipped::NeverLaunched: return QStringLiteral("neverLaunched");
    case ProtonSaves::Skipped::Uninstalled: return QStringLiteral("uninstalled");
    case ProtonSaves::Skipped::Unlisted: return QStringLiteral("unlisted");
    }
    return {};
}

std::pair<QStringList, qint64> found(const QList<BackupPlan::Place> &places, const QString &home)
{
    QStringList paths;
    qint64 bytes = 0;
    for (const BackupPlan::Place &p : places) {
        if (!p.exists)
            continue;
        paths.append(BackupPlan::tilde(p.path, home));
        bytes = p.bytes < 0 || bytes < 0 ? -1 : bytes + p.bytes;
    }
    return {paths, bytes};
}
}

Backups *Backups::instance()
{
    if (!s_instance) {
        s_instance = new Backups(s_setup, QCoreApplication::instance());
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    }
    return s_instance;
}

Backups::Backups(const Setup &setup, QObject *parent)
    : QObject(parent)
    , m_setup(setup)
    , m_job(Backup::lastFile(setup.stateDir, setup.dataDirKey), setup.runningGames)
    , m_restore(setup.runningGames)
    , m_importsFile(MovedAside::file(setup.stateDir, setup.dataDirKey))
{
    // Quitting doesn't wait for a backup to finish (the thread pool is waited on before this
    // object goes); its partial file is removed. An Import cancelled puts everything back.
    if (auto *app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit, this, [this] {
            m_job.cancel();
            m_restore.cancel();
        });
    }
    connect(&m_restore, &RestoreJob::finished, this, &Backups::onImportFinished);
    connect(&m_restore, &RestoreJob::runningChanged, this, &Backups::importingChanged);
    connect(&m_restore, &RestoreJob::progressChanged, this, &Backups::importProgressChanged);
    connect(&m_measureImport, &QFutureWatcher<QList<QList<qint64>>>::finished, this, [this] {
        const QList<QList<qint64>> now = m_measureImport.result();
        if (m_import && now.size() == m_import->parts.size()) {
            m_importNow = now;
            updateImportMap();
        }
    });
    connect(&m_measureImports, &QFutureWatcher<QList<qint64>>::finished, this, [this] {
        const QList<qint64> bytes = m_measureImports.result();
        if (bytes.size() == m_imports.size()) {
            m_importBytes = bytes;
            emit importsChanged();
        }
    });
    loadImports();
    connect(&m_job, &BackupJob::runningChanged, this, &Backups::runningChanged);
    connect(&m_job, &BackupJob::progressChanged, this, &Backups::progressChanged);
    connect(&m_job, &BackupJob::finished, this, [this](const QString &kind, const QString &path, const QString &error) {
        if (error.isEmpty())
            emit lastChanged();
        emit finished(kind, path, error);
    });
    connect(&m_measureLibrary, &QFutureWatcher<BackupPlan::LibraryPlan>::finished, this, [this] {
        const bool credentials = m_library.credentials;
        m_library = m_measureLibrary.result();
        m_library.credentials = credentials;
        updateLibraryMap();
    });
    connect(&m_measureSaves, &QFutureWatcher<BackupPlan::SavesPlan>::finished, this, [this] {
        const bool keys = m_saves.keys;
        const bool proton = m_saves.proton;
        m_saves = m_measureSaves.result();
        m_saves.keys = keys;
        m_saves.proton = proton;
        updateSavesMap();
    });
}

Backups::~Backups()
{
    if (s_instance == this)
        s_instance = nullptr;
    m_measureLibrary.waitForFinished();
    m_measureSaves.waitForFinished();
    m_measureImports.waitForFinished();
    m_measureImport.waitForFinished();
}

BackupPlan::Settings Backups::readSettings() const
{
    QFile file(m_setup.dataDir + QStringLiteral("/settings.toml"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const Parsed<BackupPlan::Settings> parsed = BackupPlan::parseSettings(file.readAll(), QStringLiteral("settings.toml"));
    for (const QString &warning : parsed.warnings)
        qCWarning(lcBackup).noquote() << warning;
    return parsed.value;
}

QVariantMap Backups::choices() const
{
    const BackupPlan::Settings s = readSettings();
    return {{QStringLiteral("credentials"), s.credentials},
            {QStringLiteral("keys"), s.keys},
            {QStringLiteral("proton"), s.proton}};
}

QString Backups::setChoice(const QString &key, bool value)
{
    QString error;
    if (!BackupPlan::saveSetting(m_setup.dataDir + QStringLiteral("/settings.toml"), key, value, &error))
        return tr("Couldn't save %1 in settings.toml: %2").arg(key, error);
    if (key == QLatin1String("credentials"))
        m_library.credentials = value;
    else if (key == QLatin1String("keys"))
        m_saves.keys = value;
    else if (key == QLatin1String("proton"))
        m_saves.proton = value;
    emit choicesChanged();
    return {};
}

void Backups::planLibrary()
{
    m_library = BackupPlan::planLibrary(m_setup.sources(), readSettings());
    updateLibraryMap();
    m_measureLibrary.setFuture(QtConcurrent::run([plan = m_library]() mutable {
        BackupPlan::measure(plan);
        return plan;
    }));
}

void Backups::planSaves()
{
    m_saves = BackupPlan::planSaves(m_setup.sources(), readSettings());
    updateSavesMap();
    m_measureSaves.setFuture(QtConcurrent::run([plan = m_saves]() mutable {
        BackupPlan::measure(plan);
        return plan;
    }));
}

void Backups::updateLibraryMap()
{
    const QString home = QDir::homePath();
    QVariantList roms;
    const QDir dataDir(m_library.dataDir);
    for (const BackupPlan::Place &rom : std::as_const(m_library.roms)) {
        if (rom.exists)
            roms.append(QVariantMap{{QStringLiteral("path"), dataDir.relativeFilePath(rom.path)},
                                    {QStringLiteral("bytes"), rom.bytes}});
    }
    m_libraryMap = {
        {QStringLiteral("measured"), m_library.dataBytes >= 0},
        {QStringLiteral("dataDir"), BackupPlan::tilde(m_library.dataDir, home)},
        {QStringLiteral("games"), m_library.games},
        {QStringLiteral("credentials"),
         QVariantMap{{QStringLiteral("exists"), m_library.credentialsFile.exists},
                     {QStringLiteral("bytes"), m_library.credentialsFile.bytes}}},
        {QStringLiteral("roms"), roms},
        {QStringLiteral("dataBytes"), m_library.dataBytes},
    };
    emit libraryPlanChanged();
}

void Backups::updateSavesMap()
{
    const QString home = m_saves.home;
    QVariantList emulators;
    for (const BackupPlan::EmulatorPart &part : std::as_const(m_saves.emulators)) {
        const auto [saves, savesBytes] = found(part.saves, home);
        const auto [keys, keysBytes] = found(part.keys, home);
        qint64 besideBytes = part.measured ? 0 : -1;
        for (const BesideRom::GameSaves &game : part.besideRoms) {
            for (const BesideRom::SaveFile &file : game.files)
                besideBytes += file.size;
        }
        emulators.append(QVariantMap{
            {QStringLiteral("id"), part.id},
            {QStringLiteral("name"), part.name},
            {QStringLiteral("describe"), part.describe},
            {QStringLiteral("skipped"), skippedName(part.skipped)},
            {QStringLiteral("saves"), saves},
            {QStringLiteral("keys"), keys},
            {QStringLiteral("savesBytes"), savesBytes},
            {QStringLiteral("keysBytes"), keysBytes},
            {QStringLiteral("besideRomSaves"), part.besideRomSaves},
            {QStringLiteral("besideRoms"), part.besideRoms.size()},
            {QStringLiteral("besideBytes"), besideBytes},
        });
    }
    QVariantList proton;
    for (const BackupPlan::ProtonPart &part : std::as_const(m_saves.protonGames)) {
        proton.append(QVariantMap{
            {QStringLiteral("id"), part.gameId},
            {QStringLiteral("title"), part.title},
            {QStringLiteral("skipped"), skippedName(part.saves.skipped)},
            {QStringLiteral("bytes"), part.measured ? part.saves.bytes : -1},
        });
    }
    m_savesMap = {
        {QStringLiteral("measured"), m_saves.measured},
        {QStringLiteral("emulators"), emulators},
        {QStringLiteral("proton"), proton},
    };
    emit savesPlanChanged();
}

QVariantMap Backups::last() const
{
    QVariantMap out;
    const QString home = QDir::homePath();
    const QHash<QString, Backup::Last> all = Backup::readLast(Backup::lastFile(m_setup.stateDir, m_setup.dataDirKey));
    for (auto it = all.cbegin(); it != all.cend(); ++it) {
        const QDate date = it->time.toLocalTime().date();
        const qint64 days = date.daysTo(QDate::currentDate());
        const QString when = days <= 0 ? tr("today")
            : days == 1                ? tr("yesterday")
            : days < 7                 ? tr("%1 days ago").arg(days)
                                       : tr("on %1").arg(DateFormat::shortDate(date));
        const QString path = BackupPlan::tilde(it->path, home);
        out.insert(it.key(), QVariantMap{
            {QStringLiteral("path"), path},
            {QStringLiteral("folder"), QUrl::fromLocalFile(QFileInfo(it->path).absolutePath())},
            {QStringLiteral("text"), tr("Last backed up %1 to %2").arg(when, path)},
        });
    }
    return out;
}

QUrl Backups::defaultFile(const QString &kind) const
{
    const Backup::Last last = Backup::readLast(Backup::lastFile(m_setup.stateDir, m_setup.dataDirKey)).value(kind);
    QString folder = last.path.isEmpty() ? QString() : QFileInfo(last.path).absolutePath();
    if (folder.isEmpty() || !QFileInfo(folder).isDir())
        folder = QDir::homePath();
    return QUrl::fromLocalFile(QDir(folder).filePath(BackupPlan::fileName(kind, QDate::currentDate())));
}

QString Backups::start(const QString &kind, const QUrl &file)
{
    const QString path = file.toLocalFile();
    if (path.isEmpty())
        return tr("Pick a file on this computer.");
    if (kind == Backup::kLibrary)
        return m_job.start(m_library, path);
    if (kind == Backup::kSaves)
        return m_job.start(m_saves, path);
    return tr("Unknown backup: %1").arg(kind);
}

QString Backups::sizeText(qint64 bytes) const
{
    return QLocale().formattedDataSize(qMax<qint64>(bytes, 0), 1, QLocale::DataSizeTraditionalFormat);
}

QUrl Backups::screenshotTarget() const
{
    const QString file = qEnvironmentVariable("LATTIS_BACKUP_TO");
    return file.isEmpty() ? QUrl() : QUrl::fromLocalFile(QFileInfo(file).absoluteFilePath());
}

QString Backups::startImport(const Restore::Plan &plan)
{
    if (m_job.running())
        return tr("Wait for the backup to finish.");
    const bool library = plan.manifest.kind == Backup::kLibrary;
    if (library && m_setup.holdLibrary)
        m_setup.holdLibrary(true);
    const QString error = m_restore.start(plan);
    if (!error.isEmpty()) {
        if (library && m_setup.holdLibrary)
            m_setup.holdLibrary(false);
        return error;
    }
    m_restoreKind = plan.manifest.kind;
    m_restoreArchive = plan.archive;
    return {};
}

QString Backups::startImport()
{
    if (!m_import)
        return tr("Pick a backup first.");
    if (std::none_of(m_import->parts.cbegin(), m_import->parts.cend(), [](const Restore::Part &p) { return p.checked; }))
        return tr("Check what to restore.");
    return startImport(*m_import);
}

QString Backups::openImport(const QUrl &file)
{
    m_import.reset();
    m_importNow.clear();
    const QString path = file.toLocalFile();
    if (path.isEmpty()) {
        updateImportMap();
        return tr("Pick a file on this computer.");
    }
    const Restore::Opened opened = Restore::open(path);
    if (!opened.manifest) {
        updateImportMap();
        return opened.error;
    }
    m_import = Restore::plan(path, *opened.manifest, m_setup.sources());
    m_import->warning = opened.warning;

    QList<QStringList> paths;
    for (const Restore::Part &part : std::as_const(m_import->parts)) {
        QStringList targets;
        for (const Restore::Target &t : part.targets)
            targets.append(part.kind != Restore::Part::Kind::BesideRoms && t.exists && t.problem.isEmpty() ? t.path : QString());
        m_importNow.append(QList<qint64>(targets.size(), -1));
        paths.append(targets);
    }
    m_measureImport.setFuture(QtConcurrent::run([paths] {
        QList<QList<qint64>> bytes;
        for (const QStringList &targets : paths) {
            QList<qint64> b;
            for (const QString &path : targets)
                b.append(path.isEmpty() ? -1 : MovedAside::bytes({path}));
            bytes.append(b);
        }
        return bytes;
    }));
    updateImportMap();
    return {};
}

void Backups::checkImportPart(int index, bool checked)
{
    if (!m_import || index < 0 || index >= m_import->parts.size() || !m_import->parts[index].restorable())
        return;
    m_import->parts[index].checked = checked;
    updateImportMap();
}

namespace {
QString partKind(Restore::Part::Kind kind)
{
    switch (kind) {
    case Restore::Part::Kind::Library: return QStringLiteral("library");
    case Restore::Part::Kind::Saves: return QStringLiteral("saves");
    case Restore::Part::Kind::Keys: return QStringLiteral("keys");
    case Restore::Part::Kind::BesideRoms: return QStringLiteral("besideRoms");
    case Restore::Part::Kind::Proton: return QStringLiteral("proton");
    }
    return {};
}

QString installName(const QString &type)
{
    if (type == QLatin1String("flatpak"))
        return Backups::tr("the Flatpak");
    if (type == QLatin1String("portable"))
        return Backups::tr("the portable install");
    return Backups::tr("the native install");
}
}

void Backups::updateImportMap()
{
    if (!m_import) {
        m_importMap.clear();
        emit importPlanChanged();
        return;
    }
    const Restore::Plan &plan = *m_import;
    const Restore::Manifest &m = plan.manifest;
    const QString home = QDir::homePath();

    QVariantList parts;
    bool anyChecked = false;
    for (qsizetype i = 0; i < plan.parts.size(); ++i) {
        const Restore::Part &part = plan.parts[i];
        anyChecked = anyChecked || part.checked;
        QVariantList targets;
        for (qsizetype j = 0; j < part.targets.size(); ++j) {
            const Restore::Target &t = part.targets[j];
            targets.append(QVariantMap{
                {QStringLiteral("label"), t.label},
                {QStringLiteral("problem"), t.problem},
                {QStringLiteral("exists"), t.exists},
                {QStringLiteral("bytes"), m_importNow.value(i).value(j, -1)},
            });
        }
        QString installText;
        if (!part.install.isEmpty() && !part.backedUp.isEmpty() && part.install != part.backedUp)
            installText = tr("Backed up from %1, restored into %2's folders.")
                              .arg(installName(part.backedUp), installName(part.install));
        parts.append(QVariantMap{
            {QStringLiteral("index"), int(i)},
            {QStringLiteral("kind"), partKind(part.kind)},
            {QStringLiteral("id"), part.id},
            {QStringLiteral("name"), part.name},
            {QStringLiteral("installText"), installText},
            {QStringLiteral("bytes"), part.bytes},
            {QStringLiteral("problem"), part.problem},
            {QStringLiteral("restorable"), part.restorable()},
            {QStringLiteral("checked"), part.checked},
            {QStringLiteral("targets"), targets},
        });
    }

    const QDate today = QDate::currentDate();
    QString aside;
    for (int n = 1; aside.isEmpty(); ++n) {
        const QString stamp = n == 1 ? today.toString(Qt::ISODate)
                                     : QStringLiteral("%1-%2").arg(today.toString(Qt::ISODate)).arg(n);
        const QString candidate = plan.dataDir + QStringLiteral(".before-import-") + stamp;
        if (!QFileInfo::exists(candidate) && !QFileInfo::exists(plan.dataDir + QStringLiteral(".import-") + stamp))
            aside = QFileInfo(candidate).fileName();
    }

    const QString made = m.created.isValid()
        ? tr("%1, by Lattis %2").arg(DateFormat::shortDate(m.created.toLocalTime().date()), m.appVersion)
        : tr("By Lattis %1").arg(m.appVersion);
    m_importMap = {
        {QStringLiteral("kind"), m.kind},
        {QStringLiteral("file"), QFileInfo(plan.archive).fileName()},
        {QStringLiteral("made"), made},
        {QStringLiteral("warning"), plan.warning},
        {QStringLiteral("sameLibrary"), plan.sameLibrary},
        {QStringLiteral("games"), m.games},
        {QStringLiteral("gamesNow"), plan.games},
        {QStringLiteral("dataDir"), BackupPlan::tilde(plan.dataDir, home)},
        {QStringLiteral("aside"), aside},
        {QStringLiteral("carriedOver"), plan.carriedOver},
        {QStringLiteral("bytes"), plan.bytes()},
        {QStringLiteral("anyChecked"), anyChecked},
        {QStringLiteral("parts"), parts},
    };
    emit importPlanChanged();
}

QUrl Backups::importFolder() const
{
    const QHash<QString, Backup::Last> all = Backup::readLast(Backup::lastFile(m_setup.stateDir, m_setup.dataDirKey));
    QDateTime newest;
    QString folder;
    for (const Backup::Last &last : all) {
        const QString dir = QFileInfo(last.path).absolutePath();
        if (QFileInfo(dir).isDir() && (!newest.isValid() || last.time > newest)) {
            newest = last.time;
            folder = dir;
        }
    }
    return QUrl::fromLocalFile(folder.isEmpty() ? QDir::homePath() : folder);
}

QVariantMap Backups::screenshotImport() const
{
    const QString file = qEnvironmentVariable("LATTIS_IMPORT_FROM");
    if (file.isEmpty())
        return {};
    const bool start = qEnvironmentVariableIntValue("LATTIS_IMPORT_START") == 1;
    // Once: the copy a library Import restarts has the same environment.
    qunsetenv("LATTIS_IMPORT_FROM");
    qunsetenv("LATTIS_IMPORT_START");
    return {{QStringLiteral("file"), QUrl::fromLocalFile(QFileInfo(file).absoluteFilePath())}, {QStringLiteral("start"), start}};
}

void Backups::onImportFinished(const QString &error)
{
    const bool library = m_restoreKind == Backup::kLibrary;
    if (!error.isEmpty()) {
        if (library && m_setup.holdLibrary)
            m_setup.holdLibrary(false);
        emit importFinished(m_restoreKind, error);
        return;
    }
    m_import.reset();
    m_importNow.clear();
    updateImportMap();
    const Restore::Result result = m_restore.result();
    MovedAside::Import import;
    import.kind = m_restoreKind;
    import.archive = m_restoreArchive;
    import.time = QDateTime::currentDateTime();
    import.paths = result.movedAside;
    // Losing it loses only the notice: what was moved aside stays where it is.
    QString saveError;
    if (!MovedAside::record(m_importsFile, import, &saveError))
        qCWarning(lcBackup).noquote() << "Couldn't remember what the import moved aside:" << saveError;
    emit importFinished(m_restoreKind, {});
    if (library) {
        // The data dir under this copy is another one now: the restarted copy reads it.
        if (m_setup.restart)
            m_setup.restart();
        return;
    }
    loadImports();
}

void Backups::loadImports()
{
    const QList<MovedAside::Import> all = MovedAside::read(m_importsFile);
    m_imports = MovedAside::present(all);
    bool changed = m_imports.size() != all.size();
    for (qsizetype i = 0; !changed && i < m_imports.size(); ++i)
        changed = m_imports[i].paths != all[i].paths || m_imports[i].notice != all[i].notice;
    if (changed)
        saveImports();

    m_importBytes = QList<qint64>(m_imports.size(), -1);
    QList<QStringList> paths;
    for (const MovedAside::Import &import : std::as_const(m_imports))
        paths.append(import.paths);
    m_measureImports.setFuture(QtConcurrent::run([paths] {
        QList<qint64> bytes;
        for (const QStringList &p : paths)
            bytes.append(MovedAside::bytes(p));
        return bytes;
    }));
    emit importsChanged();
}

void Backups::saveImports()
{
    QString error;
    if (!MovedAside::write(m_importsFile, m_imports, &error))
        qCWarning(lcBackup).noquote() << "Couldn't save" << m_importsFile << ':' << error;
}

int Backups::noticed() const
{
    for (qsizetype i = m_imports.size() - 1; i >= 0; --i) {
        if (m_imports[i].notice)
            return int(i);
    }
    return -1;
}

namespace {
QString placesText(const QStringList &paths)
{
    int folders = 0;
    int files = 0;
    for (const QString &path : paths)
        (QFileInfo(path).isDir() ? folders : files) += 1;
    const QString f = folders == 1 ? Backups::tr("a folder") : Backups::tr("%1 folders").arg(folders);
    const QString g = files == 1 ? Backups::tr("a file") : Backups::tr("%1 files").arg(files);
    if (folders == 0)
        return g;
    if (files == 0)
        return f;
    return Backups::tr("%1 and %2").arg(f, g);
}

struct Arg {
    QString text;
    bool chip = false;
};

// Fills a translated pattern's %1, %2… and keeps the pieces apart, so the bar can draw the
// chip arguments (file names) as chips.
void appendParts(QVariantList &parts, QString &text, const QString &pattern, const QList<Arg> &args)
{
    const auto add = [&](const QString &piece, bool chip) {
        if (piece.isEmpty())
            return;
        text += piece;
        parts.append(QVariantMap{{QStringLiteral("text"), piece}, {QStringLiteral("chip"), chip}});
    };
    static const QRegularExpression marker(QStringLiteral("%(\\d)"));
    qsizetype from = 0;
    for (auto it = marker.globalMatch(pattern); it.hasNext();) {
        const QRegularExpressionMatch match = it.next();
        add(pattern.mid(from, match.capturedStart() - from), false);
        const qsizetype n = match.captured(1).toInt() - 1;
        if (n >= 0 && n < args.size())
            add(args[n].text, args[n].chip);
        from = match.capturedEnd();
    }
    add(pattern.mid(from), false);
}

QUrl folderOf(const QString &path)
{
    const QFileInfo info(path);
    return QUrl::fromLocalFile(info.isDir() && !info.isSymLink() ? info.absoluteFilePath() : info.absolutePath());
}
}

QVariantMap Backups::notice() const
{
    const int index = noticed();
    if (index < 0)
        return {};
    const MovedAside::Import &import = m_imports[index];
    const QString file = QFileInfo(import.archive).fileName();
    QString text;
    QVariantList parts;
    appendParts(parts, text, import.kind == Backup::kLibrary ? tr("Restored the library from %1.")
                                                             : tr("Restored saves from %1."),
                {{file, true}});
    appendParts(parts, text, QStringLiteral(" "), {});
    const qint64 bytes = m_importBytes.value(index, -1);
    const QString size = bytes < 0 ? QString() : QStringLiteral(" (%1)").arg(sizeText(bytes));
    if (import.paths.isEmpty())
        appendParts(parts, text, tr("Nothing was there before it."), {});
    else if (import.paths.size() == 1)
        appendParts(parts, text, tr("What you had is in %1%2."),
                    {{QFileInfo(import.paths.first()).fileName(), true}, {size}});
    else
        appendParts(parts, text, tr("What you had is moved aside: %1%2."), {{placesText(import.paths)}, {size}});
    return {
        {QStringLiteral("text"), text},
        {QStringLiteral("parts"), parts},
        {QStringLiteral("folder"), import.paths.isEmpty() ? QUrl() : folderOf(import.paths.first())},
        {QStringLiteral("canDelete"), !import.paths.isEmpty()},
        {QStringLiteral("places"), import.paths.isEmpty() ? QString() : placesText(import.paths)},
    };
}

QVariantList Backups::movedAside() const
{
    constexpr int kShown = 3;
    const QString home = QDir::homePath();
    QVariantList out;
    for (qsizetype i = m_imports.size() - 1; i >= 0; --i) {
        const MovedAside::Import &import = m_imports[i];
        if (import.paths.isEmpty())
            continue;
        QStringList paths;
        for (const QString &path : import.paths.first(qMin<qsizetype>(kShown, import.paths.size())))
            paths.append(BackupPlan::tilde(path, home));
        const QString when = DateFormat::shortDate(import.time.toLocalTime().date());
        const QString file = QFileInfo(import.archive).fileName();
        const qint64 bytes = m_importBytes.value(i, -1);
        const QString places = placesText(import.paths);
        out.append(QVariantMap{
            {QStringLiteral("index"), int(i)},
            {QStringLiteral("title"), import.kind == Backup::kLibrary ? tr("Library imported %1").arg(when)
                                                                      : tr("Saves imported %1").arg(when)},
            {QStringLiteral("text"),
             bytes < 0 ? tr("From %1. What you had is moved aside: %2.").arg(file, places)
                       : tr("From %1. What you had is moved aside: %2, %3.").arg(file, places, sizeText(bytes))},
            {QStringLiteral("paths"), paths},
            {QStringLiteral("more"), int(import.paths.size() - paths.size())},
            {QStringLiteral("places"), places},
            {QStringLiteral("folder"), folderOf(import.paths.first())},
        });
    }
    return out;
}

void Backups::dismissNotice()
{
    const int index = noticed();
    if (index < 0)
        return;
    for (MovedAside::Import &import : m_imports)
        import.notice = false;
    saveImports();
    loadImports();
}

QString Backups::removeImport(int index)
{
    if (index < 0 || index >= m_imports.size())
        return {};
    const QString error = MovedAside::remove(m_imports[index].paths);
    m_imports[index].paths = MovedAside::present({m_imports[index]}).value(0).paths;
    if (m_imports[index].paths.isEmpty())
        m_imports[index].notice = false;
    saveImports();
    loadImports();
    return error;
}

QString Backups::deleteMovedAside(int index)
{
    return removeImport(index);
}

QString Backups::deleteNoticed()
{
    return removeImport(noticed());
}
