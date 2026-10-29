#include "qrcode.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace {
constexpr int kMaxVersion = 10;
// Error correction level M, versions 1 to 10.
constexpr int kEccPerBlock[kMaxVersion + 1] = {0, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26};
constexpr int kBlocks[kMaxVersion + 1] = {0, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5};
constexpr int kFormatBitsM = 0;

int rawModules(int version)
{
    int result = (16 * version + 128) * version + 64;
    if (version >= 2) {
        const int aligns = version / 7 + 2;
        result -= (25 * aligns - 10) * aligns - 55;
        if (version >= 7)
            result -= 36;
    }
    return result;
}

int dataCodewords(int version)
{
    return rawModules(version) / 8 - kEccPerBlock[version] * kBlocks[version];
}

int gfMultiply(int x, int y)
{
    int z = 0;
    for (int i = 7; i >= 0; --i) {
        z = (z << 1) ^ ((z >> 7) * 0x11D);
        z ^= ((y >> i) & 1) * x;
    }
    return z;
}

QList<quint8> divisor(int degree)
{
    QList<quint8> result(degree, 0);
    result[degree - 1] = 1;
    int root = 1;
    for (int i = 0; i < degree; ++i) {
        for (int j = 0; j < degree; ++j) {
            result[j] = quint8(gfMultiply(result.at(j), root));
            if (j + 1 < degree)
                result[j] ^= result.at(j + 1);
        }
        root = gfMultiply(root, 2);
    }
    return result;
}

QList<quint8> remainder(const QList<quint8> &data, const QList<quint8> &divisor)
{
    QList<quint8> result(divisor.size(), 0);
    for (quint8 b : data) {
        const quint8 factor = b ^ result.first();
        result.removeFirst();
        result.append(0);
        for (qsizetype i = 0; i < divisor.size(); ++i)
            result[i] ^= quint8(gfMultiply(divisor.at(i), factor));
    }
    return result;
}

void appendBits(QList<bool> &bits, unsigned value, int count)
{
    for (int i = count - 1; i >= 0; --i)
        bits.append(((value >> i) & 1) != 0);
}

// The data codewords of the version, padded.
QList<quint8> dataBytes(const QByteArray &data, int version)
{
    QList<bool> bits;
    appendBits(bits, 0x4, 4);
    appendBits(bits, unsigned(data.size()), version < 10 ? 8 : 16);
    for (char c : data)
        appendBits(bits, quint8(c), 8);
    const int capacity = dataCodewords(version) * 8;
    appendBits(bits, 0, int(std::min<qsizetype>(4, capacity - bits.size())));
    appendBits(bits, 0, int((8 - bits.size() % 8) % 8));
    QList<quint8> bytes;
    for (qsizetype i = 0; i < bits.size(); i += 8) {
        unsigned b = 0;
        for (int j = 0; j < 8; ++j)
            b = (b << 1) | (bits.at(i + j) ? 1u : 0u);
        bytes.append(quint8(b));
    }
    for (quint8 pad = 0xEC; bytes.size() < dataCodewords(version); pad ^= 0xEC ^ 0x11)
        bytes.append(pad);
    return bytes;
}

struct Blocks
{
    QList<QList<quint8>> data;
    QList<QList<quint8>> ecc;
};

Blocks splitBlocks(const QList<quint8> &bytes, int version)
{
    const int blocks = kBlocks[version];
    const int eccLen = kEccPerBlock[version];
    const int raw = rawModules(version) / 8;
    const int shortBlocks = blocks - raw % blocks;
    const int shortLen = raw / blocks;
    const QList<quint8> div = divisor(eccLen);
    Blocks out;
    qsizetype k = 0;
    for (int i = 0; i < blocks; ++i) {
        const int len = shortLen - eccLen + (i < shortBlocks ? 0 : 1);
        const QList<quint8> block = bytes.mid(k, len);
        k += len;
        out.data.append(block);
        out.ecc.append(remainder(block, div));
    }
    return out;
}

QList<quint8> interleave(const Blocks &blocks)
{
    QList<quint8> out;
    qsizetype longest = 0;
    for (const auto &b : blocks.data)
        longest = std::max(longest, b.size());
    for (qsizetype i = 0; i < longest; ++i) {
        for (const auto &b : blocks.data) {
            if (i < b.size())
                out.append(b.at(i));
        }
    }
    for (qsizetype i = 0; i < blocks.ecc.first().size(); ++i) {
        for (const auto &b : blocks.ecc)
            out.append(b.at(i));
    }
    return out;
}

class Builder
{
public:
    Builder(int version) : m_size(version * 4 + 17), m_version(version)
    {
        m_dark.assign(size_t(m_size * m_size), false);
        m_function.assign(size_t(m_size * m_size), false);
        drawFunctionPatterns();
    }

    void place(const QList<quint8> &codewords)
    {
        const int total = int(codewords.size()) * 8;
        int i = 0;
        for (int right = m_size - 1; right >= 1; right -= 2) {
            if (right == 6)
                right = 5;
            for (int vert = 0; vert < m_size; ++vert) {
                for (int j = 0; j < 2; ++j) {
                    const int x = right - j;
                    const bool upward = ((right + 1) & 2) == 0;
                    const int y = upward ? m_size - 1 - vert : vert;
                    if (!m_function[idx(x, y)] && i < total) {
                        m_dark[idx(x, y)] = ((codewords.at(i >> 3) >> (7 - (i & 7))) & 1) != 0;
                        ++i;
                    }
                }
            }
        }
    }

    QrCode::Symbol finish()
    {
        int best = 0;
        int bestPenalty = -1;
        for (int mask = 0; mask < 8; ++mask) {
            applyMask(mask);
            drawFormat(mask);
            const int penalty = penaltyScore();
            if (bestPenalty < 0 || penalty < bestPenalty) {
                best = mask;
                bestPenalty = penalty;
            }
            applyMask(mask);
        }
        applyMask(best);
        drawFormat(best);
        QrCode::Symbol symbol;
        symbol.size = m_size;
        for (bool d : m_dark)
            symbol.modules.append(d);
        return symbol;
    }

private:
    size_t idx(int x, int y) const { return size_t(y * m_size + x); }

    void set(int x, int y, bool dark)
    {
        m_dark[idx(x, y)] = dark;
        m_function[idx(x, y)] = true;
    }

    void drawFinder(int cx, int cy)
    {
        for (int dy = -4; dy <= 4; ++dy) {
            for (int dx = -4; dx <= 4; ++dx) {
                const int dist = std::max(std::abs(dx), std::abs(dy));
                const int x = cx + dx, y = cy + dy;
                if (x >= 0 && x < m_size && y >= 0 && y < m_size)
                    set(x, y, dist != 2 && dist != 4);
            }
        }
    }

    void drawAlignment(int cx, int cy)
    {
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx)
                set(cx + dx, cy + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
        }
    }

    QList<int> alignmentPositions() const
    {
        if (m_version == 1)
            return {};
        const int count = m_version / 7 + 2;
        const int step = (m_version * 4 + count * 2 + 1) / (count * 2 - 2) * 2;
        QList<int> result;
        for (int i = 0, pos = m_size - 7; i < count - 1; ++i, pos -= step)
            result.prepend(pos);
        result.prepend(6);
        return result;
    }

    void drawFunctionPatterns()
    {
        for (int i = 0; i < m_size; ++i) {
            set(6, i, i % 2 == 0);
            set(i, 6, i % 2 == 0);
        }
        drawFinder(3, 3);
        drawFinder(m_size - 4, 3);
        drawFinder(3, m_size - 4);
        const QList<int> positions = alignmentPositions();
        const int n = int(positions.size());
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (!((i == 0 && j == 0) || (i == 0 && j == n - 1) || (i == n - 1 && j == 0)))
                    drawAlignment(positions.at(i), positions.at(j));
            }
        }
        drawFormat(0);
        if (m_version >= 7) {
            int rem = m_version;
            for (int i = 0; i < 12; ++i)
                rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
            const long bits = (long(m_version) << 12) | rem;
            for (int i = 0; i < 18; ++i) {
                const bool bit = ((bits >> i) & 1) != 0;
                const int a = m_size - 11 + i % 3, b = i / 3;
                set(a, b, bit);
                set(b, a, bit);
            }
        }
    }

    void drawFormat(int mask)
    {
        const int data = kFormatBitsM << 3 | mask;
        int rem = data;
        for (int i = 0; i < 10; ++i)
            rem = (rem << 1) ^ ((rem >> 9) * 0x537);
        const int bits = (data << 10 | rem) ^ 0x5412;
        const auto bit = [bits](int i) { return ((bits >> i) & 1) != 0; };
        for (int i = 0; i <= 5; ++i)
            set(8, i, bit(i));
        set(8, 7, bit(6));
        set(8, 8, bit(7));
        set(7, 8, bit(8));
        for (int i = 9; i < 15; ++i)
            set(14 - i, 8, bit(i));
        for (int i = 0; i < 8; ++i)
            set(m_size - 1 - i, 8, bit(i));
        for (int i = 8; i < 15; ++i)
            set(8, m_size - 15 + i, bit(i));
        set(8, m_size - 8, true);
    }

    void applyMask(int mask)
    {
        for (int y = 0; y < m_size; ++y) {
            for (int x = 0; x < m_size; ++x) {
                bool invert = false;
                switch (mask) {
                case 0: invert = (x + y) % 2 == 0; break;
                case 1: invert = y % 2 == 0; break;
                case 2: invert = x % 3 == 0; break;
                case 3: invert = (x + y) % 3 == 0; break;
                case 4: invert = (x / 3 + y / 2) % 2 == 0; break;
                case 5: invert = x * y % 2 + x * y % 3 == 0; break;
                case 6: invert = (x * y % 2 + x * y % 3) % 2 == 0; break;
                default: invert = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
                }
                if (invert && !m_function[idx(x, y)])
                    m_dark[idx(x, y)] = !m_dark[idx(x, y)];
            }
        }
    }

    bool at(int x, int y) const { return x >= 0 && y >= 0 && x < m_size && y < m_size && m_dark[idx(x, y)]; }

    int linePenalty(bool horizontal) const
    {
        int penalty = 0;
        for (int a = 0; a < m_size; ++a) {
            const auto cell = [&](int b) { return horizontal ? at(b, a) : at(a, b); };
            int run = 1;
            for (int b = 1; b < m_size; ++b) {
                if (cell(b) == cell(b - 1)) {
                    ++run;
                    continue;
                }
                if (run >= 5)
                    penalty += 3 + run - 5;
                run = 1;
            }
            if (run >= 5)
                penalty += 3 + run - 5;
            // 1011101 with four light modules on a side, which a finder pattern looks like.
            for (int b = -4; b + 7 <= m_size + 4; ++b) {
                static const bool pattern[7] = {true, false, true, true, true, false, true};
                bool match = true;
                for (int k = 0; k < 7 && match; ++k)
                    match = cell(b + k) == pattern[k] && b + k >= 0 && b + k < m_size;
                if (!match)
                    continue;
                bool before = true, after = true;
                for (int k = 1; k <= 4; ++k) {
                    before = before && !cell(b - k);
                    after = after && !cell(b + 6 + k);
                }
                if (before || after)
                    penalty += 40;
            }
        }
        return penalty;
    }

    int penaltyScore() const
    {
        int penalty = linePenalty(true) + linePenalty(false);
        int dark = 0;
        for (int y = 0; y < m_size; ++y) {
            for (int x = 0; x < m_size; ++x) {
                dark += at(x, y) ? 1 : 0;
                if (x + 1 < m_size && y + 1 < m_size && at(x, y) == at(x + 1, y) && at(x, y) == at(x, y + 1)
                    && at(x, y) == at(x + 1, y + 1))
                    penalty += 3;
            }
        }
        const int total = m_size * m_size;
        const int k = (std::abs(dark * 20 - total * 10) + total - 1) / total - 1;
        return penalty + std::max(k, 0) * 10;
    }

    int m_size;
    int m_version;
    std::vector<bool> m_dark;
    std::vector<bool> m_function;
};

int versionFor(const QByteArray &data)
{
    for (int version = 1; version <= kMaxVersion; ++version) {
        const int need = 4 + (version < 10 ? 8 : 16) + 8 * int(data.size());
        if (need <= dataCodewords(version) * 8)
            return version;
    }
    return 0;
}
}

namespace QrCode {
Symbol encode(const QByteArray &data)
{
    const int version = versionFor(data);
    if (version == 0)
        return {};
    Builder builder(version);
    builder.place(interleave(splitBlocks(dataBytes(data, version), version)));
    return builder.finish();
}

QString toSvgPath(const Symbol &symbol)
{
    QString path;
    for (int y = 0; y < symbol.size; ++y) {
        for (int x = 0; x < symbol.size; ++x) {
            if (symbol.dark(x, y))
                path += QStringLiteral("M%1 %2h1v1h-1z").arg(x).arg(y);
        }
    }
    return path;
}

bool selfCheck(const QByteArray &data)
{
    const int version = versionFor(data);
    if (version == 0)
        return false;
    const Blocks blocks = splitBlocks(dataBytes(data, version), version);
    const QList<quint8> div = divisor(kEccPerBlock[version]);
    for (qsizetype i = 0; i < blocks.data.size(); ++i) {
        const QList<quint8> zeros = remainder(blocks.data.at(i) + blocks.ecc.at(i), div);
        if (std::any_of(zeros.cbegin(), zeros.cend(), [](quint8 b) { return b != 0; }))
            return false;
    }
    return interleave(blocks).size() == rawModules(version) / 8;
}
}
