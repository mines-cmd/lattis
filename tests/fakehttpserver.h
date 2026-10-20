#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

// A tiny HTTP/1.1 server on 127.0.0.1 answering canned responses by path, so the metadata
// tests never reach the network. Records every request.
class FakeHttpServer : public QObject
{
public:
    struct Response
    {
        int status = 200;
        QByteArray body;
        int delayMs = 0;
    };

    struct Request
    {
        QByteArray method;
        QByteArray target; // path and query, as sent
        QHash<QByteArray, QByteArray> headers; // lowercase names
        QByteArray body;
        qint64 atMs = 0; // when it arrived, ms since the epoch
    };

    FakeHttpServer()
    {
        m_server.listen(QHostAddress::LocalHost);
        QObject::connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection())
                QObject::connect(socket, &QTcpSocket::readyRead, this, [this, socket] { read(socket); });
        });
    }

    QUrl url(const QString &path = {}) const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(m_server.serverPort()).arg(path));
    }

    // Answers requests whose path (without the query) is `path`. Several responses for one
    // path are used in turn, the last one repeating.
    void on(const QByteArray &path, Response response) { m_responses[path].append(response); }
    void clear(const QByteArray &path) { m_responses.remove(path); }

    QList<Request> requests;

    QList<Request> requestsTo(const QByteArray &path) const
    {
        QList<Request> out;
        for (const Request &r : requests) {
            if (r.target.split('?').first() == path)
                out.append(r);
        }
        return out;
    }

private:
    void read(QTcpSocket *socket)
    {
        QByteArray &buffer = m_buffers[socket];
        buffer += socket->readAll();
        const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;
        Request request;
        const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
        const QList<QByteArray> first = lines.first().trimmed().split(' ');
        request.method = first.value(0);
        request.target = first.value(1);
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const qsizetype colon = lines[i].indexOf(':');
            if (colon > 0)
                request.headers.insert(lines[i].left(colon).trimmed().toLower(), lines[i].mid(colon + 1).trimmed());
        }
        const qsizetype length = request.headers.value("content-length", "0").toLongLong();
        if (buffer.size() < headerEnd + 4 + length)
            return;
        request.body = buffer.mid(headerEnd + 4, length);
        request.atMs = QDateTime::currentMSecsSinceEpoch();
        m_buffers.remove(socket);
        requests.append(request);

        const QByteArray path = request.target.split('?').first();
        Response response{404, "{}"};
        if (auto it = m_responses.find(path); it != m_responses.end() && !it->isEmpty())
            response = it->size() > 1 ? it->takeFirst() : it->first();
        const auto reply = [socket, response] {
            if (socket->state() != QAbstractSocket::ConnectedState)
                return;
            socket->write("HTTP/1.1 " + QByteArray::number(response.status) + " X\r\nContent-Type: "
                          "application/json\r\nConnection: close\r\nContent-Length: "
                          + QByteArray::number(response.body.size()) + "\r\n\r\n" + response.body);
            socket->disconnectFromHost();
        };
        if (response.delayMs > 0)
            QTimer::singleShot(response.delayMs, socket, reply);
        else
            reply();
    }

    QTcpServer m_server;
    QHash<QByteArray, QList<Response>> m_responses;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};
