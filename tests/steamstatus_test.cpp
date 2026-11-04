#include "steamstatus.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace Qt::StringLiterals;

namespace {
QByteArray localConfig(const QString &account, const QByteArray &prefs, const QByteArray &extra = {})
{
    return "\"UserLocalConfigStore\"\n{\n\t\"WebStorage\"\n\t{\n\t\t\"FriendStoreLocalPrefs_" + account.toUtf8()
           + "\"\t\t\"" + prefs + "\"\n" + extra + "\t}\n}\n";
}

QByteArray persona(int state)
{
    return "{\\\"ePersonaState\\\":" + QByteArray::number(state) + ",\\\"strNonFriendsAllowedToMsg\\\":\\\"\\\"}";
}
}

class SteamStatusTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void parsesStates();
    void unknownIsEmpty();
    void followsAReplacedFile();
    void missingFileThenCreated();
    void changeOfAccount();
    void avatarPicture();
    void avatarFollowsAccount();
    void steamClosed();
    void debounces();
    void urls();

private:
    QString configDir(const QString &account) const { return m_dir->path() + u"/userdata/"_s + account + u"/config"_s; }
    QString filePath(const QString &account) const { return configDir(account) + u"/localconfig.vdf"_s; }
    QString avatarPath(const QString &account) const
    {
        return m_dir->path() + u"/config/avatarcache/"_s + QString::number(76561197960265728ULL + account.toULongLong())
               + u".png"_s;
    }
    void replaceAvatar(const QString &account, const QByteArray &content);
    void write(const QString &account, const QByteArray &content);
    // Steam saves by writing a new file over the old one, which changes the inode.
    void replace(const QString &account, const QByteArray &content);
    SteamStatus::Source source();

    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_user;
    bool m_running = true;
    QStringList m_opened;
};

void SteamStatusTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_user = u"1001"_s;
    m_running = true;
    m_opened.clear();
}

void SteamStatusTest::write(const QString &account, const QByteArray &content)
{
    QDir().mkpath(configDir(account));
    QFile file(filePath(account));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(content);
}

void SteamStatusTest::replace(const QString &account, const QByteArray &content)
{
    QDir().mkpath(configDir(account));
    const QString temp = filePath(account) + u".tmp"_s;
    QFile file(temp);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
    file.close();
    QVERIFY(QFile::remove(filePath(account)) || !QFile::exists(filePath(account)));
    QVERIFY(QFile::rename(temp, filePath(account)));
}

void SteamStatusTest::replaceAvatar(const QString &account, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(avatarPath(account)).absolutePath());
    const QString temp = avatarPath(account) + u".tmp"_s;
    QFile file(temp);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
    file.close();
    QFile::remove(avatarPath(account));
    QVERIFY(QFile::rename(temp, avatarPath(account)));
}

SteamStatus::Source SteamStatusTest::source()
{
    SteamStatus::Source s;
    s.install = [this] {
        SteamInstall install;
        install.kind = SteamInstall::Kind::Native;
        install.root = m_dir->path();
        install.userId = m_user;
        return install;
    };
    s.running = [this] { return m_running; };
    s.open = [this](const QString &url) {
        m_opened.append(url);
        return QString();
    };
    return s;
}

void SteamStatusTest::parsesStates()
{
    const QString id = u"1001"_s;
    QCOMPARE(SteamStatus::parse(localConfig(id, persona(1)), id), u"online"_s);
    QCOMPARE(SteamStatus::parse(localConfig(id, persona(3)), id), u"away"_s);
    QCOMPARE(SteamStatus::parse(localConfig(id, persona(7)), id), u"invisible"_s);
    for (const int other : {2, 4, 5, 6})
        QCOMPARE(SteamStatus::parse(localConfig(id, persona(other)), id), u"away"_s);
    QCOMPARE(SteamStatus::parse(localConfig(id, persona(0)), id), QString());
    QCOMPARE(SteamStatus::parse(localConfig(id, persona(12)), id), QString());
}

void SteamStatusTest::unknownIsEmpty()
{
    const QString id = u"1001"_s;
    // No such key, another account's key, JSON that doesn't parse, a state that isn't a number, a block
    // where the string should be, and text that isn't VDF.
    QCOMPARE(SteamStatus::parse("\"UserLocalConfigStore\"\n{\n\t\"WebStorage\"\n\t{\n\t}\n}\n", id), QString());
    QCOMPARE(SteamStatus::parse(localConfig(u"2002"_s, persona(1)), id), QString());
    QCOMPARE(SteamStatus::parse(localConfig(id, "{\\\"ePersonaState\\\":"), id), QString());
    QCOMPARE(SteamStatus::parse(localConfig(id, "not json"), id), QString());
    QCOMPARE(SteamStatus::parse(localConfig(id, "{\\\"ePersonaState\\\":\\\"1\\\"}"), id), QString());
    QCOMPARE(SteamStatus::parse(localConfig(id, "{\\\"other\\\":1}"), id), QString());
    QCOMPARE(SteamStatus::parse("\"UserLocalConfigStore\"\n{\n\t\"WebStorage\"\n\t{\n\t\t\"FriendStoreLocalPrefs_1001\"\n"
                                "\t\t{\n\t\t}\n\t}\n}\n",
                                id),
             QString());
    QCOMPARE(SteamStatus::parse("{{{ \"", id), QString());
    QCOMPARE(SteamStatus::parse({}, id), QString());
}

void SteamStatusTest::followsAReplacedFile()
{
    write(m_user, localConfig(m_user, persona(1)));
    SteamStatus status(source());
    QCOMPARE(status.status(), QString());
    status.start();
    QCOMPARE(status.status(), u"online"_s);
    QVERIFY(status.running());

    // Each save replaces the file, which drops its watch; the next one must still be seen.
    replace(m_user, localConfig(m_user, persona(3)));
    QTRY_COMPARE_WITH_TIMEOUT(status.status(), u"away"_s, 3000);
    replace(m_user, localConfig(m_user, persona(7)));
    QTRY_COMPARE_WITH_TIMEOUT(status.status(), u"invisible"_s, 3000);
    replace(m_user, localConfig(m_user, persona(1)));
    QTRY_COMPARE_WITH_TIMEOUT(status.status(), u"online"_s, 3000);
}

void SteamStatusTest::missingFileThenCreated()
{
    SteamStatus status(source());
    status.setPollMs(50);
    status.start();
    QCOMPARE(status.status(), QString());

    // Neither the folder nor the file exists yet.
    QDir().mkpath(m_dir->path() + u"/userdata"_s);
    write(m_user, localConfig(m_user, persona(3)));
    QTRY_COMPARE_WITH_TIMEOUT(status.status(), u"away"_s, 3000);

    // Bad content after a good one: unknown again, no warning, no crash.
    replace(m_user, "garbage\n");
    QTRY_COMPARE_WITH_TIMEOUT(status.status(), QString(), 3000);
    replace(m_user, localConfig(m_user, "{\\\"ePersonaState\\\":"));
    replace(m_user, localConfig(m_user, persona(1)));
    QTRY_COMPARE_WITH_TIMEOUT(status.status(), u"online"_s, 3000);
}

void SteamStatusTest::changeOfAccount()
{
    write(u"1001"_s, localConfig(u"1001"_s, persona(1)));
    write(u"2002"_s, localConfig(u"2002"_s, persona(7)));
    SteamStatus status(source());
    status.setPollMs(50);
    status.start();
    QCOMPARE(status.status(), u"online"_s);

    m_user = u"2002"_s;
    QTRY_COMPARE_WITH_TIMEOUT(status.status(), u"invisible"_s, 3000);

    // The old account's file is no longer watched; the new one's is.
    replace(u"1001"_s, localConfig(u"1001"_s, persona(3)));
    QTest::qWait(400);
    QCOMPARE(status.status(), u"invisible"_s);
    replace(u"2002"_s, localConfig(u"2002"_s, persona(3)));
    QTRY_COMPARE_WITH_TIMEOUT(status.status(), u"away"_s, 3000);

    // No user at all.
    m_user.clear();
    QTRY_COMPARE_WITH_TIMEOUT(status.status(), QString(), 3000);
}

void SteamStatusTest::avatarPicture()
{
    write(m_user, localConfig(m_user, persona(1)));
    SteamStatus status(source());
    status.setPollMs(50);
    status.start();
    QCOMPARE(status.avatar(), QString());

    replaceAvatar(m_user, "first");
    QTRY_VERIFY_WITH_TIMEOUT(status.avatar().startsWith(u"file://"_s + avatarPath(m_user) + u"?v="_s), 3000);
    const QString first = status.avatar();

    // A replaced picture keeps its path and changes the URL, so QML loads it again; and the next
    // replace is seen too.
    replaceAvatar(m_user, "second, longer");
    QTRY_VERIFY_WITH_TIMEOUT(status.avatar() != first && !status.avatar().isEmpty(), 3000);
    const QString second = status.avatar();
    replaceAvatar(m_user, "third");
    QTRY_VERIFY_WITH_TIMEOUT(status.avatar() != second && !status.avatar().isEmpty(), 3000);

    QVERIFY(QFile::remove(avatarPath(m_user)));
    QTRY_COMPARE_WITH_TIMEOUT(status.avatar(), QString(), 3000);
    QCOMPARE(status.status(), u"online"_s);
}

void SteamStatusTest::avatarFollowsAccount()
{
    write(u"1001"_s, localConfig(u"1001"_s, persona(1)));
    write(u"2002"_s, localConfig(u"2002"_s, persona(1)));
    replaceAvatar(u"1001"_s, "one");
    SteamStatus status(source());
    status.setPollMs(50);
    status.start();
    QVERIFY(status.avatar().contains(avatarPath(u"1001"_s)));

    // The second account has no picture.
    m_user = u"2002"_s;
    QTRY_COMPARE_WITH_TIMEOUT(status.avatar(), QString(), 3000);
    replaceAvatar(u"2002"_s, "two");
    QTRY_VERIFY_WITH_TIMEOUT(status.avatar().contains(avatarPath(u"2002"_s)), 3000);

    // The first account's picture is no longer watched.
    const QString shown = status.avatar();
    replaceAvatar(u"1001"_s, "one again");
    QTest::qWait(400);
    QCOMPARE(status.avatar(), shown);
}

void SteamStatusTest::steamClosed()
{
    write(m_user, localConfig(m_user, persona(3)));
    SteamStatus status(source());
    status.setPollMs(50);
    status.start();
    QCOMPARE(status.status(), u"away"_s);

    // The file keeps the last status; a closed Steam shows none.
    m_running = false;
    QTRY_COMPARE_WITH_TIMEOUT(status.running(), false, 3000);
    QCOMPARE(status.status(), QString());
    m_running = true;
    QTRY_COMPARE_WITH_TIMEOUT(status.status(), u"away"_s, 3000);
}

void SteamStatusTest::debounces()
{
    write(m_user, localConfig(m_user, persona(1)));
    SteamStatus status(source());
    status.start();
    QSignalSpy spy(&status, &SteamStatus::changed);
    for (const int state : {3, 7, 3, 1, 3})
        replace(m_user, localConfig(m_user, persona(state)));
    QTRY_COMPARE_WITH_TIMEOUT(status.status(), u"away"_s, 3000);
    QTest::qWait(500);
    QCOMPARE(spy.count(), 1);
}

void SteamStatusTest::urls()
{
    write(m_user, localConfig(m_user, persona(1)));
    SteamStatus status(source());
    status.start();
    QCOMPARE(status.setStatus(u"away"_s), QString());
    QCOMPARE(status.setStatus(u"invisible"_s), QString());
    QCOMPARE(status.setStatus(u"online"_s), QString());
    QCOMPARE(status.openFriends(), QString());
    QVERIFY(!status.setStatus(u"busy"_s).isEmpty());
    QCOMPARE(m_opened, (QStringList{u"steam://friends/status/away"_s, u"steam://friends/status/invisible"_s,
                                    u"steam://friends/status/online"_s, u"steam://open/friends"_s}));
    // What Steam says decides the status, not what was asked for.
    QCOMPARE(status.status(), u"online"_s);
}

QTEST_MAIN(SteamStatusTest)
#include "steamstatus_test.moc"
