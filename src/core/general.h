#pragma once

#include "dateformat.h"
#include "generalsettings.h"

#include <QCoreApplication>
#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

// See docs/code/core.md.
class General : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool startFullscreen READ startFullscreen NOTIFY settingsChanged)
    Q_PROPERTY(bool startedFullscreen READ startedFullscreen CONSTANT)
    Q_PROPERTY(bool downloadBarCollapsed READ downloadBarCollapsed NOTIFY settingsChanged)
    Q_PROPERTY(bool systemLocale READ systemLocale NOTIFY systemLocaleChanged)
    Q_PROPERTY(bool reduceMotion READ reduceMotion NOTIFY reduceMotionChanged)
    Q_PROPERTY(bool railHidden READ railHidden NOTIFY railHiddenChanged)
    Q_PROPERTY(QStringList railHiddenButtons READ railHiddenButtons NOTIFY railHiddenButtonsChanged)
    Q_PROPERTY(QStringList railButtonOrder READ railButtonOrder NOTIFY railButtonOrderChanged)
    Q_PROPERTY(QString shortDateFormat READ shortDateFormat CONSTANT)
    Q_PROPERTY(bool startAtLogin READ startAtLogin NOTIFY loginChanged)
    Q_PROPERTY(bool startAtLoginOurs READ startAtLoginOurs NOTIFY loginChanged)
    Q_PROPERTY(QStringList problems READ problems NOTIFY settingsChanged)
    Q_PROPERTY(QString dataDir READ dataDir CONSTANT)
    Q_PROPERTY(bool dataDirFromEnv READ dataDirFromEnv CONSTANT)

public:
    General(const QString &dataDir, QObject *parent = nullptr);
    ~General() override;

    static General *create(QQmlEngine *, QJSEngine *);

    bool startFullscreen() const { return m_settings.startFullscreen; }
    bool startedFullscreen() const { return m_startedFullscreen; }
    bool downloadBarCollapsed() const { return m_settings.downloadBarCollapsed; }
    bool systemLocale() const { return m_settings.systemLocale; }
    bool reduceMotion() const { return m_settings.reduceMotion; }
    bool railHidden() const { return m_settings.railHidden; }
    QStringList railHiddenButtons() const { return m_settings.railHiddenButtons; }
    QStringList railButtonOrder() const { return orderedRailButtons(m_settings.railButtonOrder); }
    QString shortDateFormat() const { return DateFormat::shortFormat(); }
    bool startAtLogin() const { return m_loginExists && m_loginEnabled; }
    bool startAtLoginOurs() const { return m_loginOurs; }
    QStringList problems() const { return m_problems; }
    QString dataDir() const { return m_dataDir; }
    bool dataDirFromEnv() const { return !qEnvironmentVariableIsEmpty("LATTIS_DATA"); }

    Q_INVOKABLE QString setStartFullscreen(bool value);
    Q_INVOKABLE QString setDownloadBarCollapsed(bool value);
    Q_INVOKABLE QString setSystemLocale(bool value);
    Q_INVOKABLE QString setReduceMotion(bool value);
    Q_INVOKABLE QString setRailHidden(bool value);
    Q_INVOKABLE QString setRailButtonShown(const QString &name, bool shown);
    Q_INVOKABLE QString setRailButtonOrder(const QStringList &order);
    Q_INVOKABLE QString setStartAtLogin(bool on);
    Q_INVOKABLE void reload();
    void refreshAutostart();

signals:
    void settingsChanged();
    void systemLocaleChanged();
    void reduceMotionChanged();
    void railHiddenChanged();
    void railHiddenButtonsChanged();
    void railButtonOrderChanged();
    void loginChanged();

private:
    void load();
    void loadAutostart();
    QString setFlag(const QString &key, bool value);
    QString settingsFile() const { return m_dataDir + QStringLiteral("/settings.toml"); }

    static inline General *s_instance = nullptr;

    QString m_dataDir;
    bool m_startedFullscreen = false;
    GeneralSettings m_settings;
    QStringList m_problems;
    bool m_loginExists = false;
    bool m_loginOurs = false;
    bool m_loginEnabled = true;
    QString m_loginExec;
};
