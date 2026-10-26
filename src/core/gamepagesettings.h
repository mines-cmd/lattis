#pragma once

#include "parsed.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

// See docs/code/core.md.
struct GamePageSettings
{
    QStringList chips = defaultChips();
    QStringList buttons = defaultButtons();
    QStringList infoRows = defaultInfoRows();
    QStringList sections = defaultSections();
    QStringList chipsOrder;   // of every chip, as written: see gamePageOrder()
    QStringList buttonsOrder;
    QStringList infoRowsOrder;
    QStringList sectionsOrder;
    QStringList chipsWithoutIcon;
    QStringList collapsedSections;
    QString recentPeriod = QStringLiteral("two_weeks");
    QString playTimeFormat = QStringLiteral("hours");
    bool showScreenshots = true;

    bool operator==(const GamePageSettings &) const = default;

    static QStringList defaultChips();
    static QStringList defaultButtons();
    static QStringList defaultInfoRows();
    static QStringList defaultSections();
};

// The four lists on the game page.
namespace GamePage {
inline const QString chips = QStringLiteral("chips");
inline const QString buttons = QStringLiteral("buttons");
inline const QString infoRows = QStringLiteral("info_rows");
inline const QString sections = QStringLiteral("sections");
}

const QStringList &gamePageChoices(const QString &place);
const QStringList &recentPeriods();
const QStringList &playTimeFormats();
bool isGamePagePlace(const QString &place);
const QStringList &gamePageShown(const GamePageSettings &settings, const QString &place);

// Every choice of a place in the order the page shows them: the shown ones in their listed order,
// the hidden ones in their remembered slots, anything the file leaves out last.
QStringList gamePageOrder(const GamePageSettings &settings, const QString &place);

Parsed<GamePageSettings> parseGamePageSettings(const QByteArray &bytes, const QString &displayPath);
