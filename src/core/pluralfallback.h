#pragma once

#include <QTranslator>

// Source strings write plurals as "%n game(s)". With no translation file Qt only fills in the number,
// so this translator, installed first, drops the "(s)" for one and turns it into "s" for any other count.
class PluralFallback : public QTranslator
{
public:
    using QTranslator::QTranslator;

    QString translate(const char *context, const char *sourceText, const char *disambiguation, int n) const override;
    bool isEmpty() const override { return false; }

    static QString plural(const QString &text, int n);
};
