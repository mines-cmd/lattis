#include "gamepagemigration.h"
#include "gamepagesettings.h"
#include "tomlpatch.h"
#include "tomlsection.h"
#include "tomlstring.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

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
// A style is a file people share: only the names the game page knows for that list go into settings.toml.
QString tomlValue(const QVariant &value, const QString &key)
{
    if (value.typeId() == QMetaType::Bool)
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    const QStringList &choices = gamePageChoices(key);
    QStringList quoted;
    for (const QVariant &item : value.toList()) {
        const QString name = item.toString();
        if (choices.contains(name))
            quoted.append(tomlQuote(name));
    }
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

    // Everything is made in memory and parsed before a byte is written, so a bad value can't damage the file.
    QString text = QString::fromUtf8(bytes);
    bool changed = false;
    for (auto it = legacyKeys().cbegin(); it != legacyKeys().cend(); ++it) {
        const QVariant value = legacy.value(it.key());
        if (!value.isValid() || (section.table && section.table->contains(it.value().toStdString())))
            continue;
        const bool isList = value.typeId() == QMetaType::QVariantList;
        if (!isList && value.typeId() != QMetaType::Bool)
            continue;
        if (!file.exists()) {
            if (error)
                *error = QStringLiteral("could not read %1").arg(settingsFile);
            return false;
        }
        const TomlPatch::Result result = TomlPatch::setValue(text, QStringLiteral("game_page"), it.value(),
                                                             tomlValue(value, it.value()));
        if (!result.ok) {
            if (error)
                *error = result.error;
            return false;
        }
        text = result.text;
        changed = true;
    }
    if (changed) {
        try {
            (void)toml::parse(text.toStdString());
        } catch (const toml::parse_error &e) {
            if (error)
                *error = QStringLiteral("the game page lists from the style don't make a valid settings.toml: %1")
                             .arg(toQString(e.description()));
            return false;
        }
        QSaveFile out(settingsFile);
        if (!out.open(QIODevice::WriteOnly) || out.write(text.toUtf8()) < 0 || !out.commit()) {
            if (error)
                *error = out.errorString();
            return false;
        }
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
