#include "appicon.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>
#include <cmath>
#include <optional>

Q_LOGGING_CATEGORY(lcAppIcon, "lattis.appicon")

namespace AppIcon {

namespace {

const QString kFull = QStringLiteral(":/qt/qml/Launcher/resources/logo/dog/lattis-dog-icon.svg");
const QString kSmall = QStringLiteral(":/qt/qml/Launcher/resources/logo/dog/lattis-dog-icon-16px.svg");
constexpr int kMargin = 14;
constexpr int kKeyLength = 12;

QByteArray readTemplate(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcAppIcon) << "can't read" << path;
        return {};
    }
    return file.readAll();
}

const QByteArray &fullTemplate()
{
    static const QByteArray text = readTemplate(kFull);
    return text;
}

const QByteArray &smallTemplate()
{
    static const QByteArray text = readTemplate(kSmall);
    return text;
}

struct Rgb
{
    double r, g, b;
};

Rgb rgbOf(const QColor &c)
{
    return {double(c.red()), double(c.green()), double(c.blue())};
}

// The same arithmetic as tools/logo/icons.js, step for step, so the bundled Ember icons and the
// ones rendered here come out of the template with the same colors.
QString hex(const Rgb &c)
{
    const auto channel = [](double v) { return int(std::lround(std::clamp(v, 0.0, 255.0))); };
    return QColor(channel(c.r), channel(c.g), channel(c.b)).name(QColor::HexRgb);
}

Rgb mix(const Rgb &a, const Rgb &b, double t)
{
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

Rgb shade(const Rgb &c, double saturation, double value)
{
    const double r = c.r / 255, g = c.g / 255, b = c.b / 255;
    const double max = std::max({r, g, b});
    const double d = max - std::min({r, g, b});
    double h = 0;
    if (d > 0) {
        if (max == r)
            h = std::fmod((g - b) / d + 6, 6);
        else if (max == g)
            h = (b - r) / d + 2;
        else
            h = (r - g) / d + 4;
    }
    const double s = std::min(1.0, (max > 0 ? d / max : 0) * saturation);
    const double v = std::min(1.0, max * value);
    const double chroma = v * s;
    const double x = chroma * (1 - std::abs(std::fmod(h, 2) - 1));
    const double m = v - chroma;
    Rgb out{};
    switch (int(h)) {
    case 0: out = {chroma, x, 0}; break;
    case 1: out = {x, chroma, 0}; break;
    case 2: out = {0, chroma, x}; break;
    case 3: out = {0, x, chroma}; break;
    case 4: out = {x, 0, chroma}; break;
    default: out = {chroma, 0, x}; break;
    }
    return {(out.r + m) * 255, (out.g + m) * 255, (out.b + m) * 255};
}

} // namespace

Colors ember()
{
    return {QColor(0xf9, 0x86, 0x44), QColor(0xdc, 0x54, 0x18), QColor(0xff, 0xf6, 0xee),
            QColor(0xe9, 0xcd, 0xb8), QColor(0x2b, 0x3a, 0x5a)};
}

Colors fromMap(const QVariantMap &colors)
{
    const auto color = [&](const char *name) { return colors.value(QString::fromLatin1(name)).value<QColor>(); };
    const Colors out{color("background"), color("backgroundEnd"), color("coat"), color("coatEnd"), color("details")};
    for (const QColor &c : {out.background, out.backgroundEnd, out.coat, out.coatEnd, out.details}) {
        if (!c.isValid())
            return ember();
    }
    return out;
}

QString imageId(const Colors &colors)
{
    QStringList parts;
    for (const QColor &c : {colors.background, colors.backgroundEnd, colors.coat, colors.coatEnd, colors.details})
        parts.append((c.alpha() == 255 ? c.name(QColor::HexRgb) : c.name(QColor::HexArgb)).mid(1));
    return parts.join(u'/');
}

Colors fromImageId(const QString &id)
{
    const QStringList parts = id.split(u'/');
    if (parts.size() != 5)
        return ember();
    QList<QColor> c;
    for (const QString &part : parts) {
        const QColor color(u'#' + part);
        if (!color.isValid())
            return ember();
        c.append(color);
    }
    return {c[0], c[1], c[2], c[3], c[4]};
}

const QList<int> &sizes()
{
    static const QList<int> list{16, 24, 32, 48, 64, 128, 256, 512};
    return list;
}

QByteArray fill(const QByteArray &svgTemplate, const Colors &colors)
{
    const QHash<QString, Rgb> named{
        {QStringLiteral("background"), rgbOf(colors.background)},
        {QStringLiteral("background_end"), rgbOf(colors.backgroundEnd)},
        {QStringLiteral("coat"), rgbOf(colors.coat)},
        {QStringLiteral("coat_end"), rgbOf(colors.coatEnd)},
        {QStringLiteral("details"), rgbOf(colors.details)},
    };
    static const QRegularExpression placeholder(QStringLiteral("\\{\\{([^}]*)\\}\\}"));
    const QString text = QString::fromUtf8(svgTemplate);
    QString out;
    qsizetype last = 0;
    for (const QRegularExpressionMatch &match : placeholder.globalMatch(text)) {
        const QStringList words = match.captured(1).split(u' ', Qt::SkipEmptyParts);
        const auto color = [&](qsizetype i) -> std::optional<Rgb> {
            const auto it = named.constFind(words.value(i));
            return it == named.constEnd() ? std::nullopt : std::optional<Rgb>(*it);
        };
        const auto number = [&](qsizetype i) -> std::optional<double> {
            bool ok = false;
            const double v = words.value(i).toDouble(&ok);
            return ok ? std::optional<double>(v) : std::nullopt;
        };
        const QString op = words.value(0);
        std::optional<Rgb> value;
        if (words.size() == 1) {
            value = color(0);
        } else if (words.size() == 4 && op == QLatin1String("mix")) {
            if (const auto a = color(1), b = color(2); a && b && number(3))
                value = mix(*a, *b, *number(3));
        } else if (words.size() == 4 && op == QLatin1String("shade")) {
            if (const auto base = color(1); base && number(2) && number(3))
                value = shade(*base, *number(2), *number(3));
        }
        if (!value) {
            qCWarning(lcAppIcon) << "unknown placeholder in the icon template:" << match.captured(0);
            return {};
        }
        out += QStringView(text).mid(last, match.capturedStart() - last);
        out += hex(*value);
        last = match.capturedEnd();
    }
    out += QStringView(text).mid(last);
    return out.toUtf8();
}

QByteArray drawing(const Colors &colors, int size)
{
    QByteArray svg = fill(size >= 32 ? fullTemplate() : smallTemplate(), colors);
    if (size >= 48) {
        const int side = 256 + 2 * kMargin;
        svg.replace("viewBox=\"0 0 256 256\"",
                    QStringLiteral("viewBox=\"%1 %1 %2 %2\"").arg(-kMargin).arg(side).toUtf8());
    }
    return svg;
}

QImage render(const Colors &colors, int size)
{
    QByteArray svg = drawing(colors, size);
    if (svg.isEmpty())
        return {};
    QBuffer buffer(&svg);
    QImageReader reader(&buffer, "svg");
    reader.setScaledSize(QSize(size, size));
    QImage image = reader.read();
    if (image.isNull())
        qCWarning(lcAppIcon) << "can't render the icon at" << size << "px:" << reader.errorString();
    return image.convertToFormat(QImage::Format_ARGB32);
}

QString key(const Colors &colors)
{
    // Of the filled drawings, not only the colors: a changed template gets new file names too,
    // so a desktop's icon cache can't keep showing the old picture.
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(fill(fullTemplate(), colors));
    hash.addData(fill(smallTemplate(), colors));
    return QString::fromLatin1(hash.result().toHex().left(kKeyLength));
}

QString fileName(const QString &key, int size)
{
    return QStringLiteral("lattis-%1-%2.png").arg(key).arg(size);
}

QString write(const QString &folder, const Colors &colors)
{
    const QString k = key(colors);
    const QDir dir(folder);
    if (!dir.mkpath(QStringLiteral("."))) {
        qCWarning(lcAppIcon) << "can't make" << folder;
        return {};
    }
    for (int size : sizes()) {
        const QString path = dir.filePath(fileName(k, size));
        if (QFile::exists(path))
            continue;
        const QImage image = render(colors, size);
        QSaveFile file(path);
        if (image.isNull() || !file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit()) {
            qCWarning(lcAppIcon) << "can't write" << path;
            return {};
        }
    }
    return dir.filePath(fileName(k, 256));
}

int cleanUp(const QString &folder, const QStringList &keep)
{
    static const QRegularExpression rendered(QStringLiteral("^lattis-([0-9a-f]{%1})-\\d+\\.png$").arg(kKeyLength));
    QDir dir(folder);
    int removed = 0;
    for (const QString &name : dir.entryList({QStringLiteral("lattis-*.png")}, QDir::Files)) {
        const QRegularExpressionMatch match = rendered.match(name);
        if (match.hasMatch() && !keep.contains(match.captured(1)) && dir.remove(name))
            ++removed;
    }
    return removed;
}

} // namespace AppIcon
