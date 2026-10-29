#include "mediafolders.h"

#include "emulators.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

#include <algorithm>

namespace MediaFolders {
namespace {
bool isPicture(const QString &suffix)
{
    return suffix == QLatin1String("png") || suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")
           || suffix == QLatin1String("webp") || suffix == QLatin1String("bmp");
}

bool isVideo(const QString &suffix)
{
    return suffix == QLatin1String("mp4") || suffix == QLatin1String("mkv") || suffix == QLatin1String("webm")
           || suffix == QLatin1String("mov");
}
}

Listing list(const QStringList &folders, const QString &home)
{
    Listing out;
    QSet<QString> seen;
    for (const QString &folder : folders) {
        const QString root = Emulators::expandHome(folder, home);
        if (root.isEmpty() || !QFileInfo(root).isDir())
            continue;
        // Not following symlinked folders keeps a link back up the tree from looping.
        QDirIterator it(root, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QFileInfo info = it.fileInfo();
            const QString suffix = info.suffix().toLower();
            const bool video = isVideo(suffix);
            if (!video && !isPicture(suffix))
                continue;
            const QString path = info.canonicalFilePath();
            if (path.isEmpty() || seen.contains(path))
                continue;
            seen.insert(path);
            out.files.append({info.fileName(), path, info.absolutePath(), video, info.lastModified()});
        }
    }
    std::sort(out.files.begin(), out.files.end(), [](const File &a, const File &b) {
        return a.time != b.time ? a.time > b.time : a.path < b.path;
    });
    if (out.files.size() > kLimit) {
        out.files.resize(kLimit);
        out.limited = true;
    }
    return out;
}
}
