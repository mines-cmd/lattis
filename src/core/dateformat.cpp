#include "dateformat.h"

#include <QLocale>

namespace {
bool g_systemLocale = true;
}

void DateFormat::setSystemLocale(bool system)
{
    g_systemLocale = system;
}

QString DateFormat::shortFormat()
{
    return g_systemLocale ? QLocale().dateFormat(QLocale::ShortFormat) : QStringLiteral("dd/MM/yy");
}

QString DateFormat::shortDate(const QDate &date)
{
    return QLocale().toString(date, shortFormat());
}
