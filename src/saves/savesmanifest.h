#pragma once

#include "savesindex.h"

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <optional>

// See docs/code/saves.md.
namespace SavesManifest {
inline constexpr const char *kDefaultUrl =
    "https://raw.githubusercontent.com/mtkennerly/ludusavi-manifest/master/data/manifest.yaml";

// The folder in the state dir that holds manifest.yaml, index.bin and meta.json.
QString defaultDir();

// LATTIS_SAVES_MANIFEST_URL (`file://` works), else the default.
QString url();

struct Meta
{
    QString etag;
    QDateTime fetched;
    QDateTime checked;
};

Meta readMeta(const QString &dir);

// True when the manifest was never fetched or the last check is more than a day old.
bool checkDue(const Meta &meta, const QDateTime &now = QDateTime::currentDateTimeUtc());

struct UpdateResult
{
    enum Status { Updated, Unchanged, Failed };
    Status status = Failed;
    QString error;
    QStringList warnings;
};

// Fetches the manifest (only when it changed, by ETag, unless `force`) and rebuilds the index from it.
// Blocks until done or `timeoutMs`: call it from a worker thread, and only on the user's say-so.
// A reply that isn't a manifest changes nothing on disk.
UpdateResult update(const QString &dir, const QString &url, bool force = false, int timeoutMs = 120000);

std::optional<SavesIndex> loadIndex(const QString &dir);
}
