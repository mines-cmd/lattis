#include "tomlstring.h"

#include <QTest>

#include <toml++/toml.hpp>

class TomlStringTest : public QObject
{
    Q_OBJECT

private slots:
    void everyAsciiCharacterRoundTrips();
    void textBeyondAsciiRoundTrips();
    void aLineBreakAddsNoLine();
};

namespace {
QString parsedBack(const QString &text)
{
    const toml::table table = toml::parse(("key = " + tomlQuote(text) + "\n").toStdString());
    const std::string value = table["key"].value_or(std::string());
    return QString::fromUtf8(value.data(), qsizetype(value.size()));
}
}

void TomlStringTest::everyAsciiCharacterRoundTrips()
{
    for (int code = 0; code < 0x80; ++code) {
        const QString text = QStringLiteral("a") + QChar(code) + QStringLiteral("b");
        QCOMPARE(parsedBack(text), text);
        QCOMPARE(parsedBack(QString(QChar(code))), QString(QChar(code)));
    }
}

void TomlStringTest::textBeyondAsciiRoundTrips()
{
    for (const QString &text : {QStringLiteral("Pokémon"), QStringLiteral("日本語"), QStringLiteral("🎮 game"),
                                QStringLiteral("a\u0085b"), QStringLiteral("\u2028"), QStringLiteral("C:\\x\\\"y\""),
                                QString()})
        QCOMPARE(parsedBack(text), text);
}

void TomlStringTest::aLineBreakAddsNoLine()
{
    const QString hostile = QStringLiteral("x\"]\n[metadata]\nlook_up_new_games = true\n#");
    const toml::table table = toml::parse(("[a]\nkey = " + tomlQuote(hostile) + "\n").toStdString());
    QCOMPARE(table.size(), size_t(1));
    QVERIFY(!table.contains("metadata"));
}

QTEST_MAIN(TomlStringTest)
#include "tomlstring_test.moc"
