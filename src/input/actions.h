#pragma once

#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QQmlPropertyMap>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;
class Actions;

// // See docs/code/input.md.
class ActionKeys : public QQmlPropertyMap
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    explicit ActionKeys(QObject *parent)
        : QQmlPropertyMap(this, parent)
    {
    }
};

class ActionListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(QVariantList sections READ sections NOTIFY sectionsChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        GroupRole,
        GroupTitleRole,
        TitleRole,
        KeysRole,
        KeyLabelsRole,
        DefaultLabelsRole,
        IsCustomRole,
        ConflictRole,
        FirstInGroupRole,
        LastInGroupRole,
    };
    Q_ENUM(Role)

    explicit ActionListModel(Actions *actions);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const { return m_filterText; }
    void setFilterText(const QString &text);

    QVariantList sections() const;
    Q_INVOKABLE int firstRow(const QString &group) const;

signals:
    void filterTextChanged();
    void sectionsChanged();

private:
    QList<int> visibleRows() const;
    bool sameGroup(int row, int otherRow) const;

    Actions *m_actions;
    QString m_filterText;
    QList<int> m_visible;
};

class Actions : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QObject *keys READ keys CONSTANT)
    Q_PROPERTY(QObject *model READ model CONSTANT)
    Q_PROPERTY(QStringList problems READ problems NOTIFY problemsChanged)
    Q_PROPERTY(bool recording READ recording WRITE setRecording NOTIFY recordingChanged)
    //     // Qt otherwise also matches a window's shortcuts while its child window (Settings) has
    //     // focus, and a key both windows use then fires in neither.
    Q_PROPERTY(QWindow *focusWindow READ focusWindow NOTIFY focusWindowChanged)

public:
    explicit Actions(QString dataDir, QObject *parent = nullptr);
    ~Actions() override;

    static Actions *create(QQmlEngine *, QJSEngine *);

    QObject *keys() const { return m_keys; }
    QObject *model() const { return m_model; }
    QStringList problems() const { return m_problems; }
    bool recording() const { return m_recording; }
    QWindow *focusWindow() const;
    void setRecording(bool recording);

    Q_INVOKABLE QStringList names(const QString &scope) const;
    Q_INVOKABLE void trigger(const QString &name);

    Q_INVOKABLE QString tip(const QString &text, const QStringList &keys) const;
    Q_INVOKABLE QString label(const QStringList &keys) const;
    Q_INVOKABLE QString sequenceFor(int key, int modifiers) const;

    Q_INVOKABLE void setKeys(const QString &name, const QStringList &keys);
    Q_INVOKABLE void resetKeys(const QString &name);
    Q_INVOKABLE QString settingsFile() const;

    QStringList keysOf(const QString &name) const { return m_bindings.value(name); }
    bool isCustom(const QString &name) const { return m_custom.contains(name); }
    bool hasConflict(const QString &name) const { return m_conflicts.contains(name); }
    static QStringList labels(const QStringList &keys);
    static QStringList defaultKeys(const QString &name);

    void reload();

signals:
    // `window` is the one with keyboard focus; only it handles the action. (A window also
    // counts as active while its child window has focus, so `active` can't tell.)
    void triggered(const QString &name, QWindow *window);
    void problemsChanged();
    void recordingChanged();
    void focusWindowChanged();
    void reloaded();

private:
    void warn(const QString &message);
    void migrateRenamedActions();
    void watch();
    void findConflicts();

    static inline Actions *s_instance = nullptr;

    QString m_dataDir;
    ActionKeys *m_keys;
    ActionListModel *m_model = nullptr;
    QHash<QString, QStringList> m_bindings;
    QSet<QString> m_custom;
    QSet<QString> m_conflicts;
    QStringList m_problems;
    bool m_recording = false;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
};
