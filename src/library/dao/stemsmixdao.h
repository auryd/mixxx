#pragma once

#include <QHash>
#include <QObject>
#include <QSqlDatabase>
#include <QSet>

#include "library/dao/dao.h"
#include "track/trackid.h"
#include "util/class.h"

#define STEMSMIX_TABLE "StemsMixs"
#define STEMSMIX_TRACKS_TABLE "StemsMixTracks"

const QString STEMSMIXTABLE_ID = QStringLiteral("id");
const QString STEMSMIXTABLE_NAME = QStringLiteral("name");
const QString STEMSMIXTABLE_POSITION = QStringLiteral("position");
const QString STEMSMIXTABLE_HIDDEN = QStringLiteral("hidden");
const QString STEMSMIXTABLE_DATECREATED = QStringLiteral("date_created");
const QString STEMSMIXTABLE_DATEMODIFIED = QStringLiteral("date_modified");

const QString STEMSMIXTRACKSTABLE_TRACKID = QStringLiteral("track_id");
const QString STEMSMIXTRACKSTABLE_POSITION = QStringLiteral("position");
const QString STEMSMIXTRACKSTABLE_STEMSMIXID = QStringLiteral("stemsmix_id");
const QString STEMSMIXTRACKSTABLE_DATETIMEADDED = QStringLiteral("pl_datetime_added");

const QString STEMSMIXTRACKSTABLE_ABSOLUTEBARINDEX = QStringLiteral("absolute_bar_index");
const QString STEMSMIXTRACKSTABLE_TRACKBARINDEX = QStringLiteral("track_bar_index");
const QString STEMSMIXTRACKSTABLE_DURATIONBARS = QStringLiteral("duration_bars");
const QString STEMSMIXTRACKSTABLE_STEMID = QStringLiteral("stem_id");

#define AUTODJ_TABLE "Auto DJ"

class AutoDJProcessor;

class StemsMixDAO : public QObject, public virtual DAO {
    Q_OBJECT
  public:
    enum HiddenType {
        PLHT_NOT_HIDDEN = 0,
        PLHT_AUTO_DJ = 1,
        PLHT_SET_LOG = 2,
        PLHT_UNKNOWN = -1
    };

    enum class AutoDJSendLoc {
        TOP,
        BOTTOM,
        REPLACE,
    };

    StemsMixDAO();
    ~StemsMixDAO() override = default;

    void initialize(const QSqlDatabase& database) override;

    // Create a stemsmix, fails with -1 if already exists
    int createStemsMix(const QString& name, const HiddenType type = PLHT_NOT_HIDDEN);
    // Create a stemsmix, appends "(n)" if already exists, name becomes the new name
    int createUniqueStemsMix(QString* pName, const HiddenType type = PLHT_NOT_HIDDEN);
    // Delete a stemsmix
    void deleteStemsMix(const int stemsmixId);
    /// Delete StemsMixs with fewer entries then "length"
    /// Needs to be called inside a transaction.
    /// @return number of deleted stemsmixs, -1 on error
    int deleteAllUnlockedStemsMixsWithFewerTracks(StemsMixDAO::HiddenType type,
            int minNumberOfTracks);
    // Rename a stemsmix
    void renameStemsMix(const int stemsmixId, const QString& newName);
    // Lock or unlock a stemsmix
    bool setStemsMixLocked(const int stemsmixId, const bool locked);
    // Find out the state of a stemsmix lock
    bool isStemsMixLocked(const int stemsmixId) const;
    // Append a list of tracks to a stemsmix
    bool appendTracksToStemsMix(const QList<TrackId>& trackIds, const int stemsmixId);
    // Append a track to a stemsmix
    bool appendTrackToStemsMix(TrackId trackId, const int stemsmixId);
    // Find out how many stemsmixs exist.
    unsigned int stemsmixCount() const;
    // Find out the name of the stemsmix at the given Id
    QString getStemsMixName(const int stemsmixId) const;
    // Get the stemsmix id by its name
    int getStemsMixIdFromName(const QString& name) const;
    // Get the id of the stemsmix at index. Note that the index is the natural
    // position in the database table, not the display order position column
    // stored in the database.
    int getStemsMixId(const int index) const;
    QList<TrackId> getTrackIds(const int stemsmixId) const;
    // Returns true if the stemsmix with stemsmixId is hidden
    bool isHidden(const int stemsmixId) const;
    // Returns the HiddenType of stemsmixId
    HiddenType getHiddenType(const int stemsmixId) const;
    // Returns the maximum position of the given stemsmix
    int getMaxPosition(const int stemsmixId) const;
    // Remove a track from all stemsmixs
    void removeTracksFromStemsMixs(const QList<TrackId>& trackIds);
    // removes all hidden and purged Tracks from the stemsmix
    void removeHiddenTracks(const int stemsmixId);
    // Remove a track from a stemsmix
    void removeTrackFromStemsMix(int stemsmixId, int position);
    void removeTracksFromStemsMix(int stemsmixId, const QList<int>& positions);
    void removeTracksFromStemsMixById(int stemsmixId, TrackId trackId);
    // Insert a track into a specific position in a stemsmix
    bool insertTrackIntoStemsMix(TrackId trackId, int stemsmixId, int position);
    // Inserts a list of tracks into stemsmix
    int insertTracksIntoStemsMix(const QList<TrackId>& trackIds, const int stemsmixId, int position);
    // Add a stemsmix to the Auto-DJ Queue
    void addStemsMixToAutoDJQueue(const int stemsmixId, AutoDJSendLoc loc);
    // Add a list of tracks to the Auto-DJ Queue
    void addTracksToAutoDJQueue(const QList<TrackId>& trackIds, AutoDJSendLoc loc);
    // Get the preceding stemsmix of currentStemsMixId with the HiddenType
    // hidden. Returns -1 if no such stemsmix exists.
    int getPreviousStemsMix(const int currentStemsMixId, HiddenType hidden) const;
    // Append all the tracks in the source stemsmix to the target stemsmix.
    bool copyStemsMixTracks(const int sourceStemsMixID, const int targetStemsMixID);
    // Returns the number of tracks in the given stemsmix.
    int tracksInStemsMix(const int stemsmixId) const;
    // moved Track to a new position
    void moveTrack(const int stemsmixId,
            const int oldPosition, const int newPosition);
    bool isTrackInStemsMix(TrackId trackId, const int stemsmixId) const;

    void getStemsMixsTrackIsIn(TrackId trackId, QSet<int>* stemsmixSet) const;

    void setAutoDJProcessor(AutoDJProcessor* pAutoDJProcessor);

  signals:
    void added(int stemsmixId);
    void deleted(int stemsmixId);
    void renamed(int stemsmixId, const QString& newName);
    void lockChanged(int stemsmixId);
    void trackAdded(int stemsmixId, TrackId trackId, int position);
    void trackRemoved(int stemsmixId, TrackId trackId, int position);
    void tracksChanged(const QSet<int>& stemsmixIds); // added/removed/reordered
    void tracksRemovedFromPlayedHistory(const QSet<TrackId>& playedTrackIds);

  private:
    bool removeTracksFromStemsMix(int stemsmixId, int startIndex);
    void removeTracksFromStemsMixInner(int stemsmixId, int position);
    void removeTracksFromStemsMixByIdInner(int stemsmixId, TrackId trackId);
    void searchForDuplicateTrack(const int fromPosition,
                                 const int toPosition,
                                 TrackId trackID,
                                 const int excludePosition,
                                 const int otherTrackPosition,
                                 const QHash<int,TrackId>* pTrackPositionIds,
                                 int* pTrackDistance);
    void populateStemsMixMembershipCache();

    QMultiHash<TrackId, int> m_stemsmixsTrackIsIn;
    AutoDJProcessor* m_pAutoDJProcessor;
    DISALLOW_COPY_AND_ASSIGN(StemsMixDAO);
};
