#pragma once

#include <QDate>
#include <QDateTime>
#include <QTimeZone>

// QDateTime(date, time) in Qt::LocalTime asks libc for the zone on every call, which costs about half a millisecond;
// reading thousands of file names in a row (the Gallery) takes seconds that way. The system zone is looked up once.
inline QDateTime localDateTime(const QDate &date, const QTime &time)
{
    static const QTimeZone zone = QTimeZone::systemTimeZone();
    return QDateTime(date, time, zone);
}
