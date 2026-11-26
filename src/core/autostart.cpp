#include "autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>

namespace Autostart {
namespace {
QString quoteExecArg(const QString &argument)
{
    // A literal % is %%, or Exec reads %d, %f and %u as field codes.
    QString arg = argument;
    arg.replace(u'%', QStringLiteral("%%"));
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
        if (c == u'\\')
            out += QStringLiteral("\\\\\\"); // the string escape and the quoting escape both apply
        else if (c == u'"' || c == u'`' || c == u'$')
            out += u'\\';
        out += c;
    }
    out += u'"';
    return out;
}

// The reverse of quoteExecArg for a whole Exec value: string escapes, then quotes, then %%.
QStringList splitExec(const QString &exec)
{
    QString text;
    for (int i = 0; i < exec.size(); ++i) {
        if (exec[i] == u'\\' && i + 1 < exec.size() && exec[i + 1] == u'\\')
            ++i;
        text += exec[i];
    }

    QStringList args;
    QString current;
    bool inArg = false;
    bool quoted = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (quoted) {
            if (c == u'"') {
                quoted = false;
            } else if (c == u'\\' && i + 1 < text.size()) {
                current += text[++i];
            } else {
                current += c;
            }
        } else if (c == u'"') {
            quoted = true;
            inArg = true;
        } else if (c == u' ') {
            if (inArg)
                args << current;
            current.clear();
            inArg = false;
        } else {
            current += c;
            inArg = true;
        }
    }
    if (inArg)
        args << current;
    for (QString &arg : args)
        arg.replace(QStringLiteral("%%"), QStringLiteral("%"));
    return args;
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

QString execDataDir(const QString &exec)
{
    const QStringList args = splitExec(exec);
    if (args.size() < 2 || args[0] != u"env")
        return {};
    const QString prefix = QStringLiteral("LATTIS_DATA=");
    return args[1].startsWith(prefix) ? args[1].mid(prefix.size()) : QString();
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
        else if (trimmed.startsWith(u"Icon="))
            out.icon = trimmed.mid(5).toString();
    }
    return out;
}

QString writeEntry(const QString &folder, const QString &program, const QString &dataDir, const QString &icon)
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
    out << "Icon=" << (icon.isEmpty() ? QStringLiteral("lattis") : icon) << "\n";
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
