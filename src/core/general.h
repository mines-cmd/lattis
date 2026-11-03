#pragma once

#include "dateformat.h"
#include "gamepagesettings.h"
#include "gallerysettings.h"
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
    Q_PROPERTY(bool toolbar READ toolbar NOTIFY toolbarChanged)
    Q_PROPERTY(double clipVolume READ clipVolume NOTIFY clipVolumeChanged)
    Q_PROPERTY(bool checkForUpdates READ checkForUpdates NOTIFY settingsChanged)
    Q_PROPERTY(bool appMenuEntry READ appMenuEntry NOTIFY settingsChanged)
    Q_PROPERTY(bool isAppImage READ isAppImage CONSTANT)
    Q_PROPERTY(QString skippedUpdate READ skippedUpdate NOTIFY settingsChanged)
    Q_PROPERTY(QStringList railHiddenButtons READ railHiddenButtons NOTIFY railHiddenButtonsChanged)
    Q_PROPERTY(QStringList railButtonOrder READ railButtonOrder NOTIFY railButtonOrderChanged)
    Q_PROPERTY(QStringList toolbarHiddenButtons READ toolbarHiddenButtons NOTIFY toolbarHiddenButtonsChanged)
    Q_PROPERTY(QStringList toolbarButtonOrder READ toolbarButtonOrder NOTIFY toolbarButtonOrderChanged)
    Q_PROPERTY(QStringList gamePageChips READ gamePageChips NOTIFY gamePageChanged)
    Q_PROPERTY(QStringList gamePageButtons READ gamePageButtons NOTIFY gamePageChanged)
    Q_PROPERTY(QStringList gamePageInfoRows READ gamePageInfoRows NOTIFY gamePageChanged)
    Q_PROPERTY(QStringList gamePageSections READ gamePageSections NOTIFY gamePageChanged)
    Q_PROPERTY(QString recentPeriod READ recentPeriod NOTIFY gamePageChanged)
    Q_PROPERTY(QString playTimeFormat READ playTimeFormat NOTIFY gamePageChanged)
    Q_PROPERTY(bool showDescription READ showDescription NOTIFY gamePageChanged)
    Q_PROPERTY(bool showTags READ showTags NOTIFY gamePageChanged)
    Q_PROPERTY(bool showClips READ showClips NOTIFY gamePageChanged)
    Q_PROPERTY(bool showScreenshots READ showScreenshots NOTIFY gamePageChanged)
    Q_PROPERTY(bool showVideo READ showVideo NOTIFY gamePageChanged)
    Q_PROPERTY(bool showAchievements READ showAchievements NOTIFY gamePageChanged)
    Q_PROPERTY(QStringList chipsWithoutIcon READ chipsWithoutIcon NOTIFY gamePageChanged)
    Q_PROPERTY(QStringList collapsedSections READ collapsedSections NOTIFY gamePageChanged)
    Q_PROPERTY(QStringList gamePageProblems READ gamePageProblems NOTIFY gamePageChanged)
    Q_PROPERTY(QString galleryGroup READ galleryGroup NOTIFY galleryChanged)
    Q_PROPERTY(QString gallerySort READ gallerySort NOTIFY galleryChanged)
    Q_PROPERTY(int galleryTileWidth READ galleryTileWidth NOTIFY galleryChanged)
    Q_PROPERTY(QStringList galleryFoldedSections READ galleryFoldedSections NOTIFY galleryChanged)
    Q_PROPERTY(bool galleryCaptions READ galleryCaptions NOTIFY galleryChanged)
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
    bool toolbar() const { return m_settings.toolbar; }
    double clipVolume() const { return m_settings.clipVolume; }
    bool checkForUpdates() const { return m_settings.checkForUpdates; }
    bool appMenuEntry() const { return m_settings.appMenuEntry; }
    bool isAppImage() const { return !qEnvironmentVariableIsEmpty("APPIMAGE"); }
    QString skippedUpdate() const { return m_settings.skippedUpdate; }
    QStringList railHiddenButtons() const { return m_settings.railHiddenButtons; }
    QStringList railButtonOrder() const { return orderedRailButtons(m_settings.railButtonOrder); }
    QStringList toolbarHiddenButtons() const { return m_settings.toolbarHiddenButtons; }
    QStringList toolbarButtonOrder() const { return orderedToolbarButtons(m_settings.toolbarButtonOrder); }
    QStringList gamePageChips() const { return m_gamePage.chips; }
    QStringList gamePageButtons() const { return m_gamePage.buttons; }
    QStringList gamePageInfoRows() const { return m_gamePage.infoRows; }
    QStringList gamePageSections() const { return m_gamePage.sections; }
    QString recentPeriod() const { return m_gamePage.recentPeriod; }
    QString playTimeFormat() const { return m_gamePage.playTimeFormat; }
    bool showDescription() const { return m_gamePage.sections.contains(QLatin1String("description")); }
    bool showTags() const { return m_gamePage.sections.contains(QLatin1String("tags")); }
    bool showClips() const { return m_gamePage.sections.contains(QLatin1String("clips")); }
    bool showScreenshots() const { return m_gamePage.showScreenshots; }
    bool showVideo() const { return m_gamePage.showVideo; }
    bool showAchievements() const { return m_gamePage.showAchievements; }
    QStringList chipsWithoutIcon() const { return m_gamePage.chipsWithoutIcon; }
    QStringList collapsedSections() const { return m_gamePage.collapsedSections; }
    QStringList gamePageProblems() const { return m_gamePageProblems; }
    QString galleryGroup() const { return m_gallery.group; }
    QString gallerySort() const { return m_gallery.sort; }
    int galleryTileWidth() const { return m_gallery.tileWidth; }
    QStringList galleryFoldedSections() const { return m_gallery.foldedSections; }
    bool galleryCaptions() const { return m_gallery.captions; }
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
    Q_INVOKABLE QString setToolbar(bool value);
    Q_INVOKABLE QString setClipVolume(double value);
    Q_INVOKABLE QString setCheckForUpdates(bool value);
    Q_INVOKABLE QString setAppMenuEntry(bool value);
    Q_INVOKABLE QString setSkippedUpdate(const QString &version);
    Q_INVOKABLE QString setRailButtonShown(const QString &name, bool shown);
    Q_INVOKABLE QString setRailButtonOrder(const QStringList &order);
    Q_INVOKABLE QString setToolbarButtonShown(const QString &name, bool shown);
    Q_INVOKABLE QString setToolbarButtonOrder(const QStringList &order);
    Q_INVOKABLE QStringList gamePageOrder(const QString &place) const;
    Q_INVOKABLE QString setGamePageShown(const QString &place, const QString &name, bool shown);
    Q_INVOKABLE QString setGamePageOrder(const QString &place, const QStringList &order);
    Q_INVOKABLE QString setRecentPeriod(const QString &period);
    Q_INVOKABLE QString setPlayTimeFormat(const QString &format);
    Q_INVOKABLE QString setShowScreenshots(bool on);
    Q_INVOKABLE QString setShowVideo(bool on);
    Q_INVOKABLE QString setShowAchievements(bool on);
    Q_INVOKABLE QString setChipIconShown(const QString &name, bool shown);
    Q_INVOKABLE QString setSectionCollapsed(const QString &name, bool collapsed);
    Q_INVOKABLE QString setGalleryGroup(const QString &group);
    Q_INVOKABLE QString setGallerySort(const QString &sort);
    Q_INVOKABLE QString setGalleryTileWidth(int width);
    Q_INVOKABLE QString setGallerySectionFolded(const QString &name, bool folded);
    Q_INVOKABLE QString setGalleryCaptions(bool on);
    Q_INVOKABLE QString setStartAtLogin(bool on);
    Q_INVOKABLE void reload();
    void refreshAutostart();

signals:
    void settingsChanged();
    void systemLocaleChanged();
    void reduceMotionChanged();
    void railHiddenChanged();
    void toolbarChanged();
    void clipVolumeChanged();
    void railHiddenButtonsChanged();
    void railButtonOrderChanged();
    void toolbarHiddenButtonsChanged();
    void toolbarButtonOrderChanged();
    void gamePageChanged();
    void galleryChanged();
    void loginChanged();

private:
    void load();
    void loadAutostart();
    QString setGalleryValue(const QString &key, const QString &value, bool isDefault);
    QString setMediaFlag(const QString &key, bool on);
    QString setFlag(const QString &key, bool value);
    QString writeGamePage(const QString &place, const QStringList &order, const QStringList &shown);
    QString setGamePageChoice(const QString &key, const QString &value, const QString &defaultValue);
    QString setGamePageFlag(const QString &key, const QString &place, const QStringList &current, const QString &name,
                            bool on);
    QString settingsFile() const { return m_dataDir + QStringLiteral("/settings.toml"); }

    static inline General *s_instance = nullptr;

    QString m_dataDir;
    bool m_startedFullscreen = false;
    GeneralSettings m_settings;
    GamePageSettings m_gamePage;
    GallerySettings m_gallery;
    QStringList m_galleryProblems;
    QStringList m_gamePageProblems;
    QStringList m_problems;
    bool m_loginExists = false;
    bool m_loginOurs = false;
    bool m_loginEnabled = true;
    QString m_loginExec;
};
