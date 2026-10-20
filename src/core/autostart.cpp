#include "autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>

namespace Autostart {
namespace {
QString quoteExecArg(const QString &arg)
{
    bool needsQuoting = false;
    for (const QChar &c : arg) {
        if (c == u' ' || c == u'"' || c == u'\\' || c == u'\'' || c == u'`'
                || c == u'$' || c == u'<' || c == u'>' || c == u'~'
                || c == u'|' || c == u'&' || c == u';' || c == u'*'
                || c == u'?' || c == u'#' || c == u'(' || c == u')') {
            needsQuoting = true;
            break;
        }
    }
    if (!needsQuoting)
        return arg;

    QString out;
    out.reserve(arg.size() + 2);
    out += u'"';
    for (const QChar &c : arg) {
        if (c == u'"' || c == u'\\' || c == u'`' || c == u'$')
            out += u'\\';
        out += c;
    }
    out += u'"';
    return out;
}

const QString kFileName = QStringLiteral("lattis.desktop");
const QString kOurMark = QStringLiteral("X-Lattis-Autostart=true");
}

QString entryExec(const QString &program, const QString &dataDir)
{
    if (dataDir.isEmpty())
        return quoteExecArg(program);
    // env LATTIS_DATA=<dir> <program>
    // The variable's value is not an Exec argument itself; quote the whole env arg.
    return QStringLiteral("env ") + quoteExecArg(QStringLiteral("LATTIS_DATA=") + dataDir)
           + u' ' + quoteExecArg(program);
}

Entry readEntry(const QString &folder)
{
    Entry out;
    QFile file(folder + u'/' + kFileName);
    if (!file.open(QIODevice::ReadOnly))
        return out;
    out.exists = true;

    const QString text = QString::fromUtf8(file.readAll());
    for (const QStringView line : QStringView(text).split(u'\n')) {
        const QStringView trimmed = line.trimmed();
        if (trimmed == QStringView(kOurMark))
            out.ours = true;
        else if (trimmed == u"Hidden=true")
            out.enabled = false;
        else if (trimmed == u"X-GNOME-Autostart-enabled=false")
            out.enabled = false;
        else if (trimmed.startsWith(u"Exec="))
            out.exec = trimmed.mid(5).toString();
    }
    return out;
}

QString writeEntry(const QString &folder, const QString &program, const QString &dataDir)
{
    if (!QDir().mkpath(folder))
        return QStringLiteral("Could not create %1").arg(folder);

    QSaveFile file(folder + u'/' + kFileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return QStringLiteral("Could not write %1: %2").arg(file.fileName(), file.errorString());

    QTextStream out(&file);
    out << "[Desktop Entry]\n";
    out << "Type=Application\n";
    out << "Name=Lattis\n";
    out << "Icon=lattis\n";
    out << "Exec=" << entryExec(program, dataDir) << "\n";
    out << "X-Lattis-Autostart=true\n";

    if (!file.commit())
        return QStringLiteral("Could not write %1: %2").arg(file.fileName(), file.errorString());
    return {};
}

QString removeEntry(const QString &folder)
{
    const QString path = folder + u'/' + kFileName;
    if (!QFile::exists(path))
        return {};
    if (!QFile::remove(path))
        return QStringLiteral("Could not remove %1").arg(path);
    return {};
}

QString autostartFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/autostart");
}

QString currentProgram()
{
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    return appImage.isEmpty() ? QCoreApplication::applicationFilePath() : appImage;
}
}
