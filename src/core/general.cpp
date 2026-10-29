#include "general.h"
#include "autostart.h"
#include "tomlpatch.h"

#include <QFile>
#include <QJSEngine>
#include <QLoggingCategory>

#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(lcGeneral, "lattis.general")

General::General(const QString &dataDir, QObject *parent)
    : QObject(parent), m_dataDir(dataDir)
{
    Q_ASSERT(!s_instance);
    s_instance = this;
    load();
    m_startedFullscreen = m_settings.startFullscreen;
    loadAutostart();
}

General::~General()
{
    if (s_instance == this)
        s_instance = nullptr;
}

General *General::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QString General::setFlag(const QString &key, bool value)
{
    QString error;
    if (value) {
        TomlPatch::setValueInFile(settingsFile(), QStringLiteral("general"), key, QStringLiteral("true"), &error);
    } else {
        TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("general"), key, &error);
    }
    if (error.isEmpty())
        load();
    return error;
}

QString General::setStartFullscreen(bool value)
{
    return setFlag(QStringLiteral("start_fullscreen"), value);
}

QString General::setDownloadBarCollapsed(bool value)
{
    return setFlag(QStringLiteral("download_bar_collapsed"), value);
}

QString General::setSystemLocale(bool value)
{
    QString error;
    if (value) {
        TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("general"), QStringLiteral("system_locale"), &error);
    } else {
        TomlPatch::setValueInFile(settingsFile(), QStringLiteral("general"), QStringLiteral("system_locale"),
                                  QStringLiteral("false"), &error);
    }
    if (error.isEmpty())
        load();
    return error;
}

QString General::setReduceMotion(bool value)
{
    return setFlag(QStringLiteral("reduce_motion"), value);
}

QString General::setRailHidden(bool value)
{
    return setFlag(QStringLiteral("rail_hidden"), value);
}

QString General::setCheckForUpdates(bool value)
{
    QString error;
    if (value) {
        TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("general"), QStringLiteral("check_for_updates"), &error);
    } else {
        TomlPatch::setValueInFile(settingsFile(), QStringLiteral("general"), QStringLiteral("check_for_updates"),
                                  QStringLiteral("false"), &error);
    }
    if (error.isEmpty())
        load();
    return error;
}

QString General::setSkippedUpdate(const QString &version)
{
    QString error;
    if (version.isEmpty()) {
        TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("general"), QStringLiteral("skipped_update"), &error);
    } else {
        QString quoted = version;
        quoted.replace(u'\\', QStringLiteral("\\\\")).replace(u'"', QStringLiteral("\\\""));
        TomlPatch::setValueInFile(settingsFile(), QStringLiteral("general"), QStringLiteral("skipped_update"),
                                  u'"' + quoted + u'"', &error);
    }
    if (error.isEmpty())
        load();
    return error;
}

QString General::setClipVolume(double value)
{
    value = std::clamp(std::round(value * 100) / 100, 0.0, 1.0);
    if (value == m_settings.clipVolume)
        return {};
    QString error;
    if (value == 1.0) {
        TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("general"), QStringLiteral("clip_volume"), &error);
    } else {
        TomlPatch::setValueInFile(settingsFile(), QStringLiteral("general"), QStringLiteral("clip_volume"),
                                  QString::number(value, 'f', 2), &error);
    }
    if (error.isEmpty())
        load();
    return error;
}

QString General::setRailButtonShown(const QString &name, bool shown)
{
    if (!railButtons().contains(name))
        return {};
    QStringList hidden;
    for (const QString &button : railButtons()) {
        const bool isHidden = button == name ? !shown : m_settings.railHiddenButtons.contains(button);
        if (isHidden)
            hidden.append(u'"' + button + u'"');
    }
    QString error;
    if (hidden.isEmpty())
        TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("general"), QStringLiteral("rail_hidden_buttons"), &error);
    else
        TomlPatch::setValueInFile(settingsFile(), QStringLiteral("general"), QStringLiteral("rail_hidden_buttons"),
                                  u'[' + hidden.join(QLatin1String(", ")) + u']', &error);
    if (error.isEmpty())
        load();
    return error;
}

QString General::setRailButtonOrder(const QStringList &order)
{
    QStringList sorted = order;
    QStringList known = railButtons();
    sorted.sort();
    known.sort();
    if (sorted != known)
        return {};
    QString error;
    if (order == railButtons()) {
        TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("general"), QStringLiteral("rail_button_order"), &error);
    } else {
        QStringList quoted;
        for (const QString &button : order)
            quoted.append(u'"' + button + u'"');
        TomlPatch::setValueInFile(settingsFile(), QStringLiteral("general"), QStringLiteral("rail_button_order"),
                                  u'[' + quoted.join(QLatin1String(", ")) + u']', &error);
    }
    if (error.isEmpty())
        load();
    return error;
}

QStringList General::gamePageOrder(const QString &place) const
{
    if (isGamePagePlace(place))
        return ::gamePageOrder(m_gamePage, place);
    return {};
}

QString General::writeGamePage(const QString &place, const QStringList &order, const QStringList &shown)
{
    const QStringList &choices = gamePageChoices(place);
    QStringList list;
    for (const QString &name : order)
        if (shown.contains(name))
            list.append(name);
    QStringList canonical = list;
    for (const QString &name : choices)
        if (!canonical.contains(name))
            canonical.append(name);

    const GamePageSettings defaults;
    const QStringList &defaultList = gamePageShown(defaults, place);
    const auto quoted = [](const QStringList &names) {
        QStringList out;
        for (const QString &name : names)
            out.append(u'"' + name + u'"');
        return u'[' + out.join(QLatin1String(", ")) + u']';
    };
    QString error;
    const auto write = [&](const QString &key, const QStringList &names, bool isDefault) {
        if (!error.isEmpty())
            return;
        if (isDefault)
            TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("game_page"), key, &error);
        else
            TomlPatch::setValueInFile(settingsFile(), QStringLiteral("game_page"), key, quoted(names), &error);
    };
    if (place == GamePage::sections)
        for (const char *flag : {"show_description", "show_tags", "show_clips"})
            if (error.isEmpty())
                TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("game_page"), QLatin1String(flag), &error);
    write(place, list, list == defaultList);
    write(place + QStringLiteral("_order"), order, order == canonical);
    if (error.isEmpty())
        load();
    return error;
}

QString General::setGamePageShown(const QString &place, const QString &name, bool shown)
{
    if (!isGamePagePlace(place) || !gamePageChoices(place).contains(name))
        return {};
    QStringList now;
    const QStringList order = ::gamePageOrder(m_gamePage, place);
    for (const QString &entry : order) {
        const bool on = entry == name ? shown : gamePageShown(m_gamePage, place).contains(entry);
        if (on)
            now.append(entry);
    }
    return writeGamePage(place, order, now);
}

QString General::setGamePageOrder(const QString &place, const QStringList &order)
{
    if (!isGamePagePlace(place))
        return {};
    QStringList sorted = order;
    QStringList known = gamePageChoices(place);
    sorted.sort();
    known.sort();
    if (sorted != known)
        return {};
    const QStringList &shown = gamePageShown(m_gamePage, place);
    return writeGamePage(place, order, shown);
}

QString General::setGamePageChoice(const QString &key, const QString &value, const QString &defaultValue)
{
    QString error;
    if (value == defaultValue)
        TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("game_page"), key, &error);
    else
        TomlPatch::setValueInFile(settingsFile(), QStringLiteral("game_page"), key, u'"' + value + u'"', &error);
    if (error.isEmpty())
        load();
    return error;
}

QString General::setRecentPeriod(const QString &period)
{
    if (!recentPeriods().contains(period))
        return {};
    return setGamePageChoice(QStringLiteral("recent_period"), period, GamePageSettings{}.recentPeriod);
}

QString General::setPlayTimeFormat(const QString &format)
{
    if (!playTimeFormats().contains(format))
        return {};
    return setGamePageChoice(QStringLiteral("play_time_format"), format, GamePageSettings{}.playTimeFormat);
}

QString General::setShowScreenshots(bool on)
{
    return setMediaFlag(QStringLiteral("show_screenshots"), on);
}

QString General::setShowVideo(bool on)
{
    return setMediaFlag(QStringLiteral("show_video"), on);
}

QString General::setShowAchievements(bool on)
{
    return setMediaFlag(QStringLiteral("show_achievements"), on);
}

QString General::setMediaFlag(const QString &key, bool on)
{
    QString error;
    if (on)
        TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("game_page"), key, &error);
    else
        TomlPatch::setValueInFile(settingsFile(), QStringLiteral("game_page"), key, QStringLiteral("false"), &error);
    if (error.isEmpty())
        load();
    return error;
}

QString General::setGamePageFlag(const QString &key, const QString &place, const QStringList &current,
                                 const QString &name, bool on)
{
    if (!gamePageChoices(place).contains(name))
        return {};
    QStringList names;
    for (const QString &choice : gamePageChoices(place))
        if (choice == name ? on : current.contains(choice))
            names.append(u'"' + choice + u'"');
    QString error;
    if (names.isEmpty())
        TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("game_page"), key, &error);
    else
        TomlPatch::setValueInFile(settingsFile(), QStringLiteral("game_page"), key,
                                  u'[' + names.join(QLatin1String(", ")) + u']', &error);
    if (error.isEmpty())
        load();
    return error;
}

QString General::setChipIconShown(const QString &name, bool shown)
{
    return setGamePageFlag(QStringLiteral("chips_without_icon"), GamePage::chips, m_gamePage.chipsWithoutIcon, name,
                           !shown);
}

QString General::setSectionCollapsed(const QString &name, bool collapsed)
{
    return setGamePageFlag(QStringLiteral("collapsed_sections"), GamePage::sections, m_gamePage.collapsedSections,
                           name, collapsed);
}

QString General::setGalleryValue(const QString &key, const QString &value, bool isDefault)
{
    QString error;
    if (isDefault)
        TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("gallery"), key, &error);
    else
        TomlPatch::setValueInFile(settingsFile(), QStringLiteral("gallery"), key, value, &error);
    if (error.isEmpty())
        load();
    return error;
}

QString General::setGalleryGroup(const QString &group)
{
    if (!galleryGroups().contains(group))
        return {};
    return setGalleryValue(QStringLiteral("group"), u'"' + group + u'"', group == GallerySettings{}.group);
}

QString General::setGallerySort(const QString &sort)
{
    if (!gallerySorts().contains(sort))
        return {};
    return setGalleryValue(QStringLiteral("sort"), u'"' + sort + u'"', sort == GallerySettings{}.sort);
}

QString General::setGalleryTileWidth(int width)
{
    width = std::clamp(width, 80, 1000);
    return setGalleryValue(QStringLiteral("tile_width"), QString::number(width), false);
}

QString General::setGalleryCaptions(bool on)
{
    return setGalleryValue(QStringLiteral("captions"), on ? QStringLiteral("true") : QStringLiteral("false"),
                           on == GallerySettings{}.captions);
}

QString General::setGallerySectionFolded(const QString &name, bool folded)
{
    if (!gallerySections().contains(name))
        return {};
    QStringList names;
    for (const QString &section : gallerySections())
        if (section == name ? folded : m_gallery.foldedSections.contains(section))
            names.append(u'"' + section + u'"');
    return setGalleryValue(QStringLiteral("folded_sections"), u'[' + names.join(QLatin1String(", ")) + u']',
                           names.isEmpty());
}

QString General::setStartAtLogin(bool on)
{
    const QString folder = Autostart::autostartFolder();
    QString error;
    if (on) {
        error = Autostart::writeEntry(folder, Autostart::currentProgram(), m_dataDir);
    } else {
        error = Autostart::removeEntry(folder);
    }
    if (error.isEmpty())
        loadAutostart();
    return error;
}

void General::reload()
{
    load();
    loadAutostart();
}

void General::refreshAutostart()
{
    loadAutostart();
    if (!m_loginExists || !m_loginOurs)
        return;
    const QString expected = Autostart::entryExec(Autostart::currentProgram(), m_dataDir);
    if (m_loginExec == expected)
        return;
    qCInfo(lcGeneral) << "rewriting stale autostart entry";
    Autostart::writeEntry(Autostart::autostartFolder(), Autostart::currentProgram(), m_dataDir);
    loadAutostart();
}

void General::load()
{
    QFile file(settingsFile());
    QByteArray bytes;
    if (file.open(QIODevice::ReadOnly))
        bytes = file.readAll();

    const auto parsed = parseGeneralSettings(bytes, QStringLiteral("settings.toml"));
    const GeneralSettings prev = m_settings;
    const QStringList prevProblems = m_problems;
    m_settings = parsed.value;
    const auto page = parseGamePageSettings(bytes, QStringLiteral("settings.toml"));
    const GamePageSettings prevPage = m_gamePage;
    const QStringList prevPageProblems = m_gamePageProblems;
    m_gamePage = page.value;
    m_gamePageProblems = page.ok ? page.warnings : QStringList();
    const auto gallery = parseGallerySettings(bytes, QStringLiteral("settings.toml"));
    const GallerySettings prevGallery = m_gallery;
    const QStringList prevGalleryProblems = m_galleryProblems;
    m_gallery = gallery.value;
    m_galleryProblems = gallery.ok ? gallery.warnings : QStringList();
    m_problems = parsed.warnings + m_gamePageProblems + m_galleryProblems;

    // The main window's visibility bindings listen to settingsChanged; re-evaluating them makes Qt
    // show the window again, which on some compositors (Hyprland) unmaps the settings window above it.
    // systemLocale, reduceMotion, railHidden and railHiddenButtons, railButtonOrder and clipVolume have their own signals so that
    // flipping them leaves the window alone.
    GeneralSettings prevWindow = prev;
    prevWindow.clipVolume = m_settings.clipVolume;
    prevWindow.systemLocale = m_settings.systemLocale;
    prevWindow.reduceMotion = m_settings.reduceMotion;
    prevWindow.railHidden = m_settings.railHidden;
    prevWindow.railHiddenButtons = m_settings.railHiddenButtons;
    prevWindow.railButtonOrder = m_settings.railButtonOrder;
    if (m_settings != prevWindow || m_problems != prevProblems)
        emit settingsChanged();
    if (m_gamePage != prevPage || m_gamePageProblems != prevPageProblems)
        emit gamePageChanged();
    if (m_gallery != prevGallery)
        emit galleryChanged();
    if (m_settings.systemLocale != prev.systemLocale)
        emit systemLocaleChanged();
    if (m_settings.reduceMotion != prev.reduceMotion)
        emit reduceMotionChanged();
    if (m_settings.railHidden != prev.railHidden)
        emit railHiddenChanged();
    if (m_settings.railHiddenButtons != prev.railHiddenButtons)
        emit railHiddenButtonsChanged();
    if (orderedRailButtons(m_settings.railButtonOrder) != orderedRailButtons(prev.railButtonOrder))
        emit railButtonOrderChanged();
    if (m_settings.clipVolume != prev.clipVolume)
        emit clipVolumeChanged();
    for (const QString &w : std::as_const(m_problems))
        qCWarning(lcGeneral).noquote() << w;
}

void General::loadAutostart()
{
    const Autostart::Entry entry = Autostart::readEntry(Autostart::autostartFolder());
    const bool prevExists = m_loginExists;
    const bool prevOurs = m_loginOurs;
    const bool prevEnabled = m_loginEnabled;
    m_loginExists = entry.exists;
    m_loginOurs = entry.ours;
    m_loginEnabled = entry.enabled;
    m_loginExec = entry.exec;

    if (m_loginExists != prevExists || m_loginOurs != prevOurs || m_loginEnabled != prevEnabled)
        emit loginChanged();
}
