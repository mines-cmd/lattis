#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <utility>

// See docs/code/launch.md.

namespace LaunchOptions {
struct Word
{
    qsizetype begin = 0;
    qsizetype end = 0;
    QString text;
};

QList<Word> split(const QString &options, QChar *unclosed = nullptr);

QStringList words(const QString &options);

bool isAssignment(const QString &word);

struct Parts
{
    QList<std::pair<QString, QString>> env;
    QStringList wrapper;
    QStringList args;
    QString problem;

    bool operator==(const Parts &) const = default;
};

Parts parse(const QString &options);
}
