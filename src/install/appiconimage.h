#pragma once

#include <QQuickImageProvider>

// See docs/code/install.md.
class AppIconImageProvider : public QQuickImageProvider
{
public:
    AppIconImageProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
