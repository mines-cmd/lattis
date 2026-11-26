#pragma once

#include <QColor>
#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// See docs/code/install.md.
namespace AppIcon {

struct Colors
{
    QColor background;
    QColor backgroundEnd;
    QColor coat;
    QColor coatEnd;
    QColor details;
};

Colors ember();
Colors fromMap(const QVariantMap &colors);
QString imageId(const Colors &colors);
Colors fromImageId(const QString &id);

const QList<int> &sizes();
QByteArray fill(const QByteArray &svgTemplate, const Colors &colors);
QByteArray drawing(const Colors &colors, int size);
QImage render(const Colors &colors, int size);

QString key(const Colors &colors);
QString fileName(const QString &key, int size);
QString write(const QString &folder, const Colors &colors);
int cleanUp(const QString &folder, const QStringList &keep);

} // namespace AppIcon
