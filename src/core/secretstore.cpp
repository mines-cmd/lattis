#include "secretstore.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QEventLoop>
#include <QTimer>

#include <atomic>

namespace {
const QString kService = QStringLiteral("org.freedesktop.secrets");
const QString kServicePath = QStringLiteral("/org/freedesktop/secrets");
const QString kServiceInterface = QStringLiteral("org.freedesktop.Secret.Service");
const QString kCollectionInterface = QStringLiteral("org.freedesktop.Secret.Collection");
const QString kItemInterface = QStringLiteral("org.freedesktop.Secret.Item");
const QString kPromptInterface = QStringLiteral("org.freedesktop.Secret.Prompt");
const QString kDefaultAlias = QStringLiteral("default");
constexpr int kCallTimeoutMs = 30000;
constexpr int kPromptTimeoutMs = 120000;

bool isPath(const QDBusObjectPath &path)
{
    return !path.path().isEmpty() && path.path() != QLatin1String("/");
}

class PromptWaiter : public QObject
{
    Q_OBJECT

public:
    QEventLoop loop;
    bool dismissed = true;

public slots:
    void Completed(bool wasDismissed, const QDBusVariant &)
    {
        dismissed = wasDismissed;
        loop.quit();
    }
};

// One bus connection per operation: the store is used from worker threads, and a connection belongs to
// the thread that made it.
class Session
{
public:
    Session()
        : m_name(QStringLiteral("lattis-secrets-%1").arg(counter().fetch_add(1)))
        , m_bus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, m_name))
    {
    }
    ~Session()
    {
        if (!m_sessionPath.isEmpty())
            call(m_sessionPath, QStringLiteral("org.freedesktop.Secret.Session"), QStringLiteral("Close"), {});
        m_bus = QDBusConnection(QString());
        QDBusConnection::disconnectFromBus(m_name);
    }

    bool open(QString *error)
    {
        if (!m_bus.isConnected()) {
            *error = QStringLiteral("no session bus");
            return false;
        }
        const QDBusMessage reply = call(kServicePath, kServiceInterface, QStringLiteral("OpenSession"),
                                        {QStringLiteral("plain"), QVariant::fromValue(QDBusVariant(QString()))});
        if (!ok(reply, error) || reply.arguments().size() < 2)
            return false;
        m_sessionPath = qvariant_cast<QDBusObjectPath>(reply.arguments().at(1)).path();
        return true;
    }

    QString sessionPath() const { return m_sessionPath; }

    QDBusMessage call(const QString &path, const QString &interface, const QString &method,
                      const QVariantList &arguments)
    {
        QDBusMessage message = QDBusMessage::createMethodCall(kService, path, interface, method);
        message.setArguments(arguments);
        return m_bus.call(message, QDBus::Block, kCallTimeoutMs);
    }

    static bool ok(const QDBusMessage &reply, QString *error)
    {
        if (reply.type() == QDBusMessage::ReplyMessage)
            return true;
        if (error)
            *error = reply.errorMessage().isEmpty() ? reply.errorName() : reply.errorMessage();
        return false;
    }

    // Unlocking and deleting may ask the user (a prompt the service shows itself); false when dismissed.
    bool runPrompt(const QDBusObjectPath &prompt, QString *error)
    {
        if (!isPath(prompt))
            return true;
        PromptWaiter waiter;
        m_bus.connect(kService, prompt.path(), kPromptInterface, QStringLiteral("Completed"), &waiter,
                      SLOT(Completed(bool, QDBusVariant)));
        const QDBusMessage reply = call(prompt.path(), kPromptInterface, QStringLiteral("Prompt"), {QString()});
        if (!ok(reply, error))
            return false;
        QTimer::singleShot(kPromptTimeoutMs, &waiter.loop, &QEventLoop::quit);
        waiter.loop.exec();
        m_bus.disconnect(kService, prompt.path(), kPromptInterface, QStringLiteral("Completed"), &waiter,
                         SLOT(Completed(bool, QDBusVariant)));
        if (waiter.dismissed && error)
            *error = QStringLiteral("the keyring prompt was dismissed");
        return !waiter.dismissed;
    }

    std::optional<QDBusObjectPath> defaultCollection(QString *error)
    {
        const QDBusMessage reply = call(kServicePath, kServiceInterface, QStringLiteral("ReadAlias"), {kDefaultAlias});
        if (!ok(reply, error) || reply.arguments().isEmpty())
            return std::nullopt;
        const auto path = qvariant_cast<QDBusObjectPath>(reply.arguments().first());
        if (!isPath(path)) {
            if (error)
                *error = QStringLiteral("the keyring has no default collection");
            return std::nullopt;
        }
        return path;
    }

    // Unlock() answers the objects that are open afterwards; whatever is not in it stays locked.
    bool unlock(const QList<QDBusObjectPath> &objects, QString *error)
    {
        if (objects.isEmpty())
            return true;
        QVariantList arguments;
        arguments.append(QVariant::fromValue(objects));
        const QDBusMessage reply = call(kServicePath, kServiceInterface, QStringLiteral("Unlock"), arguments);
        if (!ok(reply, error) || reply.arguments().size() < 2)
            return false;
        return runPrompt(qvariant_cast<QDBusObjectPath>(reply.arguments().at(1)), error);
    }

    QList<QDBusObjectPath> search(const SecretStore::Attributes &attributes, QString *error)
    {
        QVariantList arguments;
        arguments.append(QVariant::fromValue(attributes));
        const QDBusMessage reply = call(kServicePath, kServiceInterface, QStringLiteral("SearchItems"), arguments);
        if (!ok(reply, error) || reply.arguments().size() < 2)
            return {};
        QList<QDBusObjectPath> found = qdbus_cast<QList<QDBusObjectPath>>(reply.arguments().at(0));
        const QList<QDBusObjectPath> locked = qdbus_cast<QList<QDBusObjectPath>>(reply.arguments().at(1));
        if (!locked.isEmpty() && unlock(locked, error))
            found.append(locked);
        return found;
    }

private:
    static std::atomic<int> &counter()
    {
        static std::atomic<int> value{0};
        return value;
    }

    QString m_name;
    QDBusConnection m_bus;
    QString m_sessionPath;
};
}

namespace SecretServiceTypes {
void registerAll()
{
    static const bool done = [] {
        qDBusRegisterMetaType<SecretServiceSecret>();
        qDBusRegisterMetaType<SecretStore::Attributes>();
        return true;
    }();
    Q_UNUSED(done)
}
}

QDBusArgument &operator<<(QDBusArgument &argument, const SecretServiceSecret &secret)
{
    argument.beginStructure();
    argument << secret.session << secret.parameters << secret.value << secret.contentType;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, SecretServiceSecret &secret)
{
    argument.beginStructure();
    argument >> secret.session >> secret.parameters >> secret.value >> secret.contentType;
    argument.endStructure();
    return argument;
}

std::shared_ptr<SecretStore> SecretStore::create()
{
    if (SecretServiceStore::serviceAvailable())
        return std::make_shared<SecretServiceStore>();
    return std::make_shared<MemorySecretStore>();
}

bool MemorySecretStore::store(const QString &, const Attributes &attributes, const QByteArray &secret, QString *)
{
    QMutexLocker lock(&m_mutex);
    for (auto &item : m_items) {
        if (item.first == attributes) {
            item.second = secret;
            return true;
        }
    }
    m_items.append({attributes, secret});
    return true;
}

std::optional<QByteArray> MemorySecretStore::read(const Attributes &attributes)
{
    QMutexLocker lock(&m_mutex);
    for (const auto &item : std::as_const(m_items)) {
        if (item.first == attributes)
            return item.second;
    }
    return std::nullopt;
}

bool MemorySecretStore::remove(const Attributes &attributes, QString *)
{
    QMutexLocker lock(&m_mutex);
    m_items.removeIf([&](const auto &item) { return item.first == attributes; });
    return true;
}

bool SecretServiceStore::serviceAvailable()
{
    SecretServiceTypes::registerAll();
    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface())
        return false;
    if (bus.interface()->isServiceRegistered(kService))
        return true;
    const QDBusReply<QStringList> activatable = bus.interface()->activatableServiceNames();
    return activatable.isValid() && activatable.value().contains(kService);
}

bool SecretServiceStore::store(const QString &label, const Attributes &attributes, const QByteArray &secret,
                               QString *error)
{
    SecretServiceTypes::registerAll();
    QString failure;
    QString &why = error ? *error : failure;
    Session session;
    if (!session.open(&why))
        return false;
    const auto collection = session.defaultCollection(&why);
    if (!collection || !session.unlock({*collection}, &why))
        return false;

    QVariantMap properties;
    properties.insert(QStringLiteral("org.freedesktop.Secret.Item.Label"), label);
    properties.insert(QStringLiteral("org.freedesktop.Secret.Item.Attributes"), QVariant::fromValue(attributes));
    const SecretServiceSecret wrapped{QDBusObjectPath(session.sessionPath()), {}, secret,
                                      QStringLiteral("text/plain")};
    QVariantList arguments;
    arguments.append(properties);
    arguments.append(QVariant::fromValue(wrapped));
    arguments.append(true);
    const QDBusMessage reply = session.call(collection->path(), kCollectionInterface,
                                            QStringLiteral("CreateItem"), arguments);
    if (!Session::ok(reply, &why) || reply.arguments().size() < 2)
        return false;
    return session.runPrompt(qvariant_cast<QDBusObjectPath>(reply.arguments().at(1)), &why);
}

std::optional<QByteArray> SecretServiceStore::read(const Attributes &attributes)
{
    SecretServiceTypes::registerAll();
    QString error;
    Session session;
    if (!session.open(&error))
        return std::nullopt;
    const QList<QDBusObjectPath> items = session.search(attributes, &error);
    if (items.isEmpty())
        return std::nullopt;
    const QDBusMessage reply = session.call(items.first().path(), kItemInterface, QStringLiteral("GetSecret"),
                                            {QVariant::fromValue(QDBusObjectPath(session.sessionPath()))});
    if (!Session::ok(reply, &error) || reply.arguments().isEmpty())
        return std::nullopt;
    return qdbus_cast<SecretServiceSecret>(reply.arguments().first()).value;
}

bool SecretServiceStore::remove(const Attributes &attributes, QString *error)
{
    SecretServiceTypes::registerAll();
    QString failure;
    QString &why = error ? *error : failure;
    Session session;
    if (!session.open(&why))
        return false;
    const QList<QDBusObjectPath> items = session.search(attributes, &why);
    bool allGone = why.isEmpty();
    for (const QDBusObjectPath &item : items) {
        const QDBusMessage reply = session.call(item.path(), kItemInterface, QStringLiteral("Delete"), {});
        if (!Session::ok(reply, &why) || reply.arguments().isEmpty()
            || !session.runPrompt(qvariant_cast<QDBusObjectPath>(reply.arguments().first()), &why))
            allGone = false;
    }
    return allGone;
}

#include "secretstore.moc"
