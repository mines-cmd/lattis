#include "general.h"
#include "autostart.h"
#include "tomlpatch.h"

#include <QFile>
#include <QJSEngine>
#include <QLoggingCategory>

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
    m_problems = parsed.warnings;

    // The main window's visibility bindings listen to settingsChanged; re-evaluating them makes Qt
    // show the window again, which on some compositors (Hyprland) unmaps the settings window above it.
    // systemLocale, reduceMotion, railHidden and railHiddenButtons and railButtonOrder have their own signals so that flipping them leaves the
    // window alone.
    GeneralSettings prevWindow = prev;
    prevWindow.systemLocale = m_settings.systemLocale;
    prevWindow.reduceMotion = m_settings.reduceMotion;
    prevWindow.railHidden = m_settings.railHidden;
    prevWindow.railHiddenButtons = m_settings.railHiddenButtons;
    prevWindow.railButtonOrder = m_settings.railButtonOrder;
    if (m_settings != prevWindow || m_problems != prevProblems)
        emit settingsChanged();
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
