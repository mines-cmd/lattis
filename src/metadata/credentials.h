#pragma once

#include "parsed.h"

#include <QByteArray>
#include <QString>

// See docs/code/metadata.md.
struct Credentials
{
    QString igdbClientId;
    QString igdbClientSecret;
    QString steamGridDbKey;

    bool hasIgdb() const { return !igdbClientId.isEmpty() && !igdbClientSecret.isEmpty(); }
    bool hasSteamGridDb() const { return !steamGridDbKey.isEmpty(); }

    bool operator==(const Credentials &) const = default;
};

namespace CredentialsFile {
QString path(const QString &dataDir);
Parsed<Credentials> parse(const QByteArray &bytes, const QString &displayPath);
Parsed<Credentials> load(const QString &dataDir);
bool setValue(const QString &dataDir, const QString &section, const QString &key, const QString &value,
              QString *error = nullptr);
}
