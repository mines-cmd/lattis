#pragma once

#include <QString>
#include <QStringList>

// See docs/code/library.md.
namespace SearchText {
QString fold(const QString &text);

QStringList words(const QString &search);

bool matches(const QString &foldedTitle, const QStringList &words);
}
