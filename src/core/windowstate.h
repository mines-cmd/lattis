#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSize>
#include <QTimer>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

// See docs/code/core.md.
class WindowState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool canMinimize READ canMinimize CONSTANT)

public:
    WindowState(const QString &stateDir, const QString &dataDirKey, QObject *parent = nullptr);
    ~WindowState() override;

    static WindowState *create(QQmlEngine *, QJSEngine *);

    Q_INVOKABLE int width(const QString &name, int fallback) const;
    Q_INVOKABLE int height(const QString &name, int fallback) const;
    Q_INVOKABLE bool maximized(const QString &name) const;
    Q_INVOKABLE static bool fillsScreen(const QWindow *window);

    static bool canMinimize();

    Q_INVOKABLE static int compositorRounding();

    Q_INVOKABLE void track(QWindow *window, const QString &name);

private:
    struct Entry
    {
        QSize size;
        bool maximized = false;
    };

    void load();
    void capture();
    void save();

    QString m_file;
    QHash<QString, Entry> m_entries;
    QHash<QString, QPointer<QWindow>> m_windows;
    QTimer m_saveTimer;

    static inline WindowState *s_instance = nullptr;
};
