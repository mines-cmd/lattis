#pragma once

#include <QString>

#include <optional>

// See docs/code/install.md.
struct AppImageUpdateInfo
{
    QString source;
    QString asset;
    bool prereleases = false;

    static std::optional<QString> readSection(const QString &path);
    static std::optional<AppImageUpdateInfo> parse(const QString &info);
    static std::optional<AppImageUpdateInfo> read(const QString &path);

    bool operator==(const AppImageUpdateInfo &) const = default;
};
