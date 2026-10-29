#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

// See docs/code/core.md.
namespace QrCode {

struct Symbol
{
    int size = 0;
    // Row-major, size * size; true is a dark module.
    QList<bool> modules;

    bool isEmpty() const { return size == 0; }
    bool dark(int x, int y) const { return x >= 0 && y >= 0 && x < size && y < size && modules.at(y * size + x); }
};

// Byte mode, error correction level M, the smallest version that fits (1 to 10); empty when `data` is longer
// than that holds (213 bytes).
Symbol encode(const QByteArray &data);

// The dark modules as SVG path data of unit squares ("M1 0h1v1h-1z…"), for a Shape scaled to the symbol's size.
QString toSvgPath(const Symbol &symbol);

// Whether every block's Reed-Solomon codewords divide by the generator, for the tests.
bool selfCheck(const QByteArray &data);
}
