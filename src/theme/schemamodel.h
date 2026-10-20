#pragma once

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

class Theme;

// See docs/code/theme.md.
class ThemeSettingsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(bool changedOnly READ changedOnly WRITE setChangedOnly NOTIFY changedOnlyChanged)
    Q_PROPERTY(QString show READ show WRITE setShow NOTIFY showChanged)
    Q_PROPERTY(int changedCount READ changedCount NOTIFY changedCountChanged)
    Q_PROPERTY(QVariantList sections READ sections NOTIFY sectionsChanged)

public:
    enum Role {
        SectionRole = Qt::UserRole + 1,
        SectionDocRole,
        SectionTitleRole,
        KeyRole,
        TitleRole,
        CamelKeyRole,
        TypeRole,
        DocRole,
        MinRole,
        MaxRole,
        ChoicesRole,
        OnReleaseRole,
        DefaultRole,
        ValueRole,
        RawRole,
        SourceLabelRole,
        IsCustomRole,
        RevertTextRole,
        FirstInSectionRole,
        LastInSectionRole,
        HalfRole,
    };
    Q_ENUM(Role)

    enum class Mode { Colors, Style, Both };

    ThemeSettingsModel(Theme *theme, Mode mode, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const { return m_filterText; }
    void setFilterText(const QString &text);
    bool changedOnly() const { return m_changedOnly; }
    void setChangedOnly(bool changedOnly);
    QString show() const;
    void setShow(const QString &show);
    int changedCount() const { return m_changedCount; }

    QVariantList sections() const;
    Q_INVOKABLE int firstRow(const QString &section) const;
    Q_INVOKABLE int rowOf(const QString &section, const QString &key) const;

signals:
    void filterTextChanged();
    void changedOnlyChanged();
    void showChanged();
    void changedCountChanged();
    void sectionsChanged();

private:
    bool listsHalf(bool color) const;
    QList<int> visibleRows() const;
    void rebuildVisibleRows();
    void refresh();
    void updateChangedCount();

    Theme *m_theme;
    Mode m_mode;
    enum class Show { All, Colors, Style } m_show = Show::All;
    QString m_filterText;
    bool m_changedOnly = false;
    int m_changedCount = 0;
    QList<int> m_visible;
};
