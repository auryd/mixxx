#include "library/StemsMixTableModel.h"

#include "library/dao/stemsmixdao.h"
#include "library/dao/trackschema.h"
#include "library/queryutil.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "moc_StemsMixTableModel.cpp"

namespace {

const QString kModelName = "stemsmix:";

} // anonymous namespace

StemsMixTableModel::StemsMixTableModel(QObject* parent,
        TrackCollectionManager* pTrackCollectionManager,
        const char* settingsNamespace,
        bool keepDeletedTracks)
        : TrackSetTableModel(parent, pTrackCollectionManager, settingsNamespace),
          m_iStemsMixId(-1),
          m_keepDeletedTracks(keepDeletedTracks) {
    connect(&m_pTrackCollectionManager->internalCollection()->getStemsMixDAO(),
            &StemsMixDAO::tracksChanged,
            this,
            &StemsMixTableModel::stemsmixsChanged);
}

void StemsMixTableModel::initSortColumnMapping() {
    // Add a bijective mapping between the SortColumnIds and column indices
    for (int i = 0; i < static_cast<int>(TrackModel::SortColumnId::IdMax); ++i) {
        m_columnIndexBySortColumnId[i] = -1;
    }
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Artist)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_ARTIST);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Title)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_TITLE);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Album)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_ALBUM);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::AlbumArtist)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_ALBUMARTIST);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Year)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_YEAR);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Genre)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_GENRE);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Composer)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COMPOSER);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Grouping)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_GROUPING);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::TrackNumber)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_TRACKNUMBER);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::FileType)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_FILETYPE);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::NativeLocation)] =
            fieldIndex(ColumnCache::COLUMN_TRACKLOCATIONSTABLE_LOCATION);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Comment)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COMMENT);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Duration)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_DURATION);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::BitRate)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_BITRATE);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Bpm)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_BPM);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::ReplayGain)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_REPLAYGAIN);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::DateTimeAdded)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_DATETIMEADDED);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::TimesPlayed)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_TIMESPLAYED);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::LastPlayedAt)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_LAST_PLAYED_AT);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Rating)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_RATING);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Key)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_KEY);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Preview)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_PREVIEW);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Color)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COLOR);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::CoverArt)] =
            fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COVERART);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::Position)] =
            fieldIndex(ColumnCache::COLUMN_STEMSMIXTRACKSTABLE_POSITION);

    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::AbsoluteBarIndex)] =
            fieldIndex(ColumnCache::COLUMN_STEMSMIXTRACKSTABLE_ABSOLUTEBARINDEX);
    m_columnIndexBySortColumnId[static_cast<int>(
            TrackModel::SortColumnId::TrackBarIndex)] =
            fieldIndex(ColumnCache::COLUMN_STEMSMIXTRACKSTABLE_TRACKBARINDEX);

    m_sortColumnIdByColumnIndex.clear();
    for (int i = static_cast<int>(TrackModel::SortColumnId::IdMin);
            i < static_cast<int>(TrackModel::SortColumnId::IdMax);
            ++i) {
        TrackModel::SortColumnId sortColumn = static_cast<TrackModel::SortColumnId>(i);
        m_sortColumnIdByColumnIndex.insert(
                m_columnIndexBySortColumnId[static_cast<int>(sortColumn)],
                sortColumn);
    }
}

void StemsMixTableModel::setTableModel(int stemsmixId) {
    //qDebug() << "StemsMixTableModel::setTableModel" << stemsmixId;
    if (m_iStemsMixId == stemsmixId) {
        qDebug() << "Already focused on stemsmix " << stemsmixId;
        return;
    }

    m_iStemsMixId = stemsmixId;

    if (!m_keepDeletedTracks) {
        // From Mixxx 2.1 we drop tracks that have been explicitly deleted
        // in the library (mixxx_deleted = 0) from stemsmixs.
        // These invisible tracks, consuming a stemsmix position number were
        // a source user of confusion in the past.
        m_pTrackCollectionManager->internalCollection()->getStemsMixDAO().removeHiddenTracks(m_iStemsMixId);
    }

    QString stemsmixTableName = "stemsmix_" + QString::number(m_iStemsMixId);
    QSqlQuery query(m_database);
    FieldEscaper escaper(m_database);

    QStringList columns;
    columns << STEMSMIXTRACKSTABLE_TRACKID + " AS " + LIBRARYTABLE_ID
            << STEMSMIXTRACKSTABLE_POSITION
            << STEMSMIXTRACKSTABLE_DATETIMEADDED
            << "'' AS " + LIBRARYTABLE_PREVIEW
            // For sorting the cover art column we give LIBRARYTABLE_COVERART
            // the same value as the cover digest.
            << LIBRARYTABLE_COVERART_DIGEST + " AS " + LIBRARYTABLE_COVERART;

    QString queryString = QString(
            "CREATE TEMPORARY VIEW IF NOT EXISTS %1 AS "
            "SELECT %2 FROM StemsMixTracks "
            "INNER JOIN library ON library.id = StemsMixTracks.track_id "
            "WHERE StemsMixTracks.stemsmix_id = %3")
                                  .arg(escaper.escapeString(stemsmixTableName),
                                          columns.join(","),
                                          QString::number(stemsmixId));
    query.prepare(queryString);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
    }

    columns[0] = LIBRARYTABLE_ID;
    // columns[1] = STEMSMIXTRACKSTABLE_POSITION from above
    // columns[2] = STEMSMIXTRACKSTABLE_DATETIMEADDED from above
    columns[3] = LIBRARYTABLE_PREVIEW;
    columns[4] = LIBRARYTABLE_COVERART;
    setTable(stemsmixTableName,
            LIBRARYTABLE_ID,
            columns,
            m_pTrackCollectionManager->internalCollection()->getTrackSource());
    setSearch("");
    setDefaultSort(fieldIndex(ColumnCache::COLUMN_STEMSMIXTRACKSTABLE_POSITION), Qt::AscendingOrder);
    setSort(defaultSortColumn(), defaultSortOrder());
}

int StemsMixTableModel::addTracks(const QModelIndex& index,
        const QList<QString>& locations) {
    if (locations.isEmpty()) {
        return 0;
    }

    QList<TrackId> trackIds = m_pTrackCollectionManager->resolveTrackIdsFromLocations(
            locations);

    const int positionColumn = fieldIndex(ColumnCache::COLUMN_STEMSMIXTRACKSTABLE_POSITION);
    int position = index.sibling(index.row(), positionColumn).data().toInt();

    // Handle weird cases like a drag and drop to an invalid index
    if (position <= 0) {
        position = rowCount() + 1;
    }

    int tracksAdded = m_pTrackCollectionManager->internalCollection()->getStemsMixDAO().insertTracksIntoStemsMix(
            trackIds, m_iStemsMixId, position);

    if (locations.size() - tracksAdded > 0) {
        qDebug() << "StemsMixTableModel::addTracks could not add"
                 << locations.size() - tracksAdded
                 << "to stemsmix" << m_iStemsMixId;
    }
    return tracksAdded;
}

bool StemsMixTableModel::appendTrack(TrackId trackId) {
    if (!trackId.isValid()) {
        return false;
    }
    return m_pTrackCollectionManager->internalCollection()->getStemsMixDAO().appendTrackToStemsMix(trackId, m_iStemsMixId);
}

void StemsMixTableModel::removeTrack(const QModelIndex& index) {
    if (m_pTrackCollectionManager->internalCollection()->getStemsMixDAO().isStemsMixLocked(m_iStemsMixId)) {
        return;
    }

    const int positionColumnIndex = fieldIndex(ColumnCache::COLUMN_STEMSMIXTRACKSTABLE_POSITION);
    int position = index.sibling(index.row(), positionColumnIndex).data().toInt();
    m_pTrackCollectionManager->internalCollection()->getStemsMixDAO().removeTrackFromStemsMix(m_iStemsMixId, position);
}

void StemsMixTableModel::removeTracks(const QModelIndexList& indices) {
    if (m_pTrackCollectionManager->internalCollection()->getStemsMixDAO().isStemsMixLocked(m_iStemsMixId)) {
        return;
    }

    const int positionColumnIndex = fieldIndex(ColumnCache::COLUMN_STEMSMIXTRACKSTABLE_POSITION);

    QList<int> trackPositions;
    foreach (QModelIndex index, indices) {
        int trackPosition = index.sibling(index.row(), positionColumnIndex).data().toInt();
        trackPositions.append(trackPosition);
    }

    m_pTrackCollectionManager->internalCollection()->getStemsMixDAO().removeTracksFromStemsMix(
            m_iStemsMixId,
            std::move(trackPositions));
}

void StemsMixTableModel::moveTrack(const QModelIndex& sourceIndex,
        const QModelIndex& destIndex) {
    int stemsmixPositionColumn = fieldIndex(ColumnCache::COLUMN_STEMSMIXTRACKSTABLE_POSITION);

    int newPosition = destIndex.sibling(destIndex.row(), stemsmixPositionColumn).data().toInt();
    int oldPosition = sourceIndex.sibling(sourceIndex.row(), stemsmixPositionColumn).data().toInt();

    if (newPosition > oldPosition) {
        // new position moves up due to closing the gap of the old position
        --newPosition;
    }

    //qDebug() << "old pos" << oldPosition << "new pos" << newPosition;
    if (newPosition < 0 || newPosition == oldPosition) {
        // Invalid for the position to be 0 or less.
        // or no move at all
        return;
    } else if (newPosition == 0) {
        // Dragged out of bounds, which is past the end of the rows...
        newPosition = m_pTrackCollectionManager->internalCollection()->getStemsMixDAO().getMaxPosition(m_iStemsMixId);
    }

    m_pTrackCollectionManager->internalCollection()->getStemsMixDAO().moveTrack(m_iStemsMixId, oldPosition, newPosition);
}

bool StemsMixTableModel::isLocked() {
    return m_pTrackCollectionManager->internalCollection()->getStemsMixDAO().isStemsMixLocked(m_iStemsMixId);
}

bool StemsMixTableModel::isColumnInternal(int column) {
    return column == fieldIndex(ColumnCache::COLUMN_STEMSMIXTRACKSTABLE_TRACKID) ||
            TrackSetTableModel::isColumnInternal(column);
}

bool StemsMixTableModel::isColumnHiddenByDefault(int column) {
    if (column == fieldIndex(ColumnCache::COLUMN_STEMSMIXTRACKSTABLE_DATETIMEADDED)) {
        return true;
    } else if (column == fieldIndex(ColumnCache::COLUMN_STEMSMIXTRACKSTABLE_POSITION)) {
        return false;
    }
    return BaseSqlTableModel::isColumnHiddenByDefault(column);
}

TrackModel::Capabilities StemsMixTableModel::getCapabilities() const {
    TrackModel::Capabilities caps =
            Capability::ReceiveDrops |
            Capability::Reorder |
            Capability::AddToTrackSet |
            Capability::EditMetadata |
            Capability::LoadToDeck |
            Capability::LoadToSampler |
            Capability::LoadToPreviewDeck |
            Capability::ResetPlayed |
            Capability::Analyze;

    if (m_iStemsMixId !=
            m_pTrackCollectionManager->internalCollection()
                    ->getStemsMixDAO()
                    .getStemsMixIdFromName(AUTODJ_TABLE)) {
        // Only allow Add to AutoDJ if we aren't currently showing the AutoDJ queue.
        caps |= Capability::AddToAutoDJ | Capability::RemoveStemsMix;
    } else {
        caps |= Capability::Remove;
    }
    if (StemsMixDAO::PLHT_SET_LOG ==
            m_pTrackCollectionManager->internalCollection()
                    ->getStemsMixDAO()
                    .getHiddenType(m_iStemsMixId)) {
        // Disable track reordering for history stemsmixs
        caps &= ~(Capability::Reorder | Capability::RemoveStemsMix);
    }
    bool locked = m_pTrackCollectionManager->internalCollection()->getStemsMixDAO().isStemsMixLocked(m_iStemsMixId);
    if (locked) {
        caps |= Capability::Locked;
    }

    return caps;
}

QString StemsMixTableModel::modelKey(bool noSearch) const {
    if (noSearch) {
        return kModelName + m_tableName;
    }
    return kModelName + m_tableName +
            QStringLiteral("#") +
            currentSearch();
}

void StemsMixTableModel::stemsmixsChanged(const QSet<int>& stemsmixIds) {
    if (stemsmixIds.contains(m_iStemsMixId)) {
        select(); // Repopulate the data model.
    }
}
