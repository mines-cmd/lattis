#include "appiconimage.h"

#include "appicon.h"

AppIconImageProvider::AppIconImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image, QQuickImageProvider::ForceAsynchronousImageLoading)
{
}

QImage AppIconImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const int side = qMax(requestedSize.width(), requestedSize.height());
    QImage image = AppIcon::render(AppIcon::fromImageId(id), side > 0 ? side : 256);
    if (size)
        *size = image.size();
    return image;
}
