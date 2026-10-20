#pragma once

#include "bulklookup.h"

#include <QObject>
#include <QSet>
#include <QStringList>
#include <QtQmlIntegration>

class MetadataClient;
class QQmlEngine;
class QJSEngine;

// See docs/code/metadata.md.
class ScanLookup : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int progressDone READ progressDone NOTIFY progressChanged)
    Q_PROPERTY(int progressTotal READ progressTotal NOTIFY progressChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)

public:
    ScanLookup(BulkLookup::Source source, BulkLookup::ValueWriter values, BulkLookup::ArtWriter art,
               const QString &settingsFile, MetadataClient *client = nullptr, QObject *parent = nullptr,
               BulkLookup::TagMappings tags = {});
    ~ScanLookup() override;
    static ScanLookup *create(QQmlEngine *, QJSEngine *);

    void add(const QStringList &ids);

    bool running() const { return m_bulk != nullptr || !m_waiting.isEmpty(); }
    int progressDone() const;
    int progressTotal() const;
    bool enabled() const;
    Q_INVOKABLE QString setEnabled(bool enabled);

signals:
    void runningChanged();
    void progressChanged();
    void enabledChanged();
    void message(const QString &text, bool review);
    void finished(int written, int unresolved);

private:
    void next();
    void stopped();
    void progressed();
    void done(int written);

    BulkLookup::Source m_source;
    BulkLookup::ValueWriter m_writeValues;
    BulkLookup::ArtWriter m_writeArt;
    QString m_settingsFile;
    MetadataClient *m_client = nullptr;
    BulkLookup::TagMappings m_tagMappings;
    QStringList m_waiting;
    QSet<QString> m_batch;
    BulkLookup *m_bulk = nullptr;
    bool m_applying = false;
    int m_unresolved = 0;
    int m_batchesDone = 0; // games in the batches done since it last stopped running
    static inline ScanLookup *s_instance = nullptr;
};
