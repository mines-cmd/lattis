#include "credentials.h"
#include "tomlsection.h"
#include "gamefile.h"
#include "suggestion.h"
#include "tomlpatch.h"

#include <QDir>
#include <QFile>

#include <toml++/toml.hpp>

namespace {
const QString kFileName = QStringLiteral("credentials.toml");
const QByteArray kHeader =
    "# API keys for looking up games. Keep this file private: it's readable only by you.\n";
constexpr QFile::Permissions kOwnerOnly = QFile::ReadOwner | QFile::WriteOwner;

const QList<std::pair<QString, QStringList>> &sections()
{
    static const QList<std::pair<QString, QStringList>> known = {
        {QStringLiteral("igdb"), {QStringLiteral("client_id"), QStringLiteral("client_secret")}},
        {QStringLiteral("steamgriddb"), {QStringLiteral("api_key")}},
    };
    return known;
}

QString *field(Credentials &c, const QString &section, const QString &key)
{
    if (section == QLatin1String("igdb") && key == QLatin1String("client_id"))
        return &c.igdbClientId;
    if (section == QLatin1String("igdb") && key == QLatin1String("client_secret"))
        return &c.igdbClientSecret;
    if (section == QLatin1String("steamgriddb") && key == QLatin1String("api_key"))
        return &c.steamGridDbKey;
    return nullptr;
}
}

namespace CredentialsFile {
QString path(const QString &dataDir)
{
    return QDir(dataDir).filePath(kFileName);
}

Parsed<Credentials> parse(const QByteArray &bytes, const QString &displayPath)
{
    Parsed<Credentials> out;
    toml::table root;
    try {
        root = toml::parse(std::string_view(bytes.constData(), size_t(bytes.size())));
    } catch (const toml::parse_error &e) {
        out.warnings.append(
            QStringLiteral("%1:%2: %3").arg(displayPath).arg(e.source().begin.line).arg(toQString(e.description())));
        return out;
    }
    out.ok = true;

    const auto where = [&](const toml::node &node) {
        return QStringLiteral("%1:%2").arg(displayPath).arg(node.source().begin.line);
    };
    QStringList sectionNames;
    for (const auto &[name, keys] : sections())
        sectionNames.append(name);

    for (auto &&[k, node] : root) {
        const QString section = toQString(k.str());
        const auto known = std::find_if(sections().cbegin(), sections().cend(),
                                        [&](const auto &s) { return s.first == section; });
        if (known == sections().cend()) {
            out.warnings.append(QStringLiteral("%1: unknown section [%2]%3")
                                    .arg(where(node), section, suggestion(section, sectionNames)));
            continue;
        }
        const toml::table *table = node.as_table();
        if (!table) {
            out.warnings.append(QStringLiteral("%1: %2 must be a [%2] section").arg(where(node), section));
            continue;
        }
        for (auto &&[keyName, value] : *table) {
            const QString key = toQString(keyName.str());
            QString *target = field(out.value, section, key);
            if (!target) {
                out.warnings.append(QStringLiteral("%1: unknown key %2%3")
                                        .arg(where(value), key, suggestion(key, known->second)));
            } else if (const auto *s = value.as_string()) {
                *target = toQString(s->get()).trimmed();
            } else {
                out.warnings.append(QStringLiteral("%1: %2 must be text in quotes").arg(where(value), key));
            }
        }
    }
    return out;
}

Parsed<Credentials> load(const QString &dataDir)
{
    QFile file(path(dataDir));
    if (!file.exists())
        return {true, {}, {}};
    if (!file.open(QIODevice::ReadOnly))
        return {false, {}, {QStringLiteral("%1: %2").arg(kFileName, file.errorString())}};
    Parsed<Credentials> out = parse(file.readAll(), kFileName);
    if (file.permissions() & (QFile::ReadGroup | QFile::ReadOther))
        out.warnings.append(QStringLiteral("%1: other users can read your API keys; run chmod 600 on it")
                                .arg(kFileName));
    return out;
}

bool setValue(const QString &dataDir, const QString &section, const QString &key, const QString &value,
              QString *error)
{
    const QString file = path(dataDir);
    if (!QFile::exists(file)) {
        if (value.trimmed().isEmpty())
            return true;
        QFile created(file);
        if (!created.open(QIODevice::WriteOnly | QIODevice::NewOnly, kOwnerOnly) || created.write(kHeader) < 0) {
            if (error)
                *error = created.errorString();
            return false;
        }
    } else {
        // Before the write: QSaveFile gives the new file the old one's permissions, so the keys are never
        // written into a file others can read.
        QFile::setPermissions(file, kOwnerOnly);
    }
    return value.trimmed().isEmpty()
        ? TomlPatch::removeValueInFile(file, section, key, error)
        : TomlPatch::setValueInFile(file, section, key, tomlString(value.trimmed()), error);
}
}
