#pragma once

#include <QList>
#include <QString>

#include <functional>

// See docs/code/library.md.
namespace LibraryVersion {
constexpr int kCurrent = 1;

struct Upgrade
{
    int from;
    std::function<QString(const QString &libraryDir)> run;
};

QList<Upgrade> upgrades();

struct Status
{
    int version = kCurrent;
    bool readOnly = false;
    QString notice;
    QString reason;
    QStringList problems;
    QString id;
    bool created = false;
    bool setupPending = false;
};

Status prepare(const QString &dataDir, int appVersion = kCurrent, const QList<Upgrade> &steps = upgrades(),
               bool withSetup = false);
}
