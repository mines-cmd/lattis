#include "appiconimage.h"

#include "appicon.h"

AppIconImageProvider::AppIconImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image, QQuickImageProvider::ForceAsynchronousImageLoading)
{
}

QImage AppIconImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // "wordmark/light/<colors>" draws the name for a dark background, "wordmark/dark/<colors>" for a light one.
    static const QString wordmark = QStringLiteral("wordmark/");
    QImage image;
    if (id.startsWith(wordmark)) {
        const QString rest = id.mid(wordmark.size());
        const qsizetype slash = rest.indexOf(u'/');
        image = AppIcon::renderWordmark(AppIcon::fromImageId(rest.mid(slash + 1)),
                                        rest.left(slash) == QLatin1String("light"),
                                        requestedSize.height() > 0 ? requestedSize.height() : 256);
    } else {
        const int side = qMax(requestedSize.width(), requestedSize.height());
        image = AppIcon::render(AppIcon::fromImageId(id), side > 0 ? side : 256);
    }
    if (size)
        *size = image.size();
    return image;
}
