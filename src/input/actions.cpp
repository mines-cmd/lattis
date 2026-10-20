#include "actions.h"
#include "tomlsection.h"
#include "actionschema.h"
#include "suggestion.h"
#include "tomlpatch.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJSEngine>
#include <QKeySequence>
#include <QLoggingCategory>

#include <toml++/toml.hpp>

Q_LOGGING_CATEGORY(lcActions, "launcher.actions")

using ActionSchema::Action;
using ActionSchema::Scope;

namespace {
QString scopeName(Scope scope)
{
    switch (scope) {
    case Scope::Main: return QStringLiteral("main");
    case Scope::Settings: return QStringLiteral("settings");
    case Scope::App: return QStringLiteral("app");
    }
    return {};
}

Scope scopeOf(const Action &a)
{
    const ActionSchema::Group *g = ActionSchema::groupOf(a);
    return g ? g->scope : Scope::Main;
}

bool scopesOverlap(Scope a, Scope b)
{
    return a == b || a == Scope::App || b == Scope::App;
}

const Action *findAction(const QString &name)
{
    for (const Action &a : ActionSchema::actions()) {
        if (toQString(a.name) == name)
            return &a;
    }
    return nullptr;
}

QString canonical(const QString &text)
{
    const QKeySequence seq = QKeySequence::fromString(text.trimmed(), QKeySequence::PortableText);
    if (seq.isEmpty())
        return {};
    for (int i = 0; i < seq.count(); ++i) {
        if (seq[i].key() == Qt::Key_unknown)
            return {};
    }
    return seq.toString(QKeySequence::PortableText);
}

QString quoted(const QString &s)
{
    QString escaped = s;
    escaped.replace(u'\\', QLatin1String("\\\\")).replace(u'"', QLatin1String("\\\""));
    return u'"' + escaped + u'"';
}
}

ActionListModel::ActionListModel(Actions *actions)
    : QAbstractListModel(actions)
    , m_actions(actions)
{
    m_visible = visibleRows();
    connect(m_actions, &Actions::reloaded, this, [this] {
        const QList<int> rows = visibleRows();
        if (rows == m_visible) {
            if (rowCount() > 0)
                emit dataChanged(index(0), index(rowCount() - 1));
            return;
        }
        beginResetModel();
        m_visible = rows;
        endResetModel();
        emit sectionsChanged();
    });
}

int ActionListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_visible.size());
}

bool ActionListModel::sameGroup(int row, int otherRow) const
{
    if (otherRow < 0 || otherRow >= m_visible.size())
        return false;
    const auto all = ActionSchema::actions();
    return all[m_visible[row]].group == all[m_visible[otherRow]].group;
}

QVariant ActionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_visible.size())
        return {};
    const Action &a = ActionSchema::actions()[m_visible[index.row()]];
    const QString name = toQString(a.name);

    switch (role) {
    case NameRole: return name;
    case GroupRole: return toQString(a.group);
    case GroupTitleRole: {
        const ActionSchema::Group *g = ActionSchema::groupOf(a);
        return g ? toQString(g->title) : QString();
    }
    case TitleRole: return toQString(a.title);
    case KeysRole: return m_actions->keysOf(name);
    case KeyLabelsRole: return Actions::labels(m_actions->keysOf(name));
    case DefaultLabelsRole: return Actions::labels(Actions::defaultKeys(name));
    case IsCustomRole: return m_actions->isCustom(name);
    case ConflictRole: return m_actions->hasConflict(name);
    case FirstInGroupRole: return !sameGroup(index.row(), index.row() - 1);
    case LastInGroupRole: return !sameGroup(index.row(), index.row() + 1);
    }
    return {};
}

QHash<int, QByteArray> ActionListModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {GroupRole, "group"},
        {GroupTitleRole, "groupTitle"},
        {TitleRole, "title"},
        {KeysRole, "keys"},
        {KeyLabelsRole, "keyLabels"},
        {DefaultLabelsRole, "defaultLabels"},
        {IsCustomRole, "isCustom"},
        {ConflictRole, "conflict"},
        {FirstInGroupRole, "firstInGroup"},
        {LastInGroupRole, "lastInGroup"},
    };
}

void ActionListModel::setFilterText(const QString &text)
{
    if (text == m_filterText)
        return;
    m_filterText = text;
    emit filterTextChanged();
    beginResetModel();
    m_visible = visibleRows();
    endResetModel();
    emit sectionsChanged();
}

QList<int> ActionListModel::visibleRows() const
{
    QList<int> rows;
    const auto all = ActionSchema::actions();
    for (int i = 0; i < int(all.size()); ++i) {
        const Action &a = all[i];
        const QString name = toQString(a.name);
        if (!m_filterText.isEmpty()) {
            const ActionSchema::Group *g = ActionSchema::groupOf(a);
            const QString haystack = QStringList{name, toQString(a.title), g ? toQString(g->title) : QString(),
                                                 Actions::labels(m_actions->keysOf(name)).join(u' ')}
                                         .join(u' ');
            if (!haystack.contains(m_filterText, Qt::CaseInsensitive))
                continue;
        }
        rows.append(i);
    }
    return rows;
}

QVariantList ActionListModel::sections() const
{
    QVariantList out;
    const auto all = ActionSchema::actions();
    std::string_view last;
    for (const int i : m_visible) {
        if (all[i].group == last)
            continue;
        last = all[i].group;
        const ActionSchema::Group *g = ActionSchema::groupOf(all[i]);
        out.append(QVariantMap{{QStringLiteral("name"), toQString(all[i].group)},
                               {QStringLiteral("title"), g ? toQString(g->title) : QString()}});
    }
    return out;
}

int ActionListModel::firstRow(const QString &group) const
{
    const auto all = ActionSchema::actions();
    for (int row = 0; row < m_visible.size(); ++row) {
        if (toQString(all[m_visible[row]].group) == group)
            return row;
    }
    return -1;
}

Actions::Actions(QString dataDir, QObject *parent)
    : QObject(parent)
    , m_dataDir(std::move(dataDir))
    , m_keys(new ActionKeys(this))
{
    s_instance = this;

    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(100);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_reloadTimer, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_reloadTimer, qOverload<>(&QTimer::start));
    connect(&m_reloadTimer, &QTimer::timeout, this, [this] {
        reload();
        watch();
    });

    if (qGuiApp)
        connect(qGuiApp, &QGuiApplication::focusWindowChanged, this, &Actions::focusWindowChanged);

    migrateRenamedActions();
    reload();
    m_model = new ActionListModel(this);
    watch();
}

void Actions::migrateRenamedActions()
{
    static const std::pair<const char *, const char *> kRenamed[] = {
        {"new_theme", "new_colors"},
        {"open_theme_file", "open_colors_file"},
    };

    toml::table tbl;
    try {
        tbl = toml::parse_file(settingsFile().toStdString());
    } catch (const toml::parse_error &) {
        return;
    }
    const toml::table *section = tbl["shortcuts"].as_table();
    if (!section)
        return;

    for (const auto &[oldName, newName] : kRenamed) {
        const toml::node *node = section->get(oldName);
        if (!node)
            continue;
        if (!section->contains(newName)) {
            QString value;
            if (auto text = node->value<std::string>()) {
                value = quoted(QString::fromStdString(*text));
            } else if (const toml::array *list = node->as_array()) {
                QStringList items;
                for (const toml::node &item : *list) {
                    if (auto text = item.value<std::string>())
                        items.append(quoted(QString::fromStdString(*text)));
                }
                value = u'[' + items.join(QLatin1String(", ")) + u']';
            } else {
                continue;
            }
            QString error;
            if (!TomlPatch::setValueInFile(settingsFile(), QStringLiteral("shortcuts"), QString::fromLatin1(newName), value, &error)) {
                warn(QStringLiteral("could not move shortcut %1 to %2: %3").arg(QString::fromLatin1(oldName), QString::fromLatin1(newName), error));
                continue;
            }
        }
        QString error;
        TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("shortcuts"), QString::fromLatin1(oldName), &error);
    }
}

QWindow *Actions::focusWindow() const
{
    return QGuiApplication::focusWindow();
}

Actions::~Actions()
{
    if (s_instance == this)
        s_instance = nullptr;
}

Actions *Actions::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QString Actions::settingsFile() const
{
    return m_dataDir + QStringLiteral("/settings.toml");
}

void Actions::setRecording(bool recording)
{
    if (recording == m_recording)
        return;
    m_recording = recording;
    emit recordingChanged();
}

void Actions::warn(const QString &message)
{
    qCWarning(lcActions).noquote() << message;
    m_problems.append(message);
}

QStringList Actions::defaultKeys(const QString &name)
{
    const Action *a = findAction(name);
    return a ? toQString(a->keys).split(u' ', Qt::SkipEmptyParts) : QStringList();
}

QStringList Actions::labels(const QStringList &keys)
{
    QStringList out;
    for (const QString &k : keys)
        out.append(QKeySequence::fromString(k, QKeySequence::PortableText).toString(QKeySequence::NativeText));
    return out;
}

void Actions::reload()
{
    m_problems.clear();

    QHash<QString, QStringList> bindings;
    QSet<QString> custom;
    QStringList names;
    for (const Action &a : ActionSchema::actions()) {
        names.append(toQString(a.name));
        bindings.insert(names.last(), defaultKeys(names.last()));
    }

    toml::table tbl;
    try {
        tbl = toml::parse_file(settingsFile().toStdString());
    } catch (const toml::parse_error &e) {
        warn(QStringLiteral("settings.toml:%1: %2").arg(e.source().begin.line).arg(toQString(e.description())));
        // Keep the current keys while the file is being edited, like the theme does.
        if (!m_bindings.isEmpty()) {
            emit problemsChanged();
            return;
        }
    }

    const toml::node_view<toml::node> section = tbl["shortcuts"];
    if (section && !section.is_table())
        warn(QStringLiteral("settings.toml:%1: shortcuts must be a [shortcuts] section")
                 .arg(section.node()->source().begin.line));

    if (const toml::table *table = section.as_table()) {
        for (const auto &[key, node] : *table) {
            const QString name = toQString(key.str());
            const QString where = QStringLiteral("settings.toml:%1").arg(node.source().begin.line);
            if (!bindings.contains(name)) {
                warn(QStringLiteral("%1: unknown action \"%2\"%3").arg(where, name, suggestion(name, names)));
                continue;
            }

            QStringList written;
            if (auto text = node.value<std::string>()) {
                written.append(QString::fromStdString(*text));
            } else if (const toml::array *list = node.as_array()) {
                for (const toml::node &item : *list) {
                    if (auto text = item.value<std::string>())
                        written.append(QString::fromStdString(*text));
                    else
                        warn(QStringLiteral("%1: %2: list items must be keys in quotes").arg(where, name));
                }
            } else {
                warn(QStringLiteral("%1: %2 must be keys in quotes, like \"Ctrl+D\", or a list of them")
                         .arg(where, name));
                continue;
            }
            custom.insert(name);

            QStringList keys;
            for (const QString &text : std::as_const(written)) {
                const QString key = canonical(text);
                if (key.isEmpty())
                    warn(QStringLiteral("%1: %2: \"%3\" isn't a key Qt knows").arg(where, name, text));
                else if (!keys.contains(key))
                    keys.append(key);
            }
            // Nothing usable in a non-empty list: keep the default rather than unbinding.
            if (keys.isEmpty() && !written.isEmpty())
                continue;
            bindings.insert(name, keys);
        }
    }

    m_bindings = bindings;
    m_custom = custom;
    findConflicts();

    for (auto it = m_bindings.cbegin(); it != m_bindings.cend(); ++it) {
        // An action with no keys still needs an entry: QML reads an unset one as undefined.
        if (!m_keys->contains(it.key()) || m_keys->value(it.key()).toStringList() != it.value())
            m_keys->insert(it.key(), it.value());
    }

    emit problemsChanged();
    emit reloaded();
}

void Actions::findConflicts()
{
    m_conflicts.clear();
    const auto all = ActionSchema::actions();
    for (size_t i = 0; i < all.size(); ++i) {
        const QString a = toQString(all[i].name);
        for (size_t j = i + 1; j < all.size(); ++j) {
            if (!scopesOverlap(scopeOf(all[i]), scopeOf(all[j])))
                continue;
            const QString b = toQString(all[j].name);
            for (const QString &key : m_bindings.value(a)) {
                if (!m_bindings.value(b).contains(key))
                    continue;
                m_conflicts.insert(a);
                m_conflicts.insert(b);
                warn(QStringLiteral("\"%1\" is bound to both %2 and %3; neither will work until one changes")
                         .arg(labels({key}).constFirst(), a, b));
            }
        }
    }
}

void Actions::watch()
{
    const QString file = settingsFile();
    if (QFileInfo::exists(file) && !m_watcher.files().contains(file))
        m_watcher.addPath(file);
    if (QFileInfo(m_dataDir).isDir() && !m_watcher.directories().contains(m_dataDir))
        m_watcher.addPath(m_dataDir);
}

QStringList Actions::names(const QString &scope) const
{
    QStringList out;
    for (const Action &a : ActionSchema::actions()) {
        const Scope s = scopeOf(a);
        if (s == Scope::App || scopeName(s) == scope)
            out.append(toQString(a.name));
    }
    return out;
}

void Actions::trigger(const QString &name)
{
    if (!m_bindings.contains(name)) {
        qCWarning(lcActions).noquote() << "unknown action" << name;
        return;
    }
    emit triggered(name, QGuiApplication::focusWindow());
}

QString Actions::tip(const QString &text, const QStringList &keys) const
{
    if (keys.isEmpty())
        return text;
    return QStringLiteral("%1 (%2)").arg(text, label(keys));
}

QString Actions::label(const QStringList &keys) const
{
    return keys.isEmpty() ? QString() : labels({keys.constFirst()}).constFirst();
}

QString Actions::sequenceFor(int key, int modifiers) const
{
    switch (key) {
    case 0:
    case Qt::Key_unknown:
    case Qt::Key_Control:
    case Qt::Key_Shift:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Meta:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
        return {};
    default:
        break;
    }

    Qt::KeyboardModifiers mods = Qt::KeyboardModifiers(modifiers)
                                 & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
    // Qt reports Shift+Tab as Backtab; store it the way people write it.
    if (key == Qt::Key_Backtab) {
        key = Qt::Key_Tab;
        mods |= Qt::ShiftModifier;
    }
    return QKeySequence(QKeyCombination(mods, Qt::Key(key))).toString(QKeySequence::PortableText);
}

void Actions::setKeys(const QString &name, const QStringList &keys)
{
    if (!findAction(name))
        return;

    QStringList formatted;
    for (const QString &k : keys) {
        const QString key = canonical(k);
        if (!key.isEmpty() && !formatted.contains(quoted(key)))
            formatted.append(quoted(key));
    }
    const QString value = formatted.size() == 1 ? formatted.constFirst()
                                                : u'[' + formatted.join(QLatin1String(", ")) + u']';

    QString error;
    if (!TomlPatch::setValueInFile(settingsFile(), QStringLiteral("shortcuts"), name, value, &error))
        warn(QStringLiteral("could not save shortcut %1: %2").arg(name, error));
    reload();
}

void Actions::resetKeys(const QString &name)
{
    QString error;
    if (!TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("shortcuts"), name, &error) && !error.isEmpty())
        warn(QStringLiteral("could not reset shortcut %1: %2").arg(name, error));
    reload();
}
