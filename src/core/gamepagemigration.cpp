#include "gamepagemigration.h"
#include "tomlpatch.h"
#include "tomlsection.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace GamePageMigration {
const QMap<QString, QString> &legacyKeys()
{
    static const QMap<QString, QString> keys = {
        {QStringLiteral("stats"), QStringLiteral("chips")},
        {QStringLiteral("action_buttons"), QStringLiteral("buttons")},
        {QStringLiteral("info_rows"), QStringLiteral("info_rows")},
        {QStringLiteral("show_description"), QStringLiteral("show_description")},
        {QStringLiteral("show_tags"), QStringLiteral("show_tags")}};
    return keys;
}

namespace {
QString tomlValue(const QVariant &value)
{
    if (value.typeId() == QMetaType::Bool)
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    QStringList quoted;
    for (const QVariant &item : value.toList())
        quoted.append(u'"' + item.toString() + u'"');
    return u'[' + quoted.join(QLatin1String(", ")) + u']';
}
}

bool run(const QString &settingsFile, const QString &markerFile, const QVariantMap &legacy, QString *error)
{
    if (QFileInfo::exists(markerFile))
        return false;

    QFile file(settingsFile);
    QByteArray bytes;
    if (file.open(QIODevice::ReadOnly))
        bytes = file.readAll();
    const TomlSection section(bytes, QStringLiteral("settings.toml"), "game_page");

    for (auto it = legacyKeys().cbegin(); it != legacyKeys().cend(); ++it) {
        const QVariant value = legacy.value(it.key());
        if (!value.isValid() || (section.table && section.table->contains(it.value().toStdString())))
            continue;
        const bool isList = value.typeId() == QMetaType::QVariantList;
        if (!isList && value.typeId() != QMetaType::Bool)
            continue;
        if (!TomlPatch::setValueInFile(settingsFile, QStringLiteral("game_page"), it.value(), tomlValue(value), error))
            return false;
    }

    QDir().mkpath(QFileInfo(markerFile).absolutePath());
    QFile marker(markerFile);
    if (!marker.open(QIODevice::WriteOnly)) {
        if (error)
            *error = marker.errorString();
        return false;
    }
    return true;
}
}
