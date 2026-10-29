#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QTimer>

class QQuickItem;
class QQuickWindow;

// See docs/code/core.md.
class UiDriver : public QObject
{
    Q_OBJECT

public:
    UiDriver(QQuickWindow *window, const QString &scriptPath, QObject *parent = nullptr);

    void start();

private:
    struct Command
    {
        int line = 0;
        QString name;
        QString argument;
    };

    void step();
    bool tryCommand(const Command &command);
    void fail(const QString &reason);
    void finish(int code);
    QQuickItem *find(const QString &text, bool contains = false) const;
    QStringList visibleTexts() const;
    void click(QQuickItem *item);
    bool key(const QString &name);
    void type(const QString &text);

    QQuickWindow *m_window;
    QList<Command> m_commands;
    qsizetype m_index = 0;
    QElapsedTimer m_waiting;
    QTimer m_poll;
    bool m_started = false;
};
