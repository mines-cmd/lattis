#pragma once

#include <QByteArray>
#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QMap>
#include <QMutex>
#include <QString>

#include <memory>
#include <optional>

// See docs/code/core.md.
class SecretStore
{
public:
    using Attributes = QMap<QString, QString>;

    virtual ~SecretStore() = default;

    virtual bool persistent() const = 0;
    virtual bool store(const QString &label, const Attributes &attributes, const QByteArray &secret,
                       QString *error = nullptr) = 0;
    virtual std::optional<QByteArray> read(const Attributes &attributes) = 0;
    virtual bool remove(const Attributes &attributes, QString *error = nullptr) = 0;

    // The Secret Service when something on the session bus answers for it, else memory only.
    static std::shared_ptr<SecretStore> create();
};

class MemorySecretStore : public SecretStore
{
public:
    bool persistent() const override { return false; }
    bool store(const QString &label, const Attributes &attributes, const QByteArray &secret,
               QString *error = nullptr) override;
    std::optional<QByteArray> read(const Attributes &attributes) override;
    bool remove(const Attributes &attributes, QString *error = nullptr) override;

private:
    QMutex m_mutex;
    QList<std::pair<Attributes, QByteArray>> m_items;
};

class SecretServiceStore : public SecretStore
{
public:
    static bool serviceAvailable();

    bool persistent() const override { return true; }
    bool store(const QString &label, const Attributes &attributes, const QByteArray &secret,
               QString *error = nullptr) override;
    std::optional<QByteArray> read(const Attributes &attributes) override;
    bool remove(const Attributes &attributes, QString *error = nullptr) override;
};

// The Secret Service's (oayays) struct; the test's fake service marshals the same one.
struct SecretServiceSecret
{
    QDBusObjectPath session;
    QByteArray parameters;
    QByteArray value;
    QString contentType;
};
Q_DECLARE_METATYPE(SecretServiceSecret)
QDBusArgument &operator<<(QDBusArgument &argument, const SecretServiceSecret &secret);
const QDBusArgument &operator>>(const QDBusArgument &argument, SecretServiceSecret &secret);

namespace SecretServiceTypes {
void registerAll();
}
