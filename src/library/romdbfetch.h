#pragma once

#include "romdb.h"

#include <QString>

// See docs/code/library.md.
namespace RomDbFetch {
inline constexpr const char *kDefaultUrl = "https://raw.githubusercontent.com/mines-cmd/lattis/main/data/romdb/";

// Where the databases come from: LATTIS_ROMDB_URL (a folder URL, `file://` too), else the default.
QString baseUrl();

// Downloads `<base><file>.tsv` for the platform into `path`. Blocks until done or `timeoutMs`:
// call it from the scan's thread, never the main one. Nothing is kept for a reply that isn't a database.
bool download(const QString &base, const QString &platform, const QString &path, int timeoutMs = 20000);

// Off until the app turns it on, so no test or tool reaches the network by scanning.
void setEnabled(bool enabled);

RomDbOptions options(const QString &cacheDir, bool allowDownload);
}
