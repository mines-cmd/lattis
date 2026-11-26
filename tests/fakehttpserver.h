#pragma once

#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QList>
#include <QNetworkProxy>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

#include <functional>

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
        QByteArray extraHeaders; // raw lines, each ending in \r\n
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
        m_server.make = [this](qintptr descriptor) { return accept(descriptor); };
        m_server.listen(QHostAddress::LocalHost);
        QObject::connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection())
                QObject::connect(socket, &QTcpSocket::readyRead, this, [this, socket] { read(socket); });
        });
    }

    // Answers over TLS with the certificate in `folder` (cert.pem, key.pem), which this process then trusts. With
    // `proxy`, the server is an HTTP proxy instead: it answers CONNECT and then speaks TLS in the tunnel as whatever
    // host the certificate names (api.github.com, localhost, 127.0.0.1), so code with a fixed https: address reaches
    // it once QNetworkProxy::setApplicationProxy points at proxyAddress().
    bool useTls(const QString &folder, bool proxy = false)
    {
        QFile cert(folder + QStringLiteral("/cert.pem"));
        QFile key(folder + QStringLiteral("/key.pem"));
        if (!cert.open(QIODevice::ReadOnly) || !key.open(QIODevice::ReadOnly))
            return false;
        m_cert = QSslCertificate(cert.readAll());
        m_key = QSslKey(key.readAll(), QSsl::Ec);
        if (m_cert.isNull() || m_key.isNull())
            return false;
        QSslConfiguration trusted = QSslConfiguration::defaultConfiguration();
        if (!trusted.caCertificates().contains(m_cert)) {
            trusted.addCaCertificate(m_cert);
            QSslConfiguration::setDefaultConfiguration(trusted);
        }
        m_tls = true;
        m_proxy = proxy;
        return true;
    }

    QUrl url(const QString &path = {}) const
    {
        return QUrl(QStringLiteral("%1://127.0.0.1:%2%3")
                        .arg(m_tls && !m_proxy ? QStringLiteral("https") : QStringLiteral("http"))
                        .arg(m_server.serverPort())
                        .arg(path));
    }

    QNetworkProxy proxyAddress() const
    {
        return QNetworkProxy(QNetworkProxy::HttpProxy, QStringLiteral("127.0.0.1"), m_server.serverPort());
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
    class Listener : public QTcpServer
    {
    public:
        std::function<QTcpSocket *(qintptr)> make;

    protected:
        void incomingConnection(qintptr descriptor) override { addPendingConnection(make(descriptor)); }
    };

    QTcpSocket *accept(qintptr descriptor)
    {
        if (!m_tls) {
            auto *socket = new QTcpSocket(&m_server);
            socket->setSocketDescriptor(descriptor);
            return socket;
        }
        auto *socket = new QSslSocket(&m_server);
        socket->setSocketDescriptor(descriptor);
        socket->setLocalCertificate(m_cert);
        socket->setPrivateKey(m_key);
        socket->setPeerVerifyMode(QSslSocket::VerifyNone);
        if (!m_proxy)
            socket->startServerEncryption();
        return socket;
    }

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

        if (request.method == "CONNECT" && m_proxy) {
            auto *tls = static_cast<QSslSocket *>(socket);
            tls->write("HTTP/1.1 200 Connection established\r\n\r\n");
            tls->startServerEncryption();
            return;
        }

        const QByteArray path = request.target.split('?').first();
        Response response{404, "{}"};
        if (auto it = m_responses.find(path); it != m_responses.end() && !it->isEmpty())
            response = it->size() > 1 ? it->takeFirst() : it->first();
        const auto reply = [socket, response] {
            if (socket->state() != QAbstractSocket::ConnectedState)
                return;
            socket->write("HTTP/1.1 " + QByteArray::number(response.status) + " X\r\nContent-Type: "
                          "application/json\r\nConnection: close\r\n" + response.extraHeaders
                          + "Content-Length: " + QByteArray::number(response.body.size()) + "\r\n\r\n"
                          + response.body);
            socket->disconnectFromHost();
        };
        if (response.delayMs > 0)
            QTimer::singleShot(response.delayMs, socket, reply);
        else
            reply();
    }

    Listener m_server;
    QHash<QByteArray, QList<Response>> m_responses;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    bool m_tls = false;
    bool m_proxy = false;
    QSslCertificate m_cert;
    QSslKey m_key;
};
