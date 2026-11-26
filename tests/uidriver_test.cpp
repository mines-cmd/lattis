#include "testfiles.h"
#include "uidriver.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

// The driver of the scripted UI tests, on a window of its own: what a script that can't go on
// says, and the exit code the harness reads.
class UiDriverTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void unreadableScript();
    void unknownCommand();
    void unknownKeys();
    void dragWithoutAnOffset();
    void itemThatNeverShows();
    void scriptThatPasses();

private:
    // Runs `script` to its end and returns the app's exit code.
    int run(const QByteArray &script);
    int runFile(const QString &path);
    QQuickItem *button() const { return m_window->contentItem()->childItems().value(0); }

    QTemporaryDir m_dir;
    QQmlEngine m_engine;
    std::unique_ptr<QQuickWindow> m_window;
};

void UiDriverTest::init()
{
    m_window = std::make_unique<QQuickWindow>();
    m_window->resize(300, 200);
    QQmlComponent component(&m_engine);
    component.setData("import QtQuick\n"
                      "Item {\n"
                      "    property string text: \"Hello\"\n"
                      "    property int clicks: 0\n"
                      "    width: 200; height: 100\n"
                      "    MouseArea { anchors.fill: parent; onClicked: parent.clicks++ }\n"
                      "}\n",
                      QUrl());
    auto *item = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(item, qPrintable(component.errorString()));
    item->setParent(m_window.get());
    item->setParentItem(m_window->contentItem());
    m_window->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
}

void UiDriverTest::cleanup()
{
    m_window.reset();
}

int UiDriverTest::runFile(const QString &path)
{
    UiDriver driver(m_window.get(), path);
    driver.start();
    return QCoreApplication::exec();
}

int UiDriverTest::run(const QByteArray &script)
{
    const QString path = m_dir.filePath(QStringLiteral("script.txt"));
    writeFile(path, script);
    return runFile(path);
}

void UiDriverTest::unreadableScript()
{
    const QString path = m_dir.filePath(QStringLiteral("missing.txt"));
    QTest::ignoreMessage(QtInfoMsg, qPrintable(QStringLiteral("uitest: FAIL can't read ") + path));
    QCOMPARE(runFile(path), 1);
}

void UiDriverTest::unknownCommand()
{
    QTest::ignoreMessage(QtInfoMsg, "uitest: FAIL line 3: unknown command frobnicate");
    QCOMPARE(run("# a comment\n\nfrobnicate Hello\nquit\n"), 1);
}

void UiDriverTest::unknownKeys()
{
    QTest::ignoreMessage(QtInfoMsg, "uitest: FAIL line 1: unknown key Ctrl+Nokey");
    QCOMPARE(run("key Ctrl+Nokey\n"), 1);
    QTest::ignoreMessage(QtInfoMsg, "uitest: FAIL line 1: unknown key Hyper+A");
    QCOMPARE(run("key Hyper+A\n"), 1);
}

void UiDriverTest::dragWithoutAnOffset()
{
    QTest::ignoreMessage(QtInfoMsg, "uitest: FAIL line 1: drag wants '<text> by <dx> <dy>'");
    QCOMPARE(run("drag Hello by 10\n"), 1);
}

// The 2 s the driver waits for an item, once.
void UiDriverTest::itemThatNeverShows()
{
    QTest::ignoreMessage(QtInfoMsg, "uitest: click Hello");
    QTest::ignoreMessage(QtInfoMsg,
                         QRegularExpression(QStringLiteral("^uitest: FAIL line 2: expect Goodbye didn't happen\\. "
                                                           "On screen: Hello$")));
    QCOMPARE(run("click Hello\nexpect Goodbye\nquit\n"), 1);
    QCOMPARE(button()->property("clicks").toInt(), 1); // the line before it ran
}

void UiDriverTest::scriptThatPasses()
{
    QTest::ignoreMessage(QtInfoMsg, "uitest: click Hello");
    QTest::ignoreMessage(QtInfoMsg, "uitest: done");
    QCOMPARE(run("expect Hello\nexpect-has ell\nexpect-not Goodbye\nexpect-not-has bye\nclick Hello\n"), 0);
    QCOMPARE(button()->property("clicks").toInt(), 1);
}

QTEST_MAIN(UiDriverTest)
#include "uidriver_test.moc"
