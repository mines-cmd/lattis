#pragma once

#include <QByteArray>
#include <QList>
#include <QLoggingCategory>
#include <QString>
#include <QStringList>

Q_DECLARE_LOGGING_CATEGORY(lcSteam)

// See docs/code/steam.md.
namespace Vdf {
struct Node
{
    QString key;
    QString value;
    bool isBlock = false;
    QList<Node> children;

    int line = 0;
    qsizetype begin = 0;
    qsizetype valueBegin = 0; // a value's token (with its quotes), or a block's '{'
    qsizetype end = 0;        // after the value's token, or after the block's '}'

    const Node *child(QStringView key) const;
    const Node *find(const QStringList &path) const;
    QString valueOf(QStringView key, const QString &fallback = {}) const;
};

struct Document
{
    bool ok = false;
    QString error;
    Node root;
};

Document parse(const QByteArray &text);

struct EditResult
{
    bool ok = false;
    QByteArray text;
    QString error;
};

EditResult setValue(const QByteArray &text, const QStringList &path, const QString &value);
EditResult remove(const QByteArray &text, const QStringList &path);
QByteArray quoted(const QString &value);
}
