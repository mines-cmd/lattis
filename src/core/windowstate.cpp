#include "windowstate.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QProcess>
#include <QSaveFile>
#include <QScreen>

#include <toml++/toml.hpp>

#include <sstream>

Q_LOGGING_CATEGORY(lcWindowState, "launcher.windowstate")

WindowState::WindowState(const QString &stateDir, const QString &dataDirKey, QObject *parent)
    : QObject(parent)
    , m_file(QStringLiteral("%1/windows-%2.toml").arg(stateDir, dataDirKey))
{
    s_instance = this;

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(500);
    connect(&m_saveTimer, &QTimer::timeout, this, [this] {
        capture();
        save();
    });
    if (qGuiApp) {
        connect(qGuiApp, &QGuiApplication::aboutToQuit, this, [this] {
            m_saveTimer.stop();
            capture();
            save();
        });
    }

    load();
}

WindowState::~WindowState()
{
    if (s_instance == this)
        s_instance = nullptr;
}

WindowState *WindowState::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

int WindowState::width(const QString &name, int fallback) const
{
    const QSize size = m_entries.value(name).size;
    return size.isValid() ? size.width() : fallback;
}

int WindowState::height(const QString &name, int fallback) const
{
    const QSize size = m_entries.value(name).size;
    return size.isValid() ? size.height() : fallback;
}

bool WindowState::maximized(const QString &name) const
{
    return m_entries.value(name).maximized;
}

bool WindowState::canMinimize()
{
    static const bool can = [] {
        const QStringList desktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower().split(u':');
        for (const char *name : {"hyprland", "sway", "niri", "river"}) {
            if (desktops.contains(QLatin1StringView(name)))
                return false;
        }
        return true;
    }();
    return can;
}

void WindowState::track(QWindow *window, const QString &name)
{
    if (!window)
        return;
    m_windows.insert(name, window);
    const auto changed = [this] { m_saveTimer.start(); };
    connect(window, &QWindow::widthChanged, this, changed);
    connect(window, &QWindow::heightChanged, this, changed);
    connect(window, &QWindow::windowStateChanged, this, changed);
}

void WindowState::load()
{
    toml::table table;
    try {
        table = toml::parse_file(QDir::toNativeSeparators(m_file).toStdString());
    } catch (const toml::parse_error &) {
        return;
    }
    for (const auto &[key, node] : table) {
        const toml::table *section = node.as_table();
        if (!section)
            continue;
        Entry entry;
        const auto w = (*section)["width"].value<int>();
        const auto h = (*section)["height"].value<int>();
        if (w && h && *w > 0 && *h > 0)
            entry.size = QSize(*w, *h);
        entry.maximized = (*section)["maximized"].value_or(false);
        m_entries.insert(QString::fromUtf8(key.str().data(), qsizetype(key.str().size())), entry);
    }
}

// Tiling compositors mark every window maximized to turn off client-side decorations;
// Hyprland even marks floating ones. So a window only counts as maximized if it also fills
// its screen's width and, but for a server-side title bar, its height; tiling leaves gaps.
bool WindowState::fillsScreen(const QWindow *window)
{
    const QScreen *screen = window ? window->screen() : nullptr;
    if (!screen)
        return false;
    const QSize frame = window->frameGeometry().size();
    const QSize available = screen->availableGeometry().size();
    return frame.width() >= available.width() && frame.height() >= available.height() - 64;
}

int WindowState::compositorRounding()
{
    static const int rounding = [] {
        if (qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE"))
            return -1;
        QProcess hyprctl;
        hyprctl.start(QStringLiteral("hyprctl"), {QStringLiteral("-j"), QStringLiteral("getoption"), QStringLiteral("decoration:rounding")});
        if (!hyprctl.waitForFinished(500) || hyprctl.exitCode() != 0)
            return -1;
        const QJsonValue value = QJsonDocument::fromJson(hyprctl.readAllStandardOutput()).object().value(QStringLiteral("int"));
        return value.isDouble() ? qMax(0, value.toInt()) : -1;
    }();
    return rounding;
}

// Reads the tracked windows' current state. Only a normal window's size is kept, so
// unmaximizing after a restart returns to the size it had before. Minimized and fullscreen
// windows change nothing.
void WindowState::capture()
{
    for (auto it = m_windows.cbegin(); it != m_windows.cend(); ++it) {
        const QWindow *window = it.value();
        if (!window)
            continue;
        const Qt::WindowStates states = window->windowStates();
        if (states & (Qt::WindowMinimized | Qt::WindowFullScreen))
            continue;
        Entry &entry = m_entries[it.key()];
        entry.maximized = (states & Qt::WindowMaximized) && fillsScreen(window);
        if (!entry.maximized)
            entry.size = window->size();
        qCDebug(lcWindowState) << it.key() << states << window->size() << "maximized" << entry.maximized;
    }
}

void WindowState::save()
{
    toml::table table;
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        toml::table section;
        if (it->size.isValid()) {
            section.insert("width", it->size.width());
            section.insert("height", it->size.height());
        }
        section.insert("maximized", it->maximized);
        table.insert(it.key().toStdString(), std::move(section));
    }
    std::ostringstream out;
    out << "# Window sizes, remembered between starts. Safe to delete.\n\n" << table << '\n';
    const std::string text = out.str();

    if (!QDir().mkpath(QFileInfo(m_file).absolutePath())) {
        qCWarning(lcWindowState) << "can't create" << QFileInfo(m_file).absolutePath();
        return;
    }
    QSaveFile file(m_file);
    if (!file.open(QIODevice::WriteOnly) || file.write(text.data(), qint64(text.size())) < 0 || !file.commit())
        qCWarning(lcWindowState) << "can't save" << m_file << file.errorString();
}
