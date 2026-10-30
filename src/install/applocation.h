#pragma once

#include <QObject>
#include <QString>
#include <QtQmlIntegration>

#include <functional>

class QJSEngine;
class QQmlEngine;

// See docs/code/install.md.

namespace AppLocations {
// Moves `appImage` into `folder`, keeping its name. `moved` is the new path. A different filesystem copies, then deletes.
QString moveInto(const QString &appImage, const QString &folder, QString *moved);

// Why `appImage` shouldn't be moved (empty: it can).
QString moveBlocker(const QString &appImage, const QString &folder, const QString &applicationsDir);
}

class AppLocation : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Running from an AppImage that isn't already in the applications folder, and nothing stops moving it.
    Q_PROPERTY(bool canMove READ canMove NOTIFY changed)
    // Running from an AppImage at all.
    Q_PROPERTY(bool available READ available NOTIFY changed)
    // The folder it would go to, for showing.
    Q_PROPERTY(QString folder READ folder NOTIFY changed)
    Q_PROPERTY(bool inFolder READ inFolder NOTIFY changed)
    Q_PROPERTY(QString blocker READ blocker NOTIFY changed)

public:
    struct Setup
    {
        QString appImage;
        QString home;
        // The folder of the menu entries (~/.local/share/applications).
        QString applicationsDir;
        // Called with the new path after a move, to point everything that remembers the old one at it.
        std::function<void(const QString &newPath)> moved;
    };
    static void setup(const Setup &setup) { s_setup = setup; }
    static AppLocation *instance();
    static AppLocation *create(QQmlEngine *, QJSEngine *) { return instance(); }

    explicit AppLocation(const Setup &setup, QObject *parent = nullptr);

    bool available() const;
    bool canMove() const { return available() && !inFolder() && m_blocker.isEmpty(); }
    QString folder() const { return m_folder; }
    bool inFolder() const;
    QString blocker() const { return m_blocker; }

    // The error, or empty.
    Q_INVOKABLE QString move();

signals:
    void changed();

private:
    void refresh();

    static Setup s_setup;
    Setup m_setup;
    QString m_folder;
    QString m_blocker;
};
