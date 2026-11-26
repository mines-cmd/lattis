#include "secretstore.h"

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDir>
#include <QDirIterator>
#include <QMutex>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QTimer>

namespace {
const QString kServicePath = QStringLiteral("/org/freedesktop/secrets");
const QString kCollectionPath = QStringLiteral("/org/freedesktop/secrets/collection/default");

struct FakeItem
{
    QString label;
    SecretStore::Attributes attributes;
    QByteArray secret;
};

struct FakeState
{
    QMutex mutex;
    QMap<QString, FakeItem> items;
    bool locked = false;
    bool dismissPrompts = false;
    // The failures a keyring can answer with.
    bool noDefaultCollection = false;
    bool refuseCreate = false;
    bool failGetSecret = false;
    bool failDelete = false;
    int nextItem = 1;
    QObject *itemRoot = nullptr;
};

FakeState &fake()
{
    static FakeState state;
    return state;
}

// Qt sets the context of a call on the exported object, not on its adaptor.
class FakeObject : public QObject, public QDBusContext
{
    Q_OBJECT

public:
    QString path;
};

void exportItem(const QString &path);

class FakeServiceAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Secret.Service")

public:
    explicit FakeServiceAdaptor(QObject *parent) : QDBusAbstractAdaptor(parent) {}

public slots:
    void OpenSession(const QString &algorithm, const QDBusVariant &, QDBusVariant &output, QDBusObjectPath &result)
    {
        QVERIFY2(algorithm == QLatin1String("plain"), "only plain sessions");
        output = QDBusVariant(QString());
        result = QDBusObjectPath(QStringLiteral("/org/freedesktop/secrets/session/1"));
    }

    void ReadAlias(const QString &name, QDBusObjectPath &collection)
    {
        QMutexLocker lock(&fake().mutex);
        const bool found = name == QLatin1String("default") && !fake().noDefaultCollection;
        collection = QDBusObjectPath(found ? kCollectionPath : QStringLiteral("/"));
    }

    void SearchItems(const SecretStore::Attributes &attributes, QList<QDBusObjectPath> &unlocked,
                     QList<QDBusObjectPath> &locked)
    {
        QMutexLocker lock(&fake().mutex);
        for (auto it = fake().items.cbegin(); it != fake().items.cend(); ++it) {
            bool matches = true;
            for (auto a = attributes.cbegin(); a != attributes.cend(); ++a)
                matches = matches && it->attributes.value(a.key()) == a.value();
            if (matches)
                (fake().locked ? locked : unlocked).append(QDBusObjectPath(it.key()));
        }
    }

    void Unlock(const QList<QDBusObjectPath> &, QList<QDBusObjectPath> &unlocked, QDBusObjectPath &prompt)
    {
        QMutexLocker lock(&fake().mutex);
        if (fake().locked) {
            prompt = QDBusObjectPath(QStringLiteral("/org/freedesktop/secrets/prompt/1"));
        } else {
            unlocked.append(QDBusObjectPath(kCollectionPath));
            prompt = QDBusObjectPath(QStringLiteral("/"));
        }
    }
};

class FakeCollectionAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Secret.Collection")

public:
    explicit FakeCollectionAdaptor(QObject *parent) : QDBusAbstractAdaptor(parent) {}

public slots:
    void CreateItem(const QVariantMap &properties, const SecretServiceSecret &secret, bool replace,
                    QDBusObjectPath &item, QDBusObjectPath &prompt)
    {
        QMutexLocker lock(&fake().mutex);
        if (fake().refuseCreate) {
            static_cast<FakeObject *>(parent())->sendErrorReply(
                QDBusError::AccessDenied, QStringLiteral("the fake keyring refuses new items"));
            return;
        }
        FakeItem created;
        created.label = properties.value(QStringLiteral("org.freedesktop.Secret.Item.Label")).toString();
        created.attributes = qdbus_cast<SecretStore::Attributes>(
            properties.value(QStringLiteral("org.freedesktop.Secret.Item.Attributes")));
        created.secret = secret.value;
        QString path;
        if (replace) {
            for (auto it = fake().items.cbegin(); it != fake().items.cend(); ++it) {
                if (it->attributes == created.attributes)
                    path = it.key();
            }
        }
        if (path.isEmpty())
            path = kCollectionPath + QLatin1Char('/') + QString::number(fake().nextItem++);
        fake().items.insert(path, created);
        exportItem(path);
        item = QDBusObjectPath(path);
        prompt = QDBusObjectPath(QStringLiteral("/"));
    }
};

class FakeItemAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Secret.Item")

public:
    explicit FakeItemAdaptor(QObject *parent) : QDBusAbstractAdaptor(parent) {}

public slots:
    void GetSecret(const QDBusObjectPath &session, SecretServiceSecret &secret)
    {
        QMutexLocker lock(&fake().mutex);
        if (fake().failGetSecret) {
            static_cast<FakeObject *>(parent())->sendErrorReply(
                QDBusError::Failed, QStringLiteral("the fake keyring can't read the item"));
            return;
        }
        secret = {session, {}, fake().items.value(path()).secret, QStringLiteral("text/plain")};
    }

    void Delete(QDBusObjectPath &prompt)
    {
        QMutexLocker lock(&fake().mutex);
        if (fake().failDelete) {
            static_cast<FakeObject *>(parent())->sendErrorReply(
                QDBusError::AccessDenied, QStringLiteral("the fake keyring won't delete the item"));
            return;
        }
        fake().items.remove(path());
        QDBusConnection(QStringLiteral("fake-keyring")).unregisterObject(path());
        parent()->deleteLater();
        prompt = QDBusObjectPath(QStringLiteral("/"));
    }

private:
    QString path() const { return static_cast<FakeObject *>(parent())->path; }
};

void exportItem(const QString &path)
{
    QDBusConnection bus(QStringLiteral("fake-keyring"));
    if (bus.objectRegisteredAt(path))
        return;
    auto *object = new FakeObject;
    object->setParent(fake().itemRoot);
    object->path = path;
    new FakeItemAdaptor(object);
    bus.registerObject(path, object, QDBusConnection::ExportAdaptors);
}

class FakePromptAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Secret.Prompt")

public:
    explicit FakePromptAdaptor(QObject *parent) : QDBusAbstractAdaptor(parent) {}

public slots:
    void Prompt(const QString &)
    {
        QTimer::singleShot(10, this, [this] {
            QMutexLocker lock(&fake().mutex);
            const bool dismissed = fake().dismissPrompts;
            if (!dismissed)
                fake().locked = false;
            emit Completed(dismissed, QDBusVariant(QVariant::fromValue(QDBusObjectPath(kCollectionPath))));
        });
    }

signals:
    void Completed(bool dismissed, const QDBusVariant &result);
};

// The Secret Service on its own thread, so the store's blocking calls from the test's thread get answered.
class FakeService : public QThread
{
public:
    QSemaphore ready;
    bool registered = false;

protected:
    void run() override
    {
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("fake-keyring"));
        FakeObject itemRoot;
        fake().itemRoot = &itemRoot;
        FakeObject service;
        new FakeServiceAdaptor(&service);
        FakeObject collection;
        new FakeCollectionAdaptor(&collection);
        FakeObject prompt;
        new FakePromptAdaptor(&prompt);
        registered = bus.registerService(QStringLiteral("org.freedesktop.secrets"))
            && bus.registerObject(kServicePath, &service, QDBusConnection::ExportAdaptors)
            && bus.registerObject(kCollectionPath, &collection, QDBusConnection::ExportAdaptors)
            && bus.registerObject(QStringLiteral("/org/freedesktop/secrets/prompt/1"), &prompt,
                                  QDBusConnection::ExportAdaptors);
        ready.release();
        exec();
        fake().itemRoot = nullptr;
    }
};

QStringList listing(const QString &dir)
{
    QStringList all;
    QDirIterator it(dir, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext())
        all.append(it.next());
    all.sort();
    return all;
}

const SecretStore::Attributes kAttributes = {{"application", "lattis"}, {"steamid", "76561198000000001"}};
}

class SecretStoreTest : public QObject
{
    Q_OBJECT

    FakeService *m_service = nullptr;

    static void resetFake()
    {
        QMutexLocker lock(&fake().mutex);
        fake().locked = false;
        fake().dismissPrompts = false;
        fake().noDefaultCollection = false;
        fake().refuseCreate = false;
        fake().failGetSecret = false;
        fake().failDelete = false;
    }

private slots:
    void initTestCase()
    {
        // Only a private bus, or none (see tests/CMakeLists.txt): anything else could reach the real keyring.
        if (qgetenv("LATTIS_TEST_NO_BUS") == "1") {
            QVERIFY(!QDBusConnection::sessionBus().isConnected());
            return;
        }
        if (qgetenv("LATTIS_TEST_PRIVATE_BUS") != "1")
            QSKIP("run through ctest: it starts a private session bus");
        SecretServiceTypes::registerAll();
        QVERIFY(!QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.freedesktop.secrets")));
    }

    // ctest runs this one alone, with a session bus address that leads nowhere.
    void withoutASessionBusNothingIsKept()
    {
        if (qgetenv("LATTIS_TEST_NO_BUS") != "1")
            QSKIP("runs as secretstore_nobus_test");
        QVERIFY(!SecretServiceStore::serviceAvailable());
        QVERIFY(!SecretStore::create()->persistent());
        SecretServiceStore store;
        QString error;
        QVERIFY(!store.store(QStringLiteral("Lattis Steam sign-in"), kAttributes, "refresh", &error));
        QCOMPARE(error, QStringLiteral("no session bus"));
        QVERIFY(!store.read(kAttributes));
        error.clear();
        QVERIFY(!store.remove(kAttributes, &error));
        QCOMPARE(error, QStringLiteral("no session bus"));
        // Without a place for the error, the call still fails cleanly.
        QVERIFY(!store.store(QStringLiteral("Lattis Steam sign-in"), kAttributes, "refresh"));
        QVERIFY(!store.remove(kAttributes));
    }

    void withoutAServiceItsMemoryOnly()
    {
        QVERIFY(!SecretServiceStore::serviceAvailable());
        const auto store = SecretStore::create();
        QVERIFY(!store->persistent());
        QVERIFY(!store->read(kAttributes));
        QVERIFY(store->store(QStringLiteral("Lattis Steam sign-in"), kAttributes, "token"));
        QCOMPARE(store->read(kAttributes), std::optional<QByteArray>("token"));
        QVERIFY(store->store(QStringLiteral("Lattis Steam sign-in"), kAttributes, "newer"));
        QCOMPARE(store->read(kAttributes), std::optional<QByteArray>("newer"));
        QVERIFY(!store->read({{"application", "lattis"}, {"steamid", "other"}}));
        QVERIFY(store->remove(kAttributes));
        QVERIFY(!store->read(kAttributes));
    }

    void startTheFakeService()
    {
        m_service = new FakeService;
        m_service->start();
        m_service->ready.acquire();
        QVERIFY(m_service->registered);
    }

    void usesTheServiceWhenThereIsOne()
    {
        QVERIFY(SecretServiceStore::serviceAvailable());
        QVERIFY(SecretStore::create()->persistent());
    }

    void storesReadsAndDeletesOneItem()
    {
        QTemporaryDir data;
        QVERIFY(data.isValid());
        qputenv("LATTIS_DATA", data.path().toUtf8());
        const QStringList before = listing(data.path());

        SecretServiceStore store;
        QString error;
        QVERIFY(!store.read(kAttributes));
        QVERIFY2(store.store(QStringLiteral("Lattis Steam sign-in"), kAttributes, "refresh-1", &error),
                 qPrintable(error));
        QCOMPARE(store.read(kAttributes), std::optional<QByteArray>("refresh-1"));
        {
            QMutexLocker lock(&fake().mutex);
            QCOMPARE(fake().items.size(), 1);
            QCOMPARE(fake().items.first().label, QStringLiteral("Lattis Steam sign-in"));
            QCOMPARE(fake().items.first().attributes, kAttributes);
        }

        QVERIFY(store.store(QStringLiteral("Lattis Steam sign-in"), kAttributes, "refresh-2", &error));
        QCOMPARE(store.read(kAttributes), std::optional<QByteArray>("refresh-2"));
        {
            QMutexLocker lock(&fake().mutex);
            QCOMPARE(fake().items.size(), 1);
        }
        QVERIFY(!store.read({{"application", "lattis"}, {"steamid", "76561198000000002"}}));

        QVERIFY2(store.remove(kAttributes, &error), qPrintable(error));
        QVERIFY(!store.read(kAttributes));
        QVERIFY(store.remove(kAttributes));
        QMutexLocker lock(&fake().mutex);
        QVERIFY(fake().items.isEmpty());
        lock.unlock();

        qunsetenv("LATTIS_DATA");
        QCOMPARE(listing(data.path()), before);
    }

    void aLockedKeyringIsUnlockedThroughItsPrompt()
    {
        SecretServiceStore store;
        QVERIFY(store.store(QStringLiteral("Lattis Steam sign-in"), kAttributes, "refresh"));
        {
            QMutexLocker lock(&fake().mutex);
            fake().locked = true;
        }
        QCOMPARE(store.read(kAttributes), std::optional<QByteArray>("refresh"));
        {
            QMutexLocker lock(&fake().mutex);
            QVERIFY(!fake().locked);
            fake().locked = true;
        }
        QVERIFY(store.remove(kAttributes));
        QMutexLocker lock(&fake().mutex);
        QVERIFY(fake().items.isEmpty());
    }

    void aDismissedPromptStoresNothing()
    {
        {
            QMutexLocker lock(&fake().mutex);
            fake().locked = true;
            fake().dismissPrompts = true;
        }
        SecretServiceStore store;
        QString error;
        QVERIFY(!store.store(QStringLiteral("Lattis Steam sign-in"), kAttributes, "refresh", &error));
        QVERIFY(!error.isEmpty());
        QMutexLocker lock(&fake().mutex);
        QVERIFY(fake().items.isEmpty());
    }

    void aKeyringWithoutADefaultCollectionStoresNothing()
    {
        resetFake();
        {
            QMutexLocker lock(&fake().mutex);
            fake().noDefaultCollection = true;
        }
        SecretServiceStore store;
        QString error;
        QVERIFY(!store.store(QStringLiteral("Lattis Steam sign-in"), kAttributes, "refresh", &error));
        QCOMPARE(error, QStringLiteral("the keyring has no default collection"));
        QVERIFY(!store.read(kAttributes));
        QMutexLocker lock(&fake().mutex);
        QVERIFY(fake().items.isEmpty());
    }

    void aRefusedCreateItemStoresNothing()
    {
        resetFake();
        {
            QMutexLocker lock(&fake().mutex);
            fake().refuseCreate = true;
        }
        SecretServiceStore store;
        QString error;
        QVERIFY(!store.store(QStringLiteral("Lattis Steam sign-in"), kAttributes, "refresh", &error));
        QCOMPARE(error, QStringLiteral("the fake keyring refuses new items"));
        QVERIFY(!store.read(kAttributes));
        QMutexLocker lock(&fake().mutex);
        QVERIFY(fake().items.isEmpty());
    }

    void aSecretThatCantBeReadIsNone()
    {
        resetFake();
        SecretServiceStore store;
        QVERIFY(store.store(QStringLiteral("Lattis Steam sign-in"), kAttributes, "refresh"));
        {
            QMutexLocker lock(&fake().mutex);
            fake().failGetSecret = true;
        }
        QVERIFY(!store.read(kAttributes));
        {
            QMutexLocker lock(&fake().mutex);
            QCOMPARE(fake().items.size(), 1);
            fake().failGetSecret = false;
        }
        // Nothing was lost: the next read gets it.
        QCOMPARE(store.read(kAttributes), std::optional<QByteArray>("refresh"));
        QVERIFY(store.remove(kAttributes));
    }

    void aDeleteThatFailsSaysSo()
    {
        resetFake();
        SecretServiceStore store;
        QVERIFY(store.store(QStringLiteral("Lattis Steam sign-in"), kAttributes, "refresh"));
        {
            QMutexLocker lock(&fake().mutex);
            fake().failDelete = true;
        }
        QString error;
        QVERIFY(!store.remove(kAttributes, &error));
        QCOMPARE(error, QStringLiteral("the fake keyring won't delete the item"));
        QVERIFY(!store.remove(kAttributes));
        QCOMPARE(store.read(kAttributes), std::optional<QByteArray>("refresh"));

        // A delete dismissed at the unlock prompt fails too, and keeps the item.
        {
            QMutexLocker lock(&fake().mutex);
            fake().failDelete = false;
            fake().locked = true;
            fake().dismissPrompts = true;
        }
        error.clear();
        QVERIFY(!store.remove(kAttributes, &error));
        QCOMPARE(error, QStringLiteral("the keyring prompt was dismissed"));
        QMutexLocker lock(&fake().mutex);
        QCOMPARE(fake().items.size(), 1);
        lock.unlock();

        resetFake();
        QVERIFY(store.remove(kAttributes));
        lock.relock();
        QVERIFY(fake().items.isEmpty());
    }

    void cleanupTestCase()
    {
        if (!m_service)
            return;
        m_service->quit();
        m_service->wait();
        delete m_service;
    }
};

QTEST_GUILESS_MAIN(SecretStoreTest)
#include "secretstore_test.moc"
