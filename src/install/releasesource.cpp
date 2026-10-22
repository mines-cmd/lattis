#include "releasesource.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

namespace {
const QRegularExpression &namePart()
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));
    return re;
}

const QRegularExpression &hostName()
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?(\\.[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?)+$"));
    return re;
}

bool allParts(const QStringList &parts)
{
    return std::all_of(parts.cbegin(), parts.cend(), [](const QString &p) { return namePart().match(p).hasMatch(); });
}

QString idText(const QJsonValue &value)
{
    if (value.isDouble())
        return QString::number(qint64(value.toDouble()));
    return value.toString();
}

QString digestOf(const QJsonValue &value)
{
    const QString text = value.toString();
    return text.startsWith(QLatin1String("sha256:")) ? text.mid(7).toLower() : QString();
}

std::optional<Release> fromGitHub(const QJsonObject &root, QString *error)
{
    if (!root.contains(QLatin1String("tag_name"))) {
        *error = root.value(QLatin1String("message")).toString(QStringLiteral("GitHub sent an answer that isn't a release"));
        return std::nullopt;
    }
    Release release;
    release.tag = root.value(QLatin1String("tag_name")).toString();
    release.date = root.value(QLatin1String("published_at")).toString();
    for (const QJsonValue &item : root.value(QLatin1String("assets")).toArray()) {
        const QJsonObject a = item.toObject();
        ReleaseAsset asset;
        asset.id = idText(a.value(QLatin1String("id")));
        asset.name = a.value(QLatin1String("name")).toString();
        asset.url = QUrl(a.value(QLatin1String("browser_download_url")).toString());
        asset.size = qint64(a.value(QLatin1String("size")).toDouble());
        asset.sha256 = digestOf(a.value(QLatin1String("digest")));
        asset.date = a.value(QLatin1String("updated_at")).toString();
        if (!asset.name.isEmpty() && asset.url.isValid())
            release.assets.append(asset);
    }
    return release;
}

std::optional<Release> fromGitLab(const QJsonObject &root, QString *error)
{
    if (!root.contains(QLatin1String("tag_name"))) {
        *error = root.value(QLatin1String("message")).toString(QStringLiteral("The server sent an answer that isn't a release"));
        return std::nullopt;
    }
    Release release;
    release.tag = root.value(QLatin1String("tag_name")).toString();
    release.date = root.value(QLatin1String("released_at")).toString();
    for (const QJsonValue &item : root.value(QLatin1String("assets")).toObject().value(QLatin1String("links")).toArray()) {
        const QJsonObject a = item.toObject();
        ReleaseAsset asset;
        asset.id = idText(a.value(QLatin1String("id")));
        asset.name = a.value(QLatin1String("name")).toString();
        const QString direct = a.value(QLatin1String("direct_asset_url")).toString();
        asset.url = QUrl(direct.isEmpty() ? a.value(QLatin1String("url")).toString() : direct);
        asset.date = release.date;
        if (!asset.name.isEmpty() && asset.url.isValid())
            release.assets.append(asset);
    }
    return release;
}
}

QUrl ReleaseSource::apiUrl() const
{
    if (host == Host::GitHub)
        return QUrl(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(project));
    return QUrl(QStringLiteral("https://%1/api/v4/projects/%2/releases?per_page=1")
                    .arg(server, QString::fromLatin1(QUrl::toPercentEncoding(project))),
                QUrl::TolerantMode);
}

QString ReleaseSource::label() const
{
    return host == Host::GitHub ? QStringLiteral("GitHub") : server;
}

namespace ReleaseSources {
std::optional<ReleaseSource> parse(const QString &text, QString *error)
{
    const auto fail = [&](const QString &message) {
        if (error)
            *error = message;
        return std::optional<ReleaseSource>();
    };
    ReleaseSource source;
    if (text.startsWith(QLatin1String("github:"))) {
        const QStringList parts = text.mid(7).split(u'/');
        if (parts.size() != 2 || !allParts(parts))
            return fail(QStringLiteral("release is \"github:<owner>/<repo>\""));
        source.host = ReleaseSource::Host::GitHub;
        source.server = QStringLiteral("github.com");
        source.project = text.mid(7);
        return source;
    }
    if (text.startsWith(QLatin1String("gitlab:"))) {
        const QStringList parts = text.mid(7).split(u'/');
        if (parts.size() < 3 || !hostName().match(parts.first()).hasMatch() || !allParts(parts.mid(1)))
            return fail(QStringLiteral("release is \"gitlab:<host>/<group>/<project>\""));
        source.host = ReleaseSource::Host::GitLab;
        source.server = parts.first();
        source.project = parts.mid(1).join(u'/');
        return source;
    }
    return fail(QStringLiteral("release starts with github: or gitlab:, like \"github:owner/repo\""));
}

std::optional<Release> parseRelease(const ReleaseSource &source, const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    QJsonObject root;
    if (doc.isArray() && !doc.array().isEmpty())
        root = doc.array().first().toObject();
    else if (doc.isObject())
        root = doc.object();
    else {
        *error = doc.isArray() ? QStringLiteral("There is no release yet")
                               : QStringLiteral("%1 sent an answer that isn't JSON").arg(source.label());
        return std::nullopt;
    }
    return source.host == ReleaseSource::Host::GitHub ? fromGitHub(root, error) : fromGitLab(root, error);
}

bool matches(const QString &name, const QString &patterns)
{
    for (const QString &pattern : patterns.split(u';', Qt::SkipEmptyParts)) {
        const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(pattern.trimmed()),
                                    QRegularExpression::CaseInsensitiveOption);
        if (re.match(name).hasMatch())
            return true;
    }
    return false;
}

const ReleaseAsset *pickAsset(const Release &release, const QString &patterns)
{
    // The patterns are in order of preference.
    for (const QString &pattern : patterns.split(u';', Qt::SkipEmptyParts)) {
        for (const ReleaseAsset &asset : release.assets) {
            if (matches(asset.name, pattern))
                return &asset;
        }
    }
    return nullptr;
}
}
