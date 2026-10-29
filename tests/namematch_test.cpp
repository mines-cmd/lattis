#include "namematch.h"

#include <QTest>

class NameMatchTest : public QObject
{
    Q_OBJECT

private slots:
    void compactKey_data();
    void compactKey();
    void clearMatch_data();
    void clearMatch();
    void withoutTrailingParenthetical();
    void splitCamelCase();
    void tieBreak();
};

void NameMatchTest::compactKey_data()
{
    QTest::addColumn<QString>("a");
    QTest::addColumn<QString>("b");
    QTest::addColumn<bool>("same");

    QTest::newRow("space in a word") << "TriForce Heroes" << "Tri Force Heroes" << true;
    QTest::newRow("hyphen") << "Spider-Man" << "Spiderman" << true;
    QTest::newRow("leading the") << "The Legend of Zelda" << "Legend of Zelda" << true;
    QTest::newRow("leading a") << "A Hat in Time" << "Hat in Time" << true;
    QTest::newRow("ampersand and and") << "Mario & Luigi" << "Mario and Luigi" << true;
    QTest::newRow("vs") << "Layton vs Wright" << "Layton vs. Wright" << true;
    QTest::newRow("versus") << "Layton versus Wright" << "Layton vs. Wright" << true;
    QTest::newRow("roman") << "Final Fantasy XII" << "Final Fantasy 12" << true;
    QTest::newRow("roman lowercase") << "hades ii" << "Hades 2" << true;
    QTest::newRow("accents and case") << QString::fromUtf8("POKÉMON X") << "pokemon x" << true;
    QTest::newRow("lone x is a name") << "Mega Man X" << "Mega Man 10" << false;
    QTest::newRow("lone i is a name") << "I Am Setsuna" << "1 Am Setsuna" << false;
    QTest::newRow("the in the middle") << "Beyond the Sea" << "Beyond Sea" << false;
    QTest::newRow("only the") << "The" << "" << false;
}

void NameMatchTest::compactKey()
{
    QFETCH(QString, a);
    QFETCH(QString, b);
    QFETCH(bool, same);
    QCOMPARE(NameMatch::compactKey(a) == NameMatch::compactKey(b), same);
}

void NameMatchTest::clearMatch_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QStringList>("candidates");
    QTest::addColumn<int>("expected");

    QTest::newRow("exact") << "Hades" << QStringList{"Hades"} << 0;
    QTest::newRow("the user's triforce")
        << "The Legend of Zelda: TriForce Heroes"
        << QStringList{"The Legend of Zelda: Tri Force Heroes", "The Legend of Zelda: A Link Between Worlds"} << 0;
    QTest::newRow("the user's layton")
        << "Professor Layton vs Phoenix Wright: Ace attorney"
        << QStringList{"Professor Layton vs. Phoenix Wright: Ace Attorney", "Professor Layton and the Curious Village"}
        << 0;
    QTest::newRow("sequel as roman") << "Final Fantasy 12" << QStringList{"Final Fantasy XI", "Final Fantasy XII"} << 1;
    QTest::newRow("one letter off") << "Mario Tennis Ace" << QStringList{"Mario Tennis Aces", "Mario Golf"} << 0;
    QTest::newRow("two equally close") << "Mario Tennis Ace" << QStringList{"Mario Tennis Aces", "Mario Tennis Acee"}
                                       << -1;
    QTest::newRow("shorter title") << "Mario Tennis" << QStringList{"Mario Tennis Aces"} << -1;
    QTest::newRow("empty") << "" << QStringList{"Hades"} << -1;
    QTest::newRow("no candidates") << "Hades" << QStringList{} << -1;

    QTest::newRow("trademark signs") << QString::fromUtf8("ENSLAVED™: Odyssey to the West™")
                                     << QStringList{"Enslaved: Odyssey to the West"} << 0;
    QTest::newRow("edition is not the base game")
        << "Enslaved: Odyssey to the West Premium Edition" << QStringList{"Enslaved: Odyssey to the West"} << -1;
    QTest::newRow("friend's pass is the game")
        << "The Dark Pictures Anthology: Little Hope - Friend's Pass"
        << QStringList{"The Dark Pictures Anthology: Man of Medan", "The Dark Pictures Anthology: Little Hope"} << 1;
    QTest::newRow("steam episode names")
        << "Back to the Future: Ep 2 - Get Tannen!"
        << QStringList{"Back to the Future: The Game - Episode 1: It's About Time",
                       "Back to the Future: The Game - Episode 2: Get Tannen!", "Back to the Future: The Game"}
        << 1;
    QTest::newRow("episode, two names fit")
        << "Game: Ep 2 - Title"
        << QStringList{"Game: The Game - Episode 2: Title", "Game: The Demo - Episode 2: Title"} << -1;
    QTest::newRow("extra words need a number")
        << "Mega Fun Game" << QStringList{"Mega Super Fun Game"} << -1;

    // A sequel number is never nearly another.
    QTest::newRow("pokemon x and y") << "Pokemon X" << QStringList{"Pokemon Y"} << -1;
    QTest::newRow("mario bros 2 and 3") << "Super Mario Bros. 2" << QStringList{"Super Mario Bros. 3"} << -1;
    QTest::newRow("mario bros 3 exact") << "Super Mario Bros. 3" << QStringList{"Super Mario Bros. 2", "Super Mario Bros. 3"}
                                        << 1;
    QTest::newRow("persona 4 and golden") << "Persona 4" << QStringList{"Persona 4 Golden"} << -1;
    QTest::newRow("mega man x and 10") << "Mega Man X" << QStringList{"Mega Man 10"} << -1;
    QTest::newRow("zelda ii and zelda") << "Zelda II" << QStringList{"Zelda"} << -1;
    QTest::newRow("zelda and zelda ii") << "Zelda" << QStringList{"Zelda II"} << -1;
    QTest::newRow("hades 3") << "Hades 3" << QStringList{"Hades", "Hades II"} << -1;
}

void NameMatchTest::clearMatch()
{
    QFETCH(QString, title);
    QFETCH(QStringList, candidates);
    QFETCH(int, expected);

    QList<QStringList> names;
    for (const QString &c : candidates)
        names.append({c});
    QCOMPARE(NameMatch::clearMatch(names, title), expected);
}

void NameMatchTest::withoutTrailingParenthetical()
{
    QCOMPARE(NameMatch::withoutTrailingParenthetical("Yakuza Kiwami 2 (Legacy)"), QStringLiteral("Yakuza Kiwami 2"));
    QCOMPARE(NameMatch::withoutTrailingParenthetical("Yakuza Kiwami 2"), QStringLiteral("Yakuza Kiwami 2"));
    QCOMPARE(NameMatch::withoutTrailingParenthetical("(Legacy)"), QStringLiteral("(Legacy)"));
    QCOMPARE(NameMatch::withoutTrailingParenthetical("Game (A) Two"), QStringLiteral("Game (A) Two"));
}

void NameMatchTest::splitCamelCase()
{
    QCOMPARE(NameMatch::splitCamelCase("The Legend of Zelda: TriForce Heroes"),
             QStringLiteral("The Legend of Zelda: Tri Force Heroes"));
    QCOMPARE(NameMatch::splitCamelCase("Hades"), QStringLiteral("Hades"));
    QCOMPARE(NameMatch::splitCamelCase("FIFA 23"), QStringLiteral("FIFA 23"));
    QCOMPARE(NameMatch::splitCamelCase(""), QString());
}

void NameMatchTest::tieBreak()
{
    const QList<QStringList> names = {{"Doom"}, {"DOOM"}, {"Doom II"}};
    QCOMPARE(NameMatch::clearMatch(names, "Doom"), -1);
    QCOMPARE(NameMatch::clearMatch(names, "Doom", [](const QList<int> &same) { return same.last(); }), 1);

    // An alternative name counts, and a game with two matching names is still one game.
    const QList<QStringList> alternatives = {{"Pokemon Ultra Sun", "Pokemon Ultrasol"}, {"Pokemon Sun"}};
    QCOMPARE(NameMatch::clearMatch(alternatives, "Pokémon Ultrasol"), 0);
}

QTEST_MAIN(NameMatchTest)
#include "namematch_test.moc"
