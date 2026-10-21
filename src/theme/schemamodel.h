#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QVariantList>
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
    Q_PROPERTY(bool sharedOnly READ sharedOnly WRITE setSharedOnly NOTIFY sharedOnlyChanged)
    Q_PROPERTY(QString group READ group WRITE setGroup NOTIFY groupChanged)
    Q_PROPERTY(QString following READ following WRITE setFollowing NOTIFY followingChanged)
    Q_PROPERTY(QString followingTitle READ followingTitle NOTIFY followingChanged)
    Q_PROPERTY(QVariantList groups READ groups NOTIFY groupsChanged)
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
        SubsectionRole,
        SubsectionCountRole,
        FirstInCardRole,
        LastInCardRole,
        HalfRole,
        LinkTargetRole,
        LinkTitleRole,
        FollowersRole,
        OwnValueRole,
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
    bool sharedOnly() const { return m_sharedOnly; }
    void setSharedOnly(bool sharedOnly);
    QString show() const;
    void setShow(const QString &show);
    QString group() const { return m_group; }
    void setGroup(const QString &group);
    QString following() const { return m_following; }
    void setFollowing(const QString &target);
    QString followingTitle() const;
    QVariantList groups() const;
    int changedCount() const { return m_changedCount; }

    QVariantList sections() const;
    Q_INVOKABLE int firstRow(const QString &section) const;
    Q_INVOKABLE int firstRow(const QString &section, const QString &subsection) const;
    Q_INVOKABLE int rowOf(const QString &section, const QString &key) const;

signals:
    void filterTextChanged();
    void changedOnlyChanged();
    void sharedOnlyChanged();
    void showChanged();
    void groupChanged();
    void followingChanged();
    void groupsChanged();
    void changedCountChanged();
    void sectionsChanged();

private:
    bool listsHalf(bool color) const;
    bool passes(int setting, bool withSlot) const;
    QList<int> visibleRows() const;
    void rebuildVisibleRows();
    void applyFilters();
    void setVisible(const QList<int> &rows);
    bool sameCard(int row, int other) const;
    void ensureLinks() const;
    void refresh();
    void updateChangedCount();

    Theme *m_theme;
    Mode m_mode;
    enum class Show { All, Colors, Style } m_show = Show::All;
    QString m_filterText;
    QString m_group = QStringLiteral("everything");
    int m_groupBit = -1;
    QString m_following;
    bool m_changedOnly = false;
    bool m_sharedOnly = false;
    int m_changedCount = 0;
    QList<int> m_visible;
    QList<int> m_headingOfSetting;
    QList<int> m_headingOfRow;
    QList<int> m_cardSize;
    // Links and followers depend on the merged values, so they are computed on first use after
    // each theme change, not at start.
    mutable bool m_linksDirty = true;
    mutable QStringList m_linkTarget;
    mutable QList<int> m_followers;
    mutable QList<bool> m_ownValue;
    mutable QList<quint32> m_groupMask;
};
