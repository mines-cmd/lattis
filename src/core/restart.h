#pragma once

#include <QString>
#include <QStringList>

// See docs/code/core.md.
namespace Restart {
inline const QString kWaitFlag = QStringLiteral("--wait-for-lock");
inline constexpr int kWaitMs = 30000;

bool waits(const QStringList &arguments);

struct Command
{
    QString program;
    QStringList arguments;
};

Command command(const QStringList &arguments, const QString &program, const QString &appImage);
bool start(const QStringList &arguments, QString *error);
}
