#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

#include <optional>

// See docs/code/install.md.

struct ReleaseSource
{
    enum class Host { GitHub, GitLab, Forgejo };

    Host host = Host::GitHub;
    QString server;
    QString project;
    bool prereleases = false;

    QUrl apiUrl() const;
    QString label() const;

    bool operator==(const ReleaseSource &) const = default;
};

struct ReleaseAsset
{
    QString id;
    QString name;
    QUrl url;
    qint64 size = 0;
    QString sha256;
    QString date;

    bool operator==(const ReleaseAsset &) const = default;
};

struct Release
{
    QString tag;
    QString date;
    QString notes;
    bool prerelease = false;
    QList<ReleaseAsset> assets;

    bool operator==(const Release &) const = default;
};

namespace ReleaseSources {
std::optional<ReleaseSource> parse(const QString &text, QString *error = nullptr);
std::optional<Release> parseRelease(const ReleaseSource &source, const QByteArray &json, QString *error,
                                    const QString &assetPatterns = QString());

// False for a name with a separator, a leading dot or "..": it would leave the download folder or hide the file.
bool safeFileName(const QString &name);
QString fromUrl(const QString &text);
QString withoutZsync(const QString &patterns);
bool matches(const QString &name, const QString &patterns);
const ReleaseAsset *pickAsset(const Release &release, const QString &patterns);
}
