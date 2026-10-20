#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <utility>

// See docs/code/core.md.
namespace TomlPatch {
struct Result
{
    bool ok = false;
    QString text;
    QString error;
};

Result setValue(const QString &text, const QString &section, const QString &key,
                 const QString &formattedValue);
Result removeValue(const QString &text, const QString &section, const QString &key);

struct Block
{
    QString array;
    QString value;
    QString key = QStringLiteral("id");
};

Result setBlockValue(const QString &text, const Block &block, const QString &key,
                     const QString &formattedValue);
QStringList blockValues(const QString &text, const QString &array, const QString &key);
QStringList blockKeys(const QString &text, const Block &block);
Result removeBlockValue(const QString &text, const Block &block, const QString &key);
Result appendBlock(const QString &text, const QString &array,
                   const QList<std::pair<QString, QString>> &values);
Result removeBlock(const QString &text, const Block &block);
Result moveBlock(const QString &text, const Block &block, const QString &beforeValue);

bool setValueInFile(const QString &path, const QString &section, const QString &key,
                     const QString &formattedValue, QString *error = nullptr);
bool removeValueInFile(const QString &path, const QString &section, const QString &key,
                        QString *error = nullptr);
}
