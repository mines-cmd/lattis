#include "launchcommand.h"

#include <QDir>

#include <algorithm>

namespace {
QString quoted(const QString &arg)
{
    static const QString kSafe = QStringLiteral("-_./=:+,@%");
    const bool safe = !arg.isEmpty() && std::all_of(arg.cbegin(), arg.cend(), [](QChar c) {
        return c.isLetterOrNumber() || kSafe.contains(c);
    });
    if (safe)
        return arg;
    QString out = arg;
    out.replace(u'\'', QLatin1String("'\\''"));
    return u'\'' + out + u'\'';
}
}

QString LaunchCommand::toString() const
{
    QStringList parts;
    for (const auto &[name, value] : env)
        parts.append(name + u'=' + quoted(value));
    parts.append(quoted(program));
    for (const QString &arg : arguments)
        parts.append(quoted(arg));
    return parts.join(u' ');
}

LaunchCommand buildProgramCommand(const QString &exe, const LaunchOptions::Parts &options)
{
    const QStringList words = options.wrapper + QStringList{exe} + options.args;
    return {words.first(), words.mid(1), options.env};
}

QStringList programEnvironment(const QStringList &environment, const QList<std::pair<QString, QString>> &env)
{
    QString appDir;
    for (const QString &var : environment) {
        if (var.startsWith(QLatin1String("APPDIR=")))
            appDir = QDir::cleanPath(var.mid(7));
    }
    static const QStringList appImageOwn = {QStringLiteral("APPDIR"), QStringLiteral("APPIMAGE"),
                                            QStringLiteral("ARGV0"), QStringLiteral("OWD")};
    const auto inAppDir = [&](const QString &path) {
        const QString clean = QDir::cleanPath(path);
        return clean == appDir || clean.startsWith(appDir + u'/');
    };

    QStringList out;
    for (const QString &var : environment) {
        const qsizetype eq = var.indexOf(u'=');
        const QString name = var.left(eq);
        if (eq <= 0 || std::any_of(env.cbegin(), env.cend(), [&](const auto &e) { return e.first == name; }))
            continue;
        if (appDir.isEmpty() || appDir == u'/') {
            out.append(var);
            continue;
        }
        if (appImageOwn.contains(name))
            continue;
        const QStringList paths = var.mid(eq + 1).split(u':');
        if (std::none_of(paths.cbegin(), paths.cend(), inAppDir)) {
            out.append(var);
            continue;
        }
        QStringList kept;
        for (const QString &path : paths) {
            if (!path.isEmpty() && !inAppDir(path))
                kept.append(path);
        }
        if (!kept.isEmpty())
            out.append(name + u'=' + kept.join(u':'));
    }
    for (const auto &[name, value] : env)
        out.append(name + u'=' + value);
    return out;
}

LaunchCommand buildLaunchCommand(const EmulatorDef &emulator, const EmulatorInstall &install, const QString &romPath,
                                 const QStringList &gameArgs)
{
    QStringList exe;
    if (install.kind == EmulatorInstall::Kind::Flatpak)
        exe = {QStringLiteral("flatpak"), QStringLiteral("run"), install.program};
    else
        exe = {install.program};

    const QString rom = QStringLiteral("{rom}");
    // Without a ROM, the arguments for it go too: {rom} and those between {args} (or {exe})
    // and it, like Dolphin's `-b -e {rom}`.
    qsizetype skipFrom = emulator.command.size();
    qsizetype skipTo = -1;
    if (romPath.isEmpty()) {
        for (qsizetype i = 0; i < emulator.command.size(); ++i) {
            const QString &arg = emulator.command.at(i);
            if (arg == QLatin1String("{exe}") || arg == QLatin1String("{args}"))
                skipFrom = i + 1;
            else if (arg.contains(rom)) {
                skipTo = i;
                break;
            }
        }
    }
    QStringList all;
    for (qsizetype i = 0; i < emulator.command.size(); ++i) {
        const QString &arg = emulator.command.at(i);
        if (i >= skipFrom && i <= skipTo)
            continue;
        if (arg == QLatin1String("{exe}"))
            all += exe;
        else if (arg == QLatin1String("{args}"))
            all += emulator.args + gameArgs;
        else if (arg.contains(rom) && romPath.isEmpty())
            continue;
        else
            all.append(QString(arg).replace(rom, romPath));
    }

    LaunchCommand out;
    if (!all.isEmpty())
        out.program = all.takeFirst();
    out.arguments = all;
    return out;
}
