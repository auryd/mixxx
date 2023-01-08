#include "library/dao/stemsmixdao.h"

#include "moc_stemsmixdao.cpp"

#include <QRandomGenerator>
#include <QtDebug>
#include <QtSql>

#include "library/autodj/autodjprocessor.h"
#include "library/queryutil.h"
#include "library/trackcollection.h"
#include "track/track.h"
#include "util/db/fwdsqlquery.h"
#include "util/math.h"

StemsMixDAO::StemsMixDAO()
        : m_pAutoDJProcessor(nullptr) {
}

void StemsMixDAO::initialize(const QSqlDatabase& database) {
    DAO::initialize(database);
    populateStemsMixMembershipCache();
}

void StemsMixDAO::populateStemsMixMembershipCache() {
    // Minor optimization: reserve space in m_stemsmixsTrackIsIn.
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT COUNT(*) from " STEMSMIX_TRACKS_TABLE));
    if (query.exec() && query.next()) {
        m_stemsmixsTrackIsIn.reserve(query.value(0).toInt());
    } else {
        LOG_FAILED_QUERY(query);
    }

    // now fetch all Tracks from all stemsmixs and insert them into the hashmap
    query.prepare(QStringLiteral(
            "SELECT track_id, stemsmix_id from " STEMSMIX_TRACKS_TABLE));
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
    }

    const int trackIdColumn = query.record().indexOf(STEMSMIXTRACKSTABLE_TRACKID);
    const int stemsmixIdColumn = query.record().indexOf(STEMSMIXTRACKSTABLE_STEMSMIXID);
    while (query.next()) {
        m_stemsmixsTrackIsIn.insert(TrackId(query.value(trackIdColumn)),
                query.value(stemsmixIdColumn).toInt());
    }
}

int StemsMixDAO::createStemsMix(const QString& name, const HiddenType hidden) {
    //qDebug() << "StemsMixDAO::createStemsMix"
    //         << QThread::currentThread()
    //         << m_database.connectionName();
    // Start the transaction
    ScopedTransaction transaction(m_database);

    // Find out the highest position for the existing stemsmixs so we know what
    // position this stemsmix should have.
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT max(position) as posmax FROM StemsMixs"));

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return -1;
    }

    //Get the id of the last stemsmix.
    int position = 0;
    if (query.next()) {
        position = query.value(query.record().indexOf("posmax")).toInt();
        position++; // Append after the last stemsmix.
    }

    //qDebug() << "Inserting stemsmix" << name << "at position" << position;

    query.prepare(QStringLiteral(
            "INSERT INTO StemsMixs (name, position, hidden, date_created, date_modified) "
            "VALUES (:name, :position, :hidden,  CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)"));
    query.bindValue(":name", name);
    query.bindValue(":position", position);
    query.bindValue(":hidden", static_cast<int>(hidden));

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return -1;
    }

    int stemsmixId = query.lastInsertId().toInt();
    // Commit the transaction
    transaction.commit();
    emit added(stemsmixId);
    return stemsmixId;
}

int StemsMixDAO::createUniqueStemsMix(QString* pName, const HiddenType hidden) {
    int stemsmixId = getStemsMixIdFromName(*pName);
    int i = 1;

    if (stemsmixId != -1) {
        // Calculate a unique name
        *pName += "(%1)";
        while (stemsmixId != -1) {
            i++;
            stemsmixId = getStemsMixIdFromName(pName->arg(i));
        }
        *pName = pName->arg(i);
    }
    return createStemsMix(*pName, hidden);
}

QString StemsMixDAO::getStemsMixName(const int stemsmixId) const {
    //qDebug() << "StemsMixDAO::getStemsMixName" << QThread::currentThread() << m_database.connectionName();

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT name FROM StemsMixs WHERE id= :id"));
    query.bindValue(":id", stemsmixId);

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return "";
    }

    // Get the name field
    QString name = "";
    const int nameColumn = query.record().indexOf("name");
    if (query.next()) {
        name = query.value(nameColumn).toString();
    }
    return name;
}

QList<TrackId> StemsMixDAO::getTrackIds(const int stemsmixId) const {
    QList<TrackId> trackIds;

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT DISTINCT track_id FROM StemsMixTracks "
            "WHERE stemsmix_id = :id"));
    query.bindValue(":id", stemsmixId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return trackIds;
    }

    const int trackIdColumn = query.record().indexOf("track_id");
    while (query.next()) {
        trackIds.append(TrackId(query.value(trackIdColumn)));
    }
    return trackIds;
}

int StemsMixDAO::getStemsMixIdFromName(const QString& name) const {
    //qDebug() << "StemsMixDAO::getStemsMixIdFromName" << QThread::currentThread() << m_database.connectionName();

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT id FROM StemsMixs WHERE name = :name"));
    query.bindValue(":name", name);
    if (query.exec()) {
        if (query.next()) {
            return query.value(query.record().indexOf("id")).toInt();
        }
    } else {
        LOG_FAILED_QUERY(query);
    }
    return -1;
}

void StemsMixDAO::deleteStemsMix(const int stemsmixId) {
    //qDebug() << "StemsMixDAO::deleteStemsMix" << QThread::currentThread() << m_database.connectionName();
    ScopedTransaction transaction(m_database);

    QSet<TrackId> playedTrackIds;
    if (getHiddenType(stemsmixId) == PLHT_SET_LOG) {
        const QList<TrackId> trackIds = getTrackIds(stemsmixId);

        // TODO: QSet<T>::fromList(const QList<T>&) is deprecated and should be
        // replaced with QSet<T>(list.begin(), list.end()).
        // However, the proposed alternative has just been introduced in Qt
        // 5.14. Until the minimum required Qt version of Mixxx is increased,
        // we need a version check here
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        playedTrackIds = QSet<TrackId>(trackIds.constBegin(), trackIds.constEnd());
#else
        playedTrackIds = QSet<TrackId>::fromList(trackIds);
#endif
    }

    // Get the stemsmix id for this
    QSqlQuery query(m_database);

    // Delete the row in the StemsMixs table.
    query.prepare(QStringLiteral(
            "DELETE FROM StemsMixs WHERE id= :id"));
    query.bindValue(":id", stemsmixId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return;
    }

    // Delete the tracks in this stemsmix from the StemsMixTracks table.
    query.prepare(QStringLiteral(
            "DELETE FROM StemsMixTracks WHERE stemsmix_id = :id"));
    query.bindValue(":id", stemsmixId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return;
    }

    transaction.commit();

    for (QMultiHash<TrackId, int>::iterator it = m_stemsmixsTrackIsIn.begin();
            it != m_stemsmixsTrackIsIn.end();) {
        if (it.value() == stemsmixId) {
            it = m_stemsmixsTrackIsIn.erase(it);
        } else {
            ++it;
        }
    }

    emit deleted(stemsmixId);
    if (!playedTrackIds.isEmpty()) {
        emit tracksRemovedFromPlayedHistory(playedTrackIds);
    }
}

int StemsMixDAO::deleteAllUnlockedStemsMixsWithFewerTracks(
        StemsMixDAO::HiddenType type, int minNumberOfTracks) {
    VERIFY_OR_DEBUG_ASSERT(minNumberOfTracks > 0) {
        return 0; // nothing to do, probably unintended invocation
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT id FROM StemsMixs  "
            "WHERE (SELECT count(stemsmix_id) FROM StemsMixTracks WHERE "
            "StemsMixs.ID = StemsMixTracks.stemsmix_id) < :length AND "
            "StemsMixs.hidden = :hidden AND StemsMixs.locked = 0"));
    query.bindValue(":hidden", static_cast<int>(type));
    query.bindValue(":length", minNumberOfTracks);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return -1;
    }

    QStringList idStringList;
    while (query.next()) {
        idStringList.append(query.value(0).toString());
    }
    if (idStringList.isEmpty()) {
        return 0;
    }
    QString idString = idStringList.join(",");

    qInfo() << "Deleting" << idStringList.size() << "stemsmixs of type" << type
            << "that contain fewer than" << minNumberOfTracks << "tracks";

    auto deleteTracks = FwdSqlQuery(m_database,
            QString("DELETE FROM StemsMixTracks WHERE stemsmix_id IN (%1)")
                    .arg(idString));
    if (!deleteTracks.execPrepared()) {
        return -1;
    }

    auto deleteStemsMixs = FwdSqlQuery(m_database,
            QString("DELETE FROM StemsMixs WHERE id IN (%1)").arg(idString));
    if (!deleteStemsMixs.execPrepared()) {
        return -1;
    }

    return idStringList.length();
}

void StemsMixDAO::renameStemsMix(const int stemsmixId, const QString& newName) {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "UPDATE StemsMixs SET name = :name WHERE id = :id"));
    query.bindValue(":name", newName);
    query.bindValue(":id", stemsmixId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return;
    }
    emit renamed(stemsmixId, newName);
}

bool StemsMixDAO::setStemsMixLocked(const int stemsmixId, const bool locked) {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "UPDATE StemsMixs SET locked = :lock WHERE id = :id"));
    // SQLite3 doesn't support boolean value. Using integer instead.
    int lock = locked ? 1 : 0;
    query.bindValue(":lock", lock);
    query.bindValue(":id", stemsmixId);

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }
    emit lockChanged(stemsmixId);
    return true;
}

bool StemsMixDAO::isStemsMixLocked(const int stemsmixId) const {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT locked FROM StemsMixs WHERE id = :id"));
    query.bindValue(":id", stemsmixId);

    if (query.exec()) {
        if (query.next()) {
            int lockValue = query.value(0).toInt();
            return lockValue == 1;
        }
    } else {
        LOG_FAILED_QUERY(query);
    }
    return false;
}

bool StemsMixDAO::removeTracksFromStemsMix(int stemsmixId, int startIndex) {
    // Retain the first track if it is loaded in a deck
    ScopedTransaction transaction(m_database);
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "DELETE FROM StemsMixTracks "
            "WHERE stemsmix_id=:id AND position>=:pos"));
    query.bindValue(":id", stemsmixId);
    query.bindValue(":pos", startIndex);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }
    transaction.commit();
    emit tracksChanged(QSet<int>{stemsmixId});
    return true;
}

bool StemsMixDAO::appendTracksToStemsMix(const QList<TrackId>& trackIds, const int stemsmixId) {
    // qDebug() << "StemsMixDAO::appendTracksToStemsMix"
    //          << QThread::currentThread() << m_database.connectionName();

    // Start the transaction
    ScopedTransaction transaction(m_database);

    int position = getMaxPosition(stemsmixId);

    // Append after the last song. If no songs or a failed query then 0 becomes 1.
    ++position;

    //Insert the song into the StemsMixTracks table
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "INSERT INTO StemsMixTracks (stemsmix_id, track_id, position, pl_datetime_added)"
            "VALUES (:stemsmix_id, :track_id, :position, CURRENT_TIMESTAMP)"));
    query.bindValue(":stemsmix_id", stemsmixId);

    int insertPosition = position;
    for (const auto& trackId : trackIds) {
        query.bindValue(":track_id", trackId.toVariant());
        query.bindValue(":position", insertPosition++);
        if (!query.exec()) {
            LOG_FAILED_QUERY(query);
            return false;
        }
    }

    // Commit the transaction
    transaction.commit();

    insertPosition = position;
    for (const auto& trackId : trackIds) {
        m_stemsmixsTrackIsIn.insert(trackId, stemsmixId);
        // TODO(XXX) don't emit if the track didn't add successfully.
        emit trackAdded(stemsmixId, trackId, insertPosition++);
    }
    emit tracksChanged(QSet<int>{stemsmixId});
    return true;
}

bool StemsMixDAO::appendTrackToStemsMix(TrackId trackId, const int stemsmixId) {
    QList<TrackId> trackIds;
    trackIds.append(trackId);
    return appendTracksToStemsMix(trackIds, stemsmixId);
}

/** Find out how many stemsmixs exist. */
unsigned int StemsMixDAO::stemsmixCount() const {
    // qDebug() << "StemsMixDAO::stemsmixCount" << QThread::currentThread() << m_database.connectionName();
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT count(*) as count FROM StemsMixs"));
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
    }

    int numRecords = 0;
    if (query.next()) {
        numRecords = query.value(query.record().indexOf("count")).toInt();
    }
    return numRecords;
}

int StemsMixDAO::getStemsMixId(const int index) const {
    //qDebug() << "StemsMixDAO::getStemsMixId"
    //         << QThread::currentThread() << m_database.connectionName();

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT id FROM StemsMixs"));

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return -1;
    }

    int currentRow = 0;
    while (query.next()) {
        if (currentRow++ == index) {
            int id = query.value(0).toInt();
            return id;
        }
    }
    return -1;
}

StemsMixDAO::HiddenType StemsMixDAO::getHiddenType(const int stemsmixId) const {
    // qDebug() << "StemsMixDAO::getHiddenType"
    //          << QThread::currentThread() << m_database.connectionName();

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT hidden FROM StemsMixs WHERE id = :id"));
    query.bindValue(":id", stemsmixId);

    if (query.exec()) {
        if (query.next()) {
            return static_cast<HiddenType>(query.value(0).toInt());
        }
    } else {
        LOG_FAILED_QUERY(query);
    }
    qDebug() << "StemsMixDAO::getHiddenType returns PLHT_UNKNOWN for stemsmixId "
             << stemsmixId;
    return PLHT_UNKNOWN;
}

bool StemsMixDAO::isHidden(const int stemsmixId) const {
    // qDebug() << "StemsMixDAO::isHidden"
    //          << QThread::currentThread() << m_database.connectionName();

    HiddenType ht = getHiddenType(stemsmixId);
    if (ht == PLHT_NOT_HIDDEN) {
        return false;
    }
    return true;
}

void StemsMixDAO::removeHiddenTracks(const int stemsmixId) {
    ScopedTransaction transaction(m_database);
    // This query deletes all tracks marked as deleted and all
    // phantom track_ids with no match in the library table
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT p1.position FROM StemsMixTracks AS p1 "
            "WHERE p1.id NOT IN ("
            "SELECT p2.id FROM StemsMixTracks AS p2 "
            "INNER JOIN library ON library.id=p2.track_id "
            "WHERE p2.stemsmix_id=p1.stemsmix_id "
            "AND library.mixxx_deleted=0) "
            "AND p1.stemsmix_id=:id"));
    query.bindValue(":id", stemsmixId);
    query.setForwardOnly(true);

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return;
    }

    while (query.next()) {
        int position = query.value(query.record().indexOf("position")).toInt();
        removeTracksFromStemsMixInner(stemsmixId, position);
    }

    transaction.commit();
    emit tracksChanged(QSet<int>{stemsmixId});
}

void StemsMixDAO::removeTracksFromStemsMixById(int stemsmixId, TrackId trackId) {
    ScopedTransaction transaction(m_database);
    removeTracksFromStemsMixByIdInner(stemsmixId, trackId);
    transaction.commit();
    emit tracksChanged(QSet<int>{stemsmixId});
}

void StemsMixDAO::removeTracksFromStemsMixByIdInner(int stemsmixId, TrackId trackId) {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT position FROM StemsMixTracks "
            "WHERE stemsmix_id=:id AND track_id=:track_id"));
    query.bindValue(":id", stemsmixId);
    query.bindValue(":track_id", trackId.toVariant());

    query.setForwardOnly(true);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return;
    }

    while (query.next()) {
        int position = query.value(query.record().indexOf("position")).toInt();
        removeTracksFromStemsMixInner(stemsmixId, position);
    }
}

void StemsMixDAO::removeTrackFromStemsMix(int stemsmixId, int position) {
    // qDebug() << "StemsMixDAO::removeTrackFromStemsMix"
    //          << QThread::currentThread() << m_database.connectionName();
    ScopedTransaction transaction(m_database);
    removeTracksFromStemsMixInner(stemsmixId, position);
    transaction.commit();
    emit tracksChanged(QSet<int>{stemsmixId});
}

void StemsMixDAO::removeTracksFromStemsMix(int stemsmixId, const QList<int>& positions) {
    // get positions in reversed order
    auto sortedPositons = positions;
    std::sort(sortedPositons.begin(), sortedPositons.end(), std::greater<int>());

    //qDebug() << "StemsMixDAO::removeTrackFromStemsMix"
    //         << QThread::currentThread() << m_database.connectionName();
    ScopedTransaction transaction(m_database);
    for (const auto position : qAsConst(sortedPositons)) {
        removeTracksFromStemsMixInner(stemsmixId, position);
    }
    transaction.commit();
    emit tracksChanged(QSet<int>{stemsmixId});
}

void StemsMixDAO::removeTracksFromStemsMixInner(int stemsmixId, int position) {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT track_id FROM StemsMixTracks "
            "WHERE stemsmix_id=:id AND position=:position"));
    query.bindValue(":id", stemsmixId);
    query.bindValue(":position", position);

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return;
    }

    if (!query.next()) {
        qDebug() << "removeTrackFromStemsMix no track exists at position:"
                 << position << "in stemsmix:" << stemsmixId;
        return;
    }
    TrackId trackId(query.value(query.record().indexOf("track_id")));

    // Delete the track from the stemsmix.
    query.prepare(QStringLiteral(
            "DELETE FROM StemsMixTracks "
            "WHERE stemsmix_id=:id AND position=:position"));
    query.bindValue(":id", stemsmixId);
    query.bindValue(":position", position);

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return;
    }

    query.prepare(QStringLiteral(
            "UPDATE StemsMixTracks SET position=position-1 "
            "WHERE position>=:position AND stemsmix_id=:id"));
    query.bindValue(":id", stemsmixId);
    query.bindValue(":position", position);

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
    }

    m_stemsmixsTrackIsIn.remove(trackId, stemsmixId);

    emit trackRemoved(stemsmixId, trackId, position);
    if (getHiddenType(stemsmixId) == PLHT_SET_LOG) {
        emit tracksRemovedFromPlayedHistory({trackId});
    }
}

bool StemsMixDAO::insertTrackIntoStemsMix(TrackId trackId, const int stemsmixId, int position) {
    if (stemsmixId < 0 || !trackId.isValid() || position < 0) {
        return false;
    }

    ScopedTransaction transaction(m_database);

    int max_position = getMaxPosition(stemsmixId) + 1;

    if (position > max_position) {
        position = max_position;
    }

    // Move all the tracks in the stemsmix up by one
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "UPDATE StemsMixTracks SET position=position+1 "
            "WHERE position>=:position AND stemsmix_id=:id"));
    query.bindValue(":id", stemsmixId);
    query.bindValue(":position", position);

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }

    //Insert the song into the StemsMixTracks table
    query.prepare(QStringLiteral(
            "INSERT INTO StemsMixTracks (stemsmix_id, track_id, position, pl_datetime_added)"
            "VALUES (:stemsmix_id, :track_id, :position, CURRENT_TIMESTAMP)"));
    query.bindValue(":stemsmix_id", stemsmixId);
    query.bindValue(":track_id", trackId.toVariant());
    query.bindValue(":position", position);

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }
    transaction.commit();

    m_stemsmixsTrackIsIn.insert(trackId, stemsmixId);
    emit trackAdded(stemsmixId, trackId, position);
    emit tracksChanged(QSet<int>{stemsmixId});
    return true;
}

int StemsMixDAO::insertTracksIntoStemsMix(const QList<TrackId>& trackIds,
        const int stemsmixId,
        int position) {
    if (stemsmixId < 0 || position < 0) {
        return 0;
    }

    int tracksAdded = 0;
    ScopedTransaction transaction(m_database);

    int max_position = getMaxPosition(stemsmixId) + 1;

    if (position > max_position) {
        position = max_position;
    }

    QSqlQuery insertQuery(m_database);
    insertQuery.prepare(QStringLiteral(
            "INSERT INTO StemsMixTracks (stemsmix_id, track_id, position)"
            "VALUES (:stemsmix_id, :track_id, :position)"));
    QSqlQuery query(m_database);
    int insertPositon = position;
    for (const auto& trackId : trackIds) {
        if (!trackId.isValid()) {
            continue;
        }
        // Move all tracks in stemsmix up by 1.
        // TODO(XXX) We could do this in one query before the for loop.
        query.prepare(QStringLiteral(
                "UPDATE StemsMixTracks SET position=position+1 "
                "WHERE position>=:position AND "
                "stemsmix_id=:id"));
        query.bindValue(":id", stemsmixId);
        query.bindValue(":position", insertPositon);

        if (!query.exec()) {
            LOG_FAILED_QUERY(query);
            continue;
        }

        // Insert the track at the given position
        insertQuery.bindValue(":stemsmix_id", stemsmixId);
        insertQuery.bindValue(":track_id", trackId.toVariant());
        insertQuery.bindValue(":position", insertPositon);
        if (!insertQuery.exec()) {
            LOG_FAILED_QUERY(insertQuery);
            continue;
        }

        // Increment the insert position for the track.
        ++insertPositon;
        ++tracksAdded;
    }

    transaction.commit();

    insertPositon = position;
    for (const auto& trackId : trackIds) {
        m_stemsmixsTrackIsIn.insert(trackId, stemsmixId);
        // TODO(XXX) The position is wrong if any track failed to insert.
        emit trackAdded(stemsmixId, trackId, insertPositon++);
    }
    emit tracksChanged(QSet<int>{stemsmixId});
    return tracksAdded;
}

void StemsMixDAO::addStemsMixToAutoDJQueue(const int stemsmixId, AutoDJSendLoc loc) {
    //qDebug() << "Adding tracks from stemsmix " << stemsmixId << " to the Auto-DJ Queue";

    // Query the StemsMixTracks database to locate tracks in the selected
    // stemsmix. Tracks are automatically sorted by position.
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT track_id FROM StemsMixTracks "
            "WHERE stemsmix_id = :plid ORDER BY position ASC"));
    query.bindValue(":plid", stemsmixId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return;
    }

    // Loop through the tracks, adding them to the Auto-DJ Queue. Start at
    // position 2 because position 1 was already loaded to the deck.
    QList<TrackId> trackIds;
    while (query.next()) {
        trackIds.append(TrackId(query.value(0)));
    }
    addTracksToAutoDJQueue(trackIds, loc);
}

int StemsMixDAO::getPreviousStemsMix(const int currentStemsMixId, HiddenType hidden) const {
    // Find out the highest position existing in the stemsmix so we know what
    // position this track should have.
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT max(id) as id FROM StemsMixs "
            "WHERE id < :id AND hidden = :hidden"));
    query.bindValue(":id", currentStemsMixId);
    query.bindValue(":hidden", hidden);

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return -1;
    }

    // Get the id of the highest stemsmix
    int previousStemsMixId = -1;
    if (query.next()) {
        previousStemsMixId = query.value(query.record().indexOf("id")).toInt();
    }
    return previousStemsMixId;
}

bool StemsMixDAO::copyStemsMixTracks(const int sourceStemsMixID, const int targetStemsMixID) {
    // Start the transaction
    ScopedTransaction transaction(m_database);

    // Copy the new tracks after the last track in the target stemsmix.
    int positionOffset = getMaxPosition(targetStemsMixID);

    // Copy the tracks from one stemsmix to another, adjusting the position of
    // each copied track, and preserving the date/time added.
    // INSERT INTO StemsMixTracks (stemsmix_id, track_id, position, pl_datetime_added) SELECT :target_plid, track_id, position + :position_offset, pl_datetime_added FROM StemsMixTracks WHERE stemsmix_id = :source_plid;
    QSqlQuery query(m_database);
    query.prepare(
            QStringLiteral(
                    "INSERT INTO " STEMSMIX_TRACKS_TABLE
                    " (%1, %2, %3, %4) SELECT :target_plid, %2, "
                    "%3 + :position_offset, %4 FROM " STEMSMIX_TRACKS_TABLE
                    " WHERE %1 = :source_plid")
                    .arg(
                            STEMSMIXTRACKSTABLE_STEMSMIXID,
                            STEMSMIXTRACKSTABLE_TRACKID,
                            STEMSMIXTRACKSTABLE_POSITION,
                            STEMSMIXTRACKSTABLE_DATETIMEADDED));
    query.bindValue(":position_offset", positionOffset);
    query.bindValue(":source_plid", sourceStemsMixID);
    query.bindValue(":target_plid", targetStemsMixID);

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }

    // Query each added track and its new position.
    // SELECT track_id, position FROM StemsMixTracks WHERE stemsmix_id = :target_plid AND position > :position_offset;
    query.prepare(
            QStringLiteral(
                    "SELECT %2, %3 FROM " STEMSMIX_TRACKS_TABLE
                    " WHERE %1 = :target_plid AND %3 > :position_offset")
                    .arg(
                            STEMSMIXTRACKSTABLE_STEMSMIXID,
                            STEMSMIXTRACKSTABLE_TRACKID,
                            STEMSMIXTRACKSTABLE_POSITION));
    query.bindValue(":target_plid", targetStemsMixID);
    query.bindValue(":position_offset", positionOffset);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }

    // Commit the transaction
    transaction.commit();

    // Let subscribers know about each added track.
    while (query.next()) {
        TrackId copiedTrackId(query.value(0));
        int copiedPosition = query.value(1).toInt();
        m_stemsmixsTrackIsIn.insert(copiedTrackId, targetStemsMixID);
        emit trackAdded(targetStemsMixID, copiedTrackId, copiedPosition);
    }
    emit tracksChanged(QSet<int>{targetStemsMixID});
    return true;
}

int StemsMixDAO::getMaxPosition(const int stemsmixId) const {
    // Find out the highest position existing in the stemsmix so we know what
    // position this track should have.
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT max(position) as position FROM StemsMixTracks "
            "WHERE stemsmix_id = :id"));
    query.bindValue(":id", stemsmixId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
    }

    // Get the position of the highest track in the stemsmix.
    int position = 0;
    if (query.next()) {
        position = query.value(query.record().indexOf("position")).toInt();
    }
    return position;
}

void StemsMixDAO::removeTracksFromStemsMixs(const QList<TrackId>& trackIds) {
    // copy the hash, because there is no guarantee that "it" is valid after remove
    QMultiHash<TrackId, int> stemsmixsTrackIsInCopy = m_stemsmixsTrackIsIn;
    QSet<int> stemsmixIds;

    ScopedTransaction transaction(m_database);
    for (const auto& trackId : trackIds) {
        for (auto it = stemsmixsTrackIsInCopy.constBegin();
                it != stemsmixsTrackIsInCopy.constEnd();
                ++it) {
            if (it.key() == trackId) {
                const auto stemsmixId = it.value();
                // keep tracks in history stemsmixs
                if (getHiddenType(stemsmixId) == StemsMixDAO::PLHT_SET_LOG) {
                    continue;
                }
                removeTracksFromStemsMixByIdInner(stemsmixId, trackId);
                stemsmixIds.insert(stemsmixId);
            }
        }
    }
    transaction.commit();

    emit tracksChanged(stemsmixIds);
}

int StemsMixDAO::tracksInStemsMix(const int stemsmixId) const {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "SELECT COUNT(id) AS count FROM StemsMixTracks "
            "WHERE stemsmix_id = :stemsmix_id"));
    query.bindValue(":stemsmix_id", stemsmixId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query) << "Couldn't get the number of tracks in stemsmix"
                                << stemsmixId;
        return -1;
    }
    int count = -1;
    const int countColumn = query.record().indexOf("count");
    while (query.next()) {
        count = query.value(countColumn).toInt();
    }
    return count;
}

void StemsMixDAO::moveTrack(const int stemsmixId, const int oldPosition, const int newPosition) {
    ScopedTransaction transaction(m_database);
    QSqlQuery query(m_database);

    // Algorithm for code below
    // Case 1: destination < source (newPositon < oldPosition)
    //    1) Set position = -1 where pos=source -- Gives that track a dummy index to keep stuff simple.
    //    2) Decrement position where pos >= dest AND pos < source
    //    3) Set position = dest where pos=-1 -- Move track from dummy pos to final destination.

    // Case 2: destination > source (newPos > oldPos)
    //   1) Set position=-1 where pos=source -- Give track a dummy index again.
    //   2) Decrement position where pos > source AND pos <= dest
    //   3) Set position=dest where pos=-1 -- Move that track from dummy pos to final destination

    // Move moved track to dummy position -1
    query.prepare(QStringLiteral(
            "UPDATE StemsMixTracks SET position=-1 "
            "WHERE position=:position AND "
            "stemsmix_id=:id"));
    query.bindValue(":position", oldPosition);
    query.bindValue(":id", stemsmixId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
    }

    if (newPosition < oldPosition) {
        query.prepare(QStringLiteral(
                "UPDATE StemsMixTracks SET position=position+1 "
                "WHERE position >= :new_position AND position < :old_position AND "
                "stemsmix_id=:id"));
        query.bindValue(":new_position", newPosition);
        query.bindValue(":old_position", oldPosition);
        query.bindValue(":id", stemsmixId);
    } else {
        query.prepare(QStringLiteral(
                "UPDATE StemsMixTracks SET position=position-1 "
                "WHERE position > :old_position AND position <= :new_position AND "
                "stemsmix_id=:id"));
        query.bindValue(":new_position", newPosition);
        query.bindValue(":old_position", oldPosition);
        query.bindValue(":id", stemsmixId);
    }
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
    }

    query.prepare(QStringLiteral(
            "UPDATE StemsMixTracks SET position=:new_position "
            "WHERE position=-1 AND "
            "stemsmix_id=:id"));
    query.bindValue(":new_position", newPosition);
    query.bindValue(":id", stemsmixId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
    }

    transaction.commit();

    // Print out any SQL error, if there was one.
    if (query.lastError().isValid()) {
        qDebug() << query.lastError();
    }

    emit tracksChanged(QSet<int>{stemsmixId});
}

void StemsMixDAO::searchForDuplicateTrack(const int fromPosition,
        const int toPosition,
        TrackId trackID,
        const int excludePosition,
        const int otherTrackPosition,
        const QHash<int, TrackId>* pTrackPositionIds,
        int* pTrackDistance) {
    //qDebug() << "        Searching from " << fromPosition << " to " << toPosition;
    for (int pos = fromPosition; pos <= toPosition; pos++) {
        if (pTrackPositionIds->value(pos) == trackID &&
                pos != excludePosition) {
            int tempTrackDistance =
                    (otherTrackPosition - pos) * (otherTrackPosition - pos);
            if (tempTrackDistance < *pTrackDistance || *pTrackDistance == -1) {
                *pTrackDistance = tempTrackDistance;
            }
        }
    }
}

bool StemsMixDAO::isTrackInStemsMix(TrackId trackId, const int stemsmixId) const {
    return m_stemsmixsTrackIsIn.contains(trackId, stemsmixId);
}

void StemsMixDAO::getStemsMixsTrackIsIn(TrackId trackId,
        QSet<int>* stemsmixSet) const {
    stemsmixSet->clear();
    for (auto it = m_stemsmixsTrackIsIn.constFind(trackId);
            it != m_stemsmixsTrackIsIn.constEnd() && it.key() == trackId;
            ++it) {
        stemsmixSet->insert(it.value());
    }
}

void StemsMixDAO::setAutoDJProcessor(AutoDJProcessor* pAutoDJProcessor) {
    m_pAutoDJProcessor = pAutoDJProcessor;
}

void StemsMixDAO::addTracksToAutoDJQueue(const QList<TrackId>& trackIds, AutoDJSendLoc loc) {
    int iAutoDJStemsMixId = getStemsMixIdFromName(AUTODJ_TABLE);
    if (iAutoDJStemsMixId == -1) {
        return;
    }

    // If the first track is already loaded to the player,
    // alter the stemsmix only below the first track
    int position =
            (m_pAutoDJProcessor && m_pAutoDJProcessor->nextTrackLoaded()) ? 2 : 1;

    switch (loc) {
    case AutoDJSendLoc::TOP:
        insertTracksIntoStemsMix(trackIds, iAutoDJStemsMixId, position);
        break;
    case AutoDJSendLoc::BOTTOM:
        appendTracksToStemsMix(trackIds, iAutoDJStemsMixId);
        break;
    case AutoDJSendLoc::REPLACE:
        if (removeTracksFromStemsMix(iAutoDJStemsMixId, position)) {
            appendTracksToStemsMix(trackIds, iAutoDJStemsMixId);
        }
        break;
    }
}
