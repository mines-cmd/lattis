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

    Q_INVOKABLE QString selectedGame() const { return m_selectedGame; }
    Q_INVOKABLE void setSelectedGame(const QString &id);
    // The Media row loads after the first frame; the height it last had keeps the rows below still until then.
    Q_INVOKABLE qreal openingMediaHeight(const QString &id) const { return id == m_mediaGame ? m_mediaHeight : 0; }
    Q_INVOKABLE void setOpeningMedia(const QString &id, qreal height);

    // For a window that just dropped a lot of QML: runs the JS garbage collector, then hands
    // the freed heap back to the system, which glibc otherwise keeps for reuse.
    Q_INVOKABLE void releaseMemory(QObject *context);

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
    QString m_selectedGame;
    QString m_mediaGame;
    qreal m_mediaHeight = 0;
    QHash<QString, QPointer<QWindow>> m_windows;
    QTimer m_saveTimer;

    static inline WindowState *s_instance = nullptr;
};
