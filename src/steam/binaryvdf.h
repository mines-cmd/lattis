#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QList>
#include <QString>

// See docs/code/steam.md.
namespace BinaryVdf {
enum Type : quint8 {
    Map = 0x00,
    String = 0x01, // NUL-terminated UTF-8
    Int32 = 0x02,
    Float = 0x03,
    Pointer = 0x04,
    WideString = 0x05, // UTF-16, ended by two NUL bytes
    Color = 0x06,
    UInt64 = 0x07,
    Int64 = 0x0A,
};

struct Node
{
    Type type = Map;
    QByteArray key;
    QByteArray data;
    QList<Node> children;

    const Node *child(QByteArrayView key) const;
    Node *child(QByteArrayView key);

    QString toString() const;
    qint32 toInt32() const;
    quint64 toUInt64() const;

    static Node string(const QByteArray &key, const QString &value);
    static Node int32(const QByteArray &key, qint32 value);
    static Node uint64(const QByteArray &key, quint64 value);
    static Node map(const QByteArray &key, const QList<Node> &children = {});

    bool operator==(const Node &) const = default;
};

struct Document
{
    bool ok = false;
    QString error;
    Node root;
};

Document parse(const QByteArray &bytes);

QByteArray write(const Node &root);
}
