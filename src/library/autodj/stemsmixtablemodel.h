#pragma once

#include "library/basesqltablemodel.h"
#include "library/trackset/tracksettablemodel.h"

class StemsMixTableModel final : public TrackSetTableModel {
    Q_OBJECT

  public:
    StemsMixTableModel(QObject* parent, TrackCollectionManager* pTrackCollectionManager, const char* settingsNamespace, bool keepDeletedTracks = false);
    ~StemsMixTableModel() final = default;

    void setTableModel(int stemsmixId = -1);
    int getStemsMix() const {
        return m_iStemsMixId;
    }

    bool appendTrack(TrackId trackId);
    void moveTrack(const QModelIndex& sourceIndex, const QModelIndex& destIndex) override;
    void removeTrack(const QModelIndex& index);

    bool isColumnInternal(int column) final;
    bool isColumnHiddenByDefault(int column) final;
    /// This function should only be used by AUTODJ
    void removeTracks(const QModelIndexList& indices) final;
    /// Returns the number of successful additions.
    int addTracks(const QModelIndex& index, const QList<QString>& locations) final;
    bool isLocked() final;

    Capabilities getCapabilities() const final;

    QString modelKey(bool noSearch) const override;

  private slots:
    void stemsmixsChanged(const QSet<int>& stemsmixIds);

  private:
    void initSortColumnMapping() override;

    int m_iStemsMixId;
    bool m_keepDeletedTracks;
};
