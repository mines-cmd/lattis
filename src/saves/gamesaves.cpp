#include "gamesaves.h"
#include "backupplan.h"
#include "dateformat.h"
#include "movedaside.h"
#include "savesmanifest.h"
#include "searchtext.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QLocale>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <array>

namespace {
constexpr int kMostResults = 30;

QString matchName(SavesFinder::Match match)
{
    switch (match) {
    case SavesFinder::Match::None: return {};
    case SavesFinder::Match::SteamId: return QStringLiteral("steam");
    case SavesFinder::Match::Title: return QStringLiteral("title");
    case SavesFinder::Match::Chosen: return QStringLiteral("chosen");
    case SavesFinder::Match::Epic: return QStringLiteral("epic");
    }
    return {};
}

QString whenText(const QString &archive)
{
    const QString stamp = QFileInfo(archive).fileName().left(17);
    const QDateTime when = QDateTime::fromString(stamp, QStringLiteral("yyyy-MM-dd-HHmmss"));
    if (!when.isValid())
        return QFileInfo(archive).fileName();
    const QDate today = QDate::currentDate();
    const QString day = when.date() == today ? QObject::tr("Today")
                      : when.date() == today.addDays(-1) ? QObject::tr("Yesterday")
                      : DateFormat::shortDate(when.date());
    return QStringLiteral("%1, %2").arg(day, DateFormat::shortTime(when.time()));
}

QString sizeText(qint64 bytes)
{
    return QLocale().formattedDataSize(qMax<qint64>(bytes, 0), 1, QLocale::DataSizeTraditionalFormat);
}
}

GameSaves *GameSaves::instance()
{
    static GameSaves *s_instance = nullptr;
    if (!s_instance) {
        s_instance = new GameSaves(s_setup, QCoreApplication::instance());
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    }
    return s_instance;
}

GameSaves::GameSaves(const Setup &setup, QObject *parent) : QObject(parent), m_setup(setup) {}

GameSaves::~GameSaves() = default;

SavesFinder::Found GameSaves::find(const SavesBackup::Input &input, std::shared_ptr<const SavesIndex> *index)
{
    if (!input.emulatorId.isEmpty()) {
        SavesFinder::Found found;
        found.paths = input.emulatorPaths;
        return found;
    }
    if (!*index) {
        if (std::optional<SavesIndex> loaded = SavesManifest::loadIndex(input.manifestDir))
            *index = std::make_shared<const SavesIndex>(std::move(*loaded));
    }
    const SavesIndex none;
    const SavesFinder::Matcher matcher(*index ? **index : none);
    return SavesFinder::find(matcher, input.copies, input.env);
}

GameSaves::Loaded GameSaves::read(const SavesBackup::Input &input, std::shared_ptr<const SavesIndex> index)
{
    Loaded out;
    out.index = std::move(index);
    const bool emulator = !input.emulatorId.isEmpty();
    const SavesFinder::Found found = find(input, &out.index);

    const QString home = input.env.home;
    QVariantList paths;
    for (const QString &path : found.paths)
        paths.append(BackupPlan::tilde(path, home));
    const bool fallback = found.paths.isEmpty() && !input.protonPrefix.isEmpty();
    if (fallback)
        paths.append(BackupPlan::tilde(input.protonPrefix, home));

    const QString id = SavesBackup::backupKey(input);
    QVariantList backups;
    for (const QString &archive : SavesBackup::backups(input.options.folder, id)) {
        backups.append(QVariantMap{{QStringLiteral("path"), archive},
                                   {QStringLiteral("when"), whenText(archive)},
                                   {QStringLiteral("name"), SavesBackup::backupName(archive)},
                                   {QStringLiteral("size"), sizeText(QFileInfo(archive).size())}});
    }
    out.state = {
        {QStringLiteral("id"), id},
        {QStringLiteral("eligible"), true},
        {QStringLiteral("emulator"), emulator ? input.title : QString()},
        {QStringLiteral("title"), input.title},
        {QStringLiteral("hasManifest"), out.index && !out.index->games.isEmpty()},
        {QStringLiteral("matched"), found.matched},
        {QStringLiteral("match"), matchName(found.match)},
        {QStringLiteral("paths"), paths},
        {QStringLiteral("cloudSync"), found.cloudSync},
        {QStringLiteral("fallback"), fallback},
        {QStringLiteral("backups"), backups},
        {QStringLiteral("folder"), BackupPlan::tilde(input.options.folder + u'/' + id, home)},
        {QStringLiteral("keep"), input.options.keep},
    };
    return out;
}

void GameSaves::open(const QString &gameId)
{
    m_openGame = gameId;
    setMessage({});
    load(gameId);
}

void GameSaves::load(const QString &gameId)
{
    const int token = ++m_token;
    const std::optional<SavesBackup::Input> input = m_setup.input ? m_setup.input(gameId) : std::nullopt;
    if (!input) {
        m_state = {{QStringLiteral("requested"), gameId}, {QStringLiteral("eligible"), false}};
        emit stateChanged();
        return;
    }
    auto *watcher = new QFutureWatcher<Loaded>(this);
    connect(watcher, &QFutureWatcher<Loaded>::finished, this, [this, watcher, token, gameId] {
        Loaded loaded = watcher->result();
        watcher->deleteLater();
        if (loaded.index)
            m_index = loaded.index;
        if (token != m_token)
            return;
        loaded.state.insert(QStringLiteral("requested"), gameId);
        m_state = loaded.state;
        emit stateChanged();
    });
    watcher->setFuture(QtConcurrent::run([in = *input, index = m_index] { return read(in, index); }));
}

void GameSaves::findPaths(const QString &gameId, std::function<void(const QStringList &paths)> done)
{
    const std::optional<SavesBackup::Input> input = m_setup.input ? m_setup.input(gameId) : std::nullopt;
    if (!input) {
        done({});
        return;
    }
    struct Result
    {
        QStringList paths;
        std::shared_ptr<const SavesIndex> index;
    };
    auto *watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, done = std::move(done)] {
        const Result result = watcher->result();
        watcher->deleteLater();
        if (result.index)
            m_index = result.index;
        done(result.paths);
    });
    watcher->setFuture(QtConcurrent::run([in = *input, index = m_index]() mutable {
        const SavesFinder::Found found = find(in, &index);
        return Result{found.paths, index};
    }));
}

void GameSaves::loadLast(const QString &gameId)
{
    const int token = ++m_lastToken;
    m_lastGame = gameId;
    const std::optional<SavesBackup::Input> input = m_setup.input ? m_setup.input(gameId) : std::nullopt;
    auto set = [this, gameId](QVariantMap last) {
        last.insert(QStringLiteral("requested"), gameId);
        m_lasts.insert(gameId, last);
        if (last == m_last)
            return;
        m_last = last;
        emit lastChanged();
    };
    if (!input) {
        set({});
        return;
    }
    if (m_lasts.contains(gameId))
        set(m_lasts.value(gameId));
    const QString id = SavesBackup::backupKey(*input);
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, token, set] {
        const QVariantMap last = watcher->result();
        watcher->deleteLater();
        if (token == m_lastToken)
            set(last);
    });
    watcher->setFuture(QtConcurrent::run([folder = input->options.folder, id] {
        const QStringList all = SavesBackup::backups(folder, id);
        if (all.isEmpty())
            return QVariantMap();
        return QVariantMap{{QStringLiteral("when"), whenText(all.first())},
                           {QStringLiteral("size"), sizeText(QFileInfo(all.first()).size())},
                           {QStringLiteral("name"), SavesBackup::backupName(all.first())}};
    }));
}

void GameSaves::refreshLast()
{
    const QString id = m_lastGame;
    if (!id.isEmpty())
        loadLast(id);
}

QVariantMap GameSaves::lastFor(const QString &gameId) const
{
    return m_lasts.value(gameId, {{QStringLiteral("pending"), true}});
}

QString GameSaves::rename(const QString &gameId, const QString &archive, const QString &name)
{
    const std::optional<SavesBackup::Input> input = m_setup.input ? m_setup.input(gameId) : std::nullopt;
    if (!input)
        return {};
    const QString id = SavesBackup::backupKey(*input);
    if (!SavesBackup::backups(input->options.folder, id).contains(archive))
        return tr("That backup is gone.");
    const QString error = SavesBackup::setBackupName(archive, name);
    load(gameId);
    refreshLast();
    return error;
}

QString GameSaves::remove(const QString &gameId, const QString &archive)
{
    const std::optional<SavesBackup::Input> input = m_setup.input ? m_setup.input(gameId) : std::nullopt;
    if (!input)
        return {};
    const QString id = SavesBackup::backupKey(*input);
    if (!SavesBackup::backups(input->options.folder, id).contains(archive))
        return tr("That backup is gone.");
    QString error;
    const bool trashed = m_setup.trash ? m_setup.trash(archive, &error) : MovedAside::moveToTrash(archive, &error);
    if (!trashed)
        return tr("Couldn't delete the backup: %1").arg(error);
    QFile::remove(archive + QStringLiteral(".name"));
    load(gameId);
    refreshLast();
    return {};
}

void GameSaves::backUpAll()
{
    if (m_working || !m_setup.playedInputs)
        return;
    QList<SavesBackup::Input> inputs = m_setup.playedInputs();
    if (inputs.isEmpty()) {
        setMessage(tr("No played games with saves to back up."));
        emit backedUpAll();
        return;
    }
    m_working = true;
    setMessage({});
    emit workingChanged();
    auto *watcher = new QFutureWatcher<std::array<int, 3>>(this);
    connect(watcher, &QFutureWatcher<std::array<int, 3>>::finished, this, [this, watcher] {
        const std::array<int, 3> counts = watcher->result();
        watcher->deleteLater();
        m_working = false;
        const int written = counts[0];
        const int failed = counts[1];
        const int unchanged = counts[2];
        if (failed > 0)
            setMessage(tr("%n game(s) couldn't be backed up.", "", failed));
        else if (written > 0)
            setMessage(tr("Backed up the saves of %n game(s).", "", written));
        else if (unchanged > 0)
            setMessage(tr("The saves haven't changed since the last backup."));
        else
            setMessage(tr("No saves found to back up."));
        emit workingChanged();
        emit backedUpAll();
        refreshLast();
    });
    // One after another: games of one emulator share a backup folder.
    watcher->setFuture(QtConcurrent::run([inputs = std::move(inputs)] {
        std::array<int, 3> counts{};
        for (const SavesBackup::Input &input : inputs) {
            switch (SavesBackup::run(input).status) {
            case SavesBackup::Status::Written: ++counts[0]; break;
            case SavesBackup::Status::Failed:
            case SavesBackup::Status::TooBig: ++counts[1]; break;
            case SavesBackup::Status::Unchanged: ++counts[2]; break;
            case SavesBackup::Status::Nothing: break;
            }
        }
        return counts;
    }));
}

void GameSaves::backUpNow(const QString &gameId)
{
    if (m_working)
        return;
    std::optional<SavesBackup::Input> input = m_setup.input ? m_setup.input(gameId) : std::nullopt;
    if (!input) {
        emit backedUp(gameId, tr("This game's saves can't be backed up."));
        return;
    }
    m_working = true;
    setMessage({});
    emit workingChanged();
    auto *watcher = new QFutureWatcher<SavesBackup::Result>(this);
    connect(watcher, &QFutureWatcher<SavesBackup::Result>::finished, this, [this, watcher, gameId] {
        const SavesBackup::Result result = watcher->result();
        watcher->deleteLater();
        m_working = false;
        const bool stillOpen = gameId == m_openGame;
        QString text;
        switch (result.status) {
        case SavesBackup::Status::Written:
            text = tr("Backed up %n file(s), %1.", "", result.files).arg(sizeText(result.bytes));
            break;
        case SavesBackup::Status::Unchanged:
            text = tr("The saves haven't changed since the last backup.");
            break;
        case SavesBackup::Status::TooBig:
            text = tr("The saves come to %1, over the limit in Settings → Library.").arg(sizeText(result.bytes));
            break;
        case SavesBackup::Status::Failed:
            text = tr("Couldn't back up: %1").arg(result.error);
            break;
        case SavesBackup::Status::Nothing:
            text = tr("No saves found to back up.");
            break;
        }
        if (stillOpen)
            setMessage(text);
        emit workingChanged();
        if (stillOpen)
            load(gameId);
        refreshLast();
        emit backedUp(gameId, text);
    });
    watcher->setFuture(QtConcurrent::run([in = std::move(*input)] { return SavesBackup::run(in); }));
}

QVariantList GameSaves::search(const QString &text) const
{
    QVariantList out;
    const QStringList words = SearchText::words(text);
    if (!m_index || words.isEmpty())
        return out;
    if (m_foldedFor != m_index.get()) {
        m_folded.clear();
        m_folded.reserve(m_index->games.size());
        for (const SavesGame &game : m_index->games)
            m_folded.append(SearchText::fold(game.name));
        m_foldedFor = m_index.get();
    }
    for (qsizetype i = 0; i < m_folded.size() && out.size() < kMostResults; ++i) {
        if (SearchText::matches(m_folded.at(i), words))
            out.append(m_index->games.at(i).name);
    }
    return out;
}

QString GameSaves::choose(const QString &gameId, const QString &name)
{
    if (!m_setup.choose)
        return {};
    const QString error = m_setup.choose(gameId, name);
    if (error.isEmpty())
        load(gameId);
    return error;
}

void GameSaves::setMessage(const QString &text)
{
    if (m_message == text)
        return;
    m_message = text;
    emit messageChanged();
}
