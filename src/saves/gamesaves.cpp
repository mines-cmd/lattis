#include "gamesaves.h"
#include "backupplan.h"
#include "dateformat.h"
#include "savesmanifest.h"
#include "searchtext.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QJSEngine>
#include <QLocale>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace {
constexpr int kMostResults = 30;

QString matchName(SavesFinder::Match match)
{
    switch (match) {
    case SavesFinder::Match::None: return {};
    case SavesFinder::Match::SteamId: return QStringLiteral("steam");
    case SavesFinder::Match::Title: return QStringLiteral("title");
    case SavesFinder::Match::Chosen: return QStringLiteral("chosen");
    }
    return {};
}

QString whenText(const QString &archive)
{
    const QString stamp = QFileInfo(archive).fileName().left(17);
    const QDateTime when = QDateTime::fromString(stamp, QStringLiteral("yyyy-MM-dd-HHmmss"));
    if (!when.isValid())
        return QFileInfo(archive).fileName();
    return QStringLiteral("%1, %2").arg(DateFormat::shortDate(when.date()), QLocale().toString(when.time(), QLocale::ShortFormat));
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

GameSaves::Loaded GameSaves::read(const SavesBackup::Input &input, std::shared_ptr<const SavesIndex> index)
{
    Loaded out;
    out.index = std::move(index);
    if (!out.index) {
        if (std::optional<SavesIndex> loaded = SavesManifest::loadIndex(input.manifestDir))
            out.index = std::make_shared<const SavesIndex>(std::move(*loaded));
    }
    const SavesIndex none;
    const SavesFinder::Matcher matcher(out.index ? *out.index : none);
    const SavesFinder::Found found = SavesFinder::find(matcher, input.copies, input.env);

    const QString home = input.env.home;
    QVariantList paths;
    for (const QString &path : found.paths)
        paths.append(BackupPlan::tilde(path, home));
    const bool fallback = found.paths.isEmpty() && !input.protonPrefix.isEmpty();
    if (fallback)
        paths.append(BackupPlan::tilde(input.protonPrefix, home));

    const QString id = input.copies.isEmpty() ? input.gameId : input.copies.first().id;
    QVariantList backups;
    for (const QString &archive : SavesBackup::backups(input.options.folder, id)) {
        backups.append(QVariantMap{{QStringLiteral("path"), archive},
                                   {QStringLiteral("when"), whenText(archive)},
                                   {QStringLiteral("size"), sizeText(QFileInfo(archive).size())}});
    }
    out.state = {
        {QStringLiteral("id"), id},
        {QStringLiteral("eligible"), true},
        {QStringLiteral("title"), input.title},
        {QStringLiteral("hasManifest"), out.index && !out.index->games.isEmpty()},
        {QStringLiteral("matched"), found.matched},
        {QStringLiteral("match"), matchName(found.match)},
        {QStringLiteral("paths"), paths},
        {QStringLiteral("fallback"), fallback},
        {QStringLiteral("backups"), backups},
        {QStringLiteral("folder"), BackupPlan::tilde(input.options.folder + u'/' + id, home)},
        {QStringLiteral("keep"), input.options.keep},
    };
    return out;
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

void GameSaves::backUpNow(const QString &gameId)
{
    if (m_working)
        return;
    std::optional<SavesBackup::Input> input = m_setup.input ? m_setup.input(gameId) : std::nullopt;
    if (!input)
        return;
    m_working = true;
    setMessage({});
    emit workingChanged();
    auto *watcher = new QFutureWatcher<SavesBackup::Result>(this);
    connect(watcher, &QFutureWatcher<SavesBackup::Result>::finished, this, [this, watcher, gameId] {
        const SavesBackup::Result result = watcher->result();
        watcher->deleteLater();
        m_working = false;
        switch (result.status) {
        case SavesBackup::Status::Written:
            setMessage(tr("Backed up %n file(s), %1.", "", result.files).arg(sizeText(result.bytes)));
            break;
        case SavesBackup::Status::Unchanged:
            setMessage(tr("The saves haven't changed since the last backup."));
            break;
        case SavesBackup::Status::TooBig:
            setMessage(tr("The saves come to %1, over the limit in Settings → Library.").arg(sizeText(result.bytes)));
            break;
        case SavesBackup::Status::Failed:
            setMessage(tr("Couldn't back up: %1").arg(result.error));
            break;
        case SavesBackup::Status::Nothing:
            setMessage(tr("No saves found to back up."));
            break;
        }
        emit workingChanged();
        load(gameId);
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
