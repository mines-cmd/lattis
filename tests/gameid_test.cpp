#include "gameid.h"

#include <QTest>

class GameIdTest : public QObject
{
    Q_OBJECT

private slots:
    void slug_data();
    void slug();
    void isValid_data();
    void isValid();
    void forTitle();
    void forTitleCollisions();
    void forTitleWithoutLatinLetters();
    void forSteamApp();
};

void GameIdTest::slug_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expected");

    QTest::newRow("plain") << "Hades" << "hades";
    QTest::newRow("accent") << "Pokémon Ultra Sun" << "pokemon-ultra-sun";
    QTest::newRow("accents") << "Ōkami Déjà Vu Ñandú" << "okami-deja-vu-nandu";
    QTest::newRow("decomposed input") << QString::fromUtf8("Pokémon") << "pokemon";
    QTest::newRow("punctuation") << "The Legend of Zelda: Tears of the Kingdom"
                                 << "the-legend-of-zelda-tears-of-the-kingdom";
    QTest::newRow("apostrophe") << "Baldur's Gate 3" << "baldur-s-gate-3";
    QTest::newRow("dots") << "Super Smash Bros. Ultimate" << "super-smash-bros-ultimate";
    QTest::newRow("spaces") << "  Mario   Kart  " << "mario-kart";
    QTest::newRow("leading punctuation") << "...Hello!" << "hello";
    QTest::newRow("ligature") << "ﬁnal" << "final";
    QTest::newRow("fullwidth digits") << "Ｆ２" << "f2";
    QTest::newRow("japanese") << "ゼルダの伝説" << "";
    QTest::newRow("mixed scripts") << "Ys ⅷ ラクリモサ" << "ys-viii";
    QTest::newRow("cyrillic") << "Тетрис" << "";
    QTest::newRow("empty") << "" << "";
}

void GameIdTest::slug()
{
    QFETCH(QString, text);
    QFETCH(QString, expected);
    QCOMPARE(GameId::slug(text), expected);
}

void GameIdTest::isValid_data()
{
    QTest::addColumn<QString>("id");
    QTest::addColumn<bool>("valid");

    QTest::newRow("slug") << "switch-mario-kart-8-deluxe" << true;
    QTest::newRow("steam") << "steam-1145360" << true;
    QTest::newRow("digits") << "3ds" << true;
    QTest::newRow("uppercase") << "Switch-mario" << false;
    QTest::newRow("accent") << "3ds-pokémon" << false;
    QTest::newRow("underscore") << "my_game" << false;
    QTest::newRow("space") << "my game" << false;
    QTest::newRow("leading dash") << "-game" << false;
    QTest::newRow("trailing dash") << "game-" << false;
    QTest::newRow("empty") << "" << false;
}

void GameIdTest::isValid()
{
    QFETCH(QString, id);
    QFETCH(bool, valid);
    QCOMPARE(GameId::isValid(id), valid);
}

void GameIdTest::forTitle()
{
    QCOMPARE(GameId::forTitle("3ds", "Pokémon Ultra Sun", {}), QStringLiteral("3ds-pokemon-ultra-sun"));
    QVERIFY(GameId::isValid(GameId::forTitle("switch", "Mario Kart 8 Deluxe", {})));
}

void GameIdTest::forTitleCollisions()
{
    QSet<QString> taken{QStringLiteral("switch-tetris")};
    QCOMPARE(GameId::forTitle("switch", "Tetris", taken), QStringLiteral("switch-tetris-2"));
    taken.insert(QStringLiteral("switch-tetris-2"));
    QCOMPARE(GameId::forTitle("switch", "TETRIS!", taken), QStringLiteral("switch-tetris-3"));
    QCOMPARE(GameId::forTitle("3ds", "Tetris", taken), QStringLiteral("3ds-tetris"));
}

void GameIdTest::forTitleWithoutLatinLetters()
{
    QCOMPARE(GameId::forTitle("switch", "ゼルダの伝説", {}), QStringLiteral("switch-game"));
    const QSet<QString> taken{QStringLiteral("switch-game")};
    QCOMPARE(GameId::forTitle("switch", "テトリス", taken), QStringLiteral("switch-game-2"));
}

void GameIdTest::forSteamApp()
{
    QCOMPARE(GameId::forSteamApp(1145360), QStringLiteral("steam-1145360"));
}

QTEST_APPLESS_MAIN(GameIdTest)
#include "gameid_test.moc"
