#pragma once

#include "uninstall.h"

#include <QFileSystemWatcher>
#include <QHash>
#include <QQmlPropertyMap>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <optional>
#include <QtQml/qqmlregistration.h>

namespace ThemeSchema {
struct Setting;
}

class QJSEngine;
class QQmlEngine;
class ThemeSettingsModel;

// // See docs/code/theme.md.
class ThemeSection : public QQmlPropertyMap
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    explicit ThemeSection(QObject *parent)
        : QQmlPropertyMap(this, parent)
    {
    }
};

class Theme : public QQmlPropertyMap
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString activeColors READ activeColors NOTIFY activeColorsChanged)
    Q_PROPERTY(QString activeStyle READ activeStyle NOTIFY activeStyleChanged)
    Q_PROPERTY(QString activeTheme READ activeTheme NOTIFY activeThemeChanged)
    Q_PROPERTY(QStringList availableColors READ availableColors NOTIFY availableChanged)
    Q_PROPERTY(QStringList availableStyles READ availableStyles NOTIFY availableChanged)
    Q_PROPERTY(QStringList availableThemes READ availableThemes NOTIFY availableChanged)
    Q_PROPERTY(QStringList builtInColors READ builtInColors CONSTANT)
    Q_PROPERTY(QStringList builtInStyles READ builtInStyles CONSTANT)
    Q_PROPERTY(QStringList builtInThemes READ builtInThemes CONSTANT)
    Q_PROPERTY(QStringList problems READ problems NOTIFY problemsChanged)
    Q_PROPERTY(QObject *colorsSchema READ colorsSchema CONSTANT)
    Q_PROPERTY(QObject *styleSchema READ styleSchema CONSTANT)
    Q_PROPERTY(QObject *customizeSchema READ customizeSchema CONSTANT)
    Q_PROPERTY(QVariantList sessionChanges READ sessionChanges NOTIFY sessionChangesChanged)

public:
    explicit Theme(QString dataDir, Uninstall::Trash trash = Uninstall::systemTrash, QObject *parent = nullptr);

    static Theme *create(QQmlEngine *, QJSEngine *);

    QString activeColors() const { return m_activeColors; }
    QString activeStyle() const { return m_activeStyle; }
    QString activeTheme() const { return m_activeTheme; }
    // The details.* keys that moved to [game_page] (see GamePageMigration), as the active style sets them.
    QVariantMap legacyGamePage() const { return m_legacyGamePage; }
    QStringList availableColors() const;
    QStringList availableStyles() const;
    QStringList availableThemes() const;
    QStringList builtInColors() const;
    QStringList builtInStyles() const;
    QStringList builtInThemes() const;
    QStringList problems() const { return m_problems; }
    QObject *colorsSchema() const;
    QObject *styleSchema() const;
    QObject *customizeSchema() const;

    Q_INVOKABLE QVariantMap preview(const QString &colors, const QString &style);
    Q_INVOKABLE QVariantMap themePair(const QString &name) const;

    Q_INVOKABLE QString colorsFilePath();
    Q_INVOKABLE QString styleFilePath();
    Q_INVOKABLE bool canResetToBuiltIn(const QString &half) const;
    Q_INVOKABLE bool resetToBuiltIn(const QString &half);
    Q_INVOKABLE QString activeFile(const QString &section, const QString &key) const;

    Q_INVOKABLE void setValue(const QString &section, const QString &key, const QVariant &value);
    Q_INVOKABLE void resetValue(const QString &section, const QString &key);

    Q_INVOKABLE void beginEditSession();
    Q_INVOKABLE void revertValue(const QString &section, const QString &key);
    Q_INVOKABLE void revertAll();
    Q_INVOKABLE void undoReset();
    QString revertText(const QString &section, const QString &key) const;
    Q_INVOKABLE QVariantMap rowMenu(const QString &section, const QString &key);
    Q_INVOKABLE void useDefault(const QString &section, const QString &key);
    Q_INVOKABLE void useOwnValue(const QString &section, const QString &key);
    QVariantList sessionChanges() const;

    Q_INVOKABLE QVariant rawValue(const QString &section, const QString &key) const;
    Q_INVOKABLE QString sourceLabel(const QString &section, const QString &key) const;
    Q_INVOKABLE bool isCustomized(const QString &section, const QString &key) const;
    Q_INVOKABLE QVariant resolvedValue(const QString &section, const QString &key) const;
    Q_INVOKABLE QVariantList paletteColors() const;

    Q_INVOKABLE void setColors(const QString &name);
    Q_INVOKABLE void setStyle(const QString &name);
    Q_INVOKABLE void setTheme(const QString &name);
    Q_INVOKABLE bool createColors(const QString &name, const QString &basedOn);
    Q_INVOKABLE bool createStyle(const QString &name, const QString &basedOn, const QString &description = {});
    Q_INVOKABLE QString styleDescription(const QString &style) const;
    Q_INVOKABLE QString styleSummary(const QString &style);
    Q_INVOKABLE bool setStyleDescription(const QString &name, const QString &text);
    Q_INVOKABLE QString nameError(const QString &kind, const QString &name) const;
    Q_INVOKABLE bool themeExists(const QString &name) const;
    Q_INVOKABLE QVariantMap usedBy(const QString &kind, const QString &name) const;
    Q_INVOKABLE bool deleteColors(const QString &name, bool permanent = false);
    Q_INVOKABLE bool deleteStyle(const QString &name, bool permanent = false);
    Q_INVOKABLE bool deleteTheme(const QString &name, bool permanent = false);
    Q_INVOKABLE bool needsPermanent() const { return m_needsPermanent; }
    Q_INVOKABLE bool renameColors(const QString &name, const QString &newName);
    Q_INVOKABLE bool renameStyle(const QString &name, const QString &newName);
    Q_INVOKABLE bool renameTheme(const QString &name, const QString &newName);
    Q_INVOKABLE bool saveTheme(const QString &name, bool replace = false);

signals:
    void activeColorsChanged();
    void activeStyleChanged();
    void activeThemeChanged();
    void availableChanged();
    void problemsChanged();
    void valueChanged(const QString &section, const QString &key);
    void reloaded();
    void sessionChangesChanged();

private:
    enum class Half { Colors, Style };

    struct Entry
    {
        QVariant raw;
        QString origin;
        QString name;
        bool user;
        Half half;
    };

    struct Pair
    {
        QString colors;
        QString style;
    };

    static QString folder(Half half);
    static QString word(Half half);
    static Half halfOf(const QString &fullKey);

    void ensureFiles();
    void migrateRadiusTiers();
    void dropMaterialTheme();
    void writeReference(Half half);
    void reload();
    void watch();
    void dropHalfState(Half half);
    bool deleteUserFile(const QString &kind, const QString &name, bool permanent);
    bool renameUserFile(const QString &kind, const QString &name, const QString &newName);
    void warn(const QString &message);

    QString readAppearance(Half half) const;
    QString readActiveThemeName() const;
    QString activeName(Half half) const { return half == Half::Colors ? m_loadedColors : m_loadedStyle; }
    QString userHalfPath(Half half, const QString &name) const;
    QString editFilePath(Half half);
    static std::optional<Half> halfNamed(const QString &word);
    bool isBuiltInName(Half half, const QString &name) const;
    bool createHalf(Half half, const QString &name, const QString &basedOn, const QString &description = {});
    static constexpr int kMaxDescription = 48;
    static QString capDescription(const QString &text);
    QString halfPath(Half half, const QString &name, bool builtInOnly = false) const;
    QString userThemePath(const QString &name) const;
    std::optional<Pair> readPair(const QString &path) const;
    std::optional<Pair> pairOf(const QString &theme) const;
    QStringList namesIn(Half half) const;
    QStringList legacyThemeNames() const;
    QString deriveActiveTheme() const;
    void setAppearance(const QString &colors, const QString &style);
    bool applyHalfFile(Half half, const QString &name, bool builtInOnly, QHash<QString, Entry> &merged,
                       QStringList &chain, bool parentsOnly = false);
    QVariant parentRaw(const QString &full);
    QString describeRaw(const QString &full, const QVariant &raw) const;

    QVariant valueOf(const ThemeSchema::Setting &setting);
    QVariant convert(const QVariant &raw, const ThemeSchema::Setting &setting, QString *error);

    void scheduleSave(const QString &section, const QString &key);
    void flushPendingSaves();
    void applyOverridesOnTop();
    struct SessionStart
    {
        QVariant raw;
        QString resolved;
    };

    QVariant ownRaw(const QString &full) const;
    QString resolvedText(const QString &full) const;
    QString shownValue(const QString &full, const QVariant &raw, const QString &resolved) const;
    bool changedThisSession(const QString &full, const SessionStart &start) const;
    void rememberSessionStart(const QString &full);

    static inline Theme *s_instance = nullptr;

    QString m_dataDir;
    Uninstall::Trash m_trash;
    bool m_needsPermanent = false;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
    QTimer m_saveTimer;
    bool m_loaded = false;
    bool m_savingOwnChange = false;
    bool m_quiet = false;
    bool m_collectLegacy = false;
    QVariantMap m_legacyGamePage;

    QString m_activeColors;
    QString m_activeStyle;
    QString m_loadedColors;
    QString m_loadedStyle;
    QString m_activeTheme;
    QStringList m_problems;

    QHash<QString, QQmlPropertyMap *> m_sections;
    QHash<QString, Entry> m_merged;
    QHash<QString, QVariant> m_resolved;
    QSet<QString> m_resolving;

    // Overrides applied by the settings window, kept on top of reloads until they've been
    // written to disk (so a hot-reload triggered by our own save, or a concurrent hand
    // edit, can't revert an unsaved edit still in the debounce window).
    QHash<QString, QVariant> m_overrides;
    QSet<QString> m_pendingSaves;

    // Each setting edited since the settings window opened, as it was before the first edit.
    // Edits are saved as they happen, so this is the only copy of the value a user may want back.
    QHash<QString, SessionStart> m_sessionStart;
    QString m_lastResetKey;
    QVariant m_lastResetRaw;

    ThemeSettingsModel *m_colorsSchema = nullptr;
    ThemeSettingsModel *m_styleSchema = nullptr;
    ThemeSettingsModel *m_customizeSchema = nullptr;
};
