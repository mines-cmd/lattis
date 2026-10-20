#pragma once

#include <QDate>
#include <QString>

namespace DateFormat {
// Set once at start, from the same setting that picks the default QLocale: that locale doesn't
// change while running, so neither does this.
void setSystemLocale(bool system);

QString shortFormat();
QString shortDate(const QDate &date);
}
