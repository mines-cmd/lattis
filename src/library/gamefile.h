#pragma once

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QStringList>
#include <QVariant>

#include <span>

// See docs/code/library.md.
namespace GameFields {
enum class Type { String, Integer, Float, Bool, StringList, DateTime, Date, Choice };

struct Field
{
    const char *key;
    Type type;
    double min;
    double max;
    const char *defaultText;
    const char *description;
    const char *choices = nullptr;
};

std::span<const Field> all();
const Field *find(const QString &key);
QStringList keys();
QString typeName(Type type);
QStringList choices(const Field &field);
}

struct GameFile
{
    QString title;
    QString platform;
    QString rom;
    int romLine = 0;
    QString description;
    int year = 0;
    double rating = -1;
    QString ratingSource; // igdb = the rating came from a lookup; empty = yours
    QStringList tags;
    bool favorite = false;
    QString status;
    double logoScale = 1;
    QString logoPosition;
    double logoOffsetX = 0;
    double logoOffsetY = 0;
    QString logoScrim;
    QString logoScrimSide;
    double logoScrimStrength = 1;
    int igdb = -1; // 0 = not on IGDB, -1 = not set
    int steamgriddb = -1; // 0 = not on it, -1 = not set
    int playMinutes = 0;
    QDateTime lastPlayed;
    QDate added;
    bool hidden = false;
    QString emulator;
    QStringList args;
    QString launchOptions;
    // Steam games: empty = Steam's default; Proton games: empty = none (runs natively)
    QString proton;
    QStringList skipSteamArt;
    QString exe;
    QString startDir;
    QString installDir;
    QString saveFolder;
    qint64 shortcut = 0;
    bool installed = true;

    bool operator==(const GameFile &) const = default;
};

struct ParsedGameFile
{
    bool ok = false;
    GameFile game;
    QStringList warnings;
};

ParsedGameFile parseGameFile(const QByteArray &bytes, const QString &displayPath);

namespace GameFields {
QVariant validate(const QString &key, const QVariant &value, QString *error);

bool isCleared(const QString &key, const QVariant &value);

void apply(GameFile &game, const QString &key, const QVariant &value);

QString format(const QString &key, const QVariant &value);
}

QString tomlString(const QString &text);
