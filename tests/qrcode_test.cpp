#include "qrcode.h"

#include <QTest>

class QrCodeTest : public QObject
{
    Q_OBJECT

private slots:
    void sizeFollowsTheLength();
    void tooLongIsEmpty();
    void everyLengthSplitsIntoValidBlocks();
    void finderPatternsAndTiming();
    void formatBitsAgreeAndAreAValidCode();
    void pathHasOneSquarePerDarkModule();

private:
    static QByteArray data(int length) { return QByteArray(length, 'x'); }
    static int formatBits(const QrCode::Symbol &s, bool second)
    {
        int bits = 0;
        if (!second) {
            for (int i = 0; i <= 5; ++i)
                bits |= int(s.dark(8, i)) << i;
            bits |= int(s.dark(8, 7)) << 6;
            bits |= int(s.dark(8, 8)) << 7;
            bits |= int(s.dark(7, 8)) << 8;
            for (int i = 9; i < 15; ++i)
                bits |= int(s.dark(14 - i, 8)) << i;
        } else {
            for (int i = 0; i < 8; ++i)
                bits |= int(s.dark(s.size - 1 - i, 8)) << i;
            for (int i = 8; i < 15; ++i)
                bits |= int(s.dark(8, s.size - 15 + i)) << i;
        }
        return bits;
    }
};

void QrCodeTest::sizeFollowsTheLength()
{
    // Level M holds 14 bytes in version 1, 26 in 2, 42 in 3, 62 in 4.
    QCOMPARE(QrCode::encode(data(1)).size, 21);
    QCOMPARE(QrCode::encode(data(14)).size, 21);
    QCOMPARE(QrCode::encode(data(15)).size, 25);
    QCOMPARE(QrCode::encode(data(26)).size, 25);
    QCOMPARE(QrCode::encode(data(40)).size, 29);
    QCOMPARE(QrCode::encode(data(62)).size, 33);
    QCOMPARE(QrCode::encode(data(213)).size, 57);
}

void QrCodeTest::tooLongIsEmpty()
{
    QVERIFY(QrCode::encode(data(214)).isEmpty());
    QVERIFY(!QrCode::selfCheck(data(214)));
}

void QrCodeTest::everyLengthSplitsIntoValidBlocks()
{
    for (int length = 0; length <= 213; ++length)
        QVERIFY2(QrCode::selfCheck(data(length)), qPrintable(QString::number(length)));
}

void QrCodeTest::finderPatternsAndTiming()
{
    const QrCode::Symbol s = QrCode::encode(QByteArrayLiteral("https://s.team/q/1/1234567890123456789"));
    QVERIFY(!s.isEmpty());
    for (const auto &[ox, oy] : {std::pair{0, 0}, std::pair{s.size - 7, 0}, std::pair{0, s.size - 7}}) {
        for (int y = 0; y < 7; ++y) {
            for (int x = 0; x < 7; ++x) {
                const int ring = qMax(qAbs(x - 3), qAbs(y - 3));
                QCOMPARE(s.dark(ox + x, oy + y), ring != 2);
            }
        }
    }
    for (int i = 8; i < s.size - 8; ++i) {
        QCOMPARE(s.dark(6, i), i % 2 == 0);
        QCOMPARE(s.dark(i, 6), i % 2 == 0);
    }
    QVERIFY(s.dark(8, s.size - 8));
}

void QrCodeTest::formatBitsAgreeAndAreAValidCode()
{
    const QrCode::Symbol s = QrCode::encode(QByteArrayLiteral("https://s.team/q/1/1234567890123456789"));
    const int first = formatBits(s, false);
    QCOMPARE(formatBits(s, true), first);
    const int unmasked = first ^ 0x5412;
    // Level M is 0 in the first two data bits, and the 15 bits divide by the BCH generator.
    QCOMPARE((unmasked >> 13) & 3, 0);
    int rem = unmasked;
    for (int i = 14; i >= 10; --i) {
        if ((rem >> i) & 1)
            rem ^= 0x537 << (i - 10);
    }
    QCOMPARE(rem, 0);
}

void QrCodeTest::pathHasOneSquarePerDarkModule()
{
    const QrCode::Symbol s = QrCode::encode(QByteArrayLiteral("abc"));
    int dark = 0;
    for (bool d : s.modules)
        dark += d ? 1 : 0;
    QCOMPARE(QrCode::toSvgPath(s).count(QLatin1Char('M')), dark);
    QVERIFY(QrCode::toSvgPath(QrCode::Symbol()).isEmpty());
}

QTEST_APPLESS_MAIN(QrCodeTest)
#include "qrcode_test.moc"
