#pragma once

#include <QObject>
#include <QMap>
#include <QStringList>

#include "track/keyutils.h"
#include "control/controlproxy.h"
#include "preferences/usersettings.h"

// Caches the index of frequently used columns and provides a lookup-table of
// column name to index.
class ColumnCache : public QObject {
  Q_OBJECT
  public:
    enum Column {
        COLUMN_LIBRARYTABLE_INVALID = -1,
        COLUMN_LIBRARYTABLE_ID = 0,
        COLUMN_LIBRARYTABLE_ARTIST,
        COLUMN_LIBRARYTABLE_TITLE,
        COLUMN_LIBRARYTABLE_ALBUM,
        COLUMN_LIBRARYTABLE_ALBUMARTIST,
        COLUMN_LIBRARYTABLE_YEAR,
        COLUMN_LIBRARYTABLE_GENRE,
        COLUMN_LIBRARYTABLE_COMPOSER,
        COLUMN_LIBRARYTABLE_GROUPING,
        COLUMN_LIBRARYTABLE_TRACKNUMBER,
        COLUMN_LIBRARYTABLE_FILETYPE,
        COLUMN_LIBRARYTABLE_COMMENT,
        COLUMN_LIBRARYTABLE_DURATION,
        COLUMN_LIBRARYTABLE_BITRATE,
        COLUMN_LIBRARYTABLE_BPM,
        COLUMN_LIBRARYTABLE_REPLAYGAIN,
        COLUMN_LIBRARYTABLE_CUEPOINT,
        COLUMN_LIBRARYTABLE_URL,
        COLUMN_LIBRARYTABLE_SAMPLERATE,
        COLUMN_LIBRARYTABLE_WAVESUMMARYHEX,
        COLUMN_LIBRARYTABLE_CHANNELS,
        COLUMN_LIBRARYTABLE_MIXXXDELETED,
        COLUMN_LIBRARYTABLE_DATETIMEADDED,
        COLUMN_LIBRARYTABLE_HEADERPARSED,
        COLUMN_LIBRARYTABLE_TIMESPLAYED,
        COLUMN_LIBRARYTABLE_PLAYED,
        COLUMN_LIBRARYTABLE_RATING,
        COLUMN_LIBRARYTABLE_KEY,
        COLUMN_LIBRARYTABLE_KEY_ID,
        COLUMN_LIBRARYTABLE_BPM_LOCK,
        COLUMN_LIBRARYTABLE_PREVIEW,
        COLUMN_LIBRARYTABLE_COLOR,
        COLUMN_LIBRARYTABLE_COVERART,
        COLUMN_LIBRARYTABLE_COVERART_SOURCE,
        COLUMN_LIBRARYTABLE_COVERART_TYPE,
        COLUMN_LIBRARYTABLE_COVERART_LOCATION,
        COLUMN_LIBRARYTABLE_COVERART_COLOR,
        COLUMN_LIBRARYTABLE_COVERART_DIGEST,
        COLUMN_LIBRARYTABLE_COVERART_HASH,
        COLUMN_LIBRARYTABLE_LAST_PLAYED_AT,

        COLUMN_TRACKLOCATIONSTABLE_LOCATION,
        COLUMN_TRACKLOCATIONSTABLE_FSDELETED,

        COLUMN_PLAYLISTTRACKSTABLE_TRACKID,
        COLUMN_PLAYLISTTRACKSTABLE_POSITION,
        COLUMN_PLAYLISTTRACKSTABLE_PLAYLISTID,
        COLUMN_PLAYLISTTRACKSTABLE_DATETIMEADDED,

        COLUMN_STEMSMIXTRACKSTABLE_TRACKID,
        COLUMN_STEMSMIXTRACKSTABLE_STEMID,
        COLUMN_STEMSMIXTRACKSTABLE_POSITION,
        COLUMN_STEMSMIXTRACKSTABLE_DATETIMEADDED,

        COLUMN_STEMSMIXTRACKSTABLE_ABSOLUTEBARINDEX,
        COLUMN_STEMSMIXTRACKSTABLE_TRACKBARINDEX,
        COLUMN_STEMSMIXTRACKSTABLE_DURATIONBARS,
        COLUMN_STEMSMIXTRACKSTABLE_STEMSMIXID,

        COLUMN_REKORDBOX_ANALYZE_PATH,

        // NUM_COLUMNS should always be the last item.
        NUM_COLUMNS
    };

    explicit ColumnCache(const QStringList& columns = QStringList());

    void setColumns(const QStringList& columns);

    inline int fieldIndex(Column column) const {
        if (column < 0 || column >= NUM_COLUMNS) {
            return -1;
        }
        return m_columnIndexByEnum[column];
    }

    inline int fieldIndex(const QString& columnName) const {
        return m_columnIndexByName.value(columnName, -1);
    }

    inline QString columnName(Column column) const {
        return m_columnNameByEnum[column];
    }

    inline QString columnNameForFieldIndex(int index) const {
        if (index < 0 || index >= m_columnsByIndex.size()) {
            return QString();
        }
        return m_columnsByIndex.at(index);
    }

    inline QString columnSortForFieldIndex(int index) const {
        // Check if there is a special sort clause
        QString format = m_columnSortByIndex.value(index, "%1");
        return format.arg(columnNameForFieldIndex(index));
    }

    void insertColumnSortByEnum(
            Column column,
            const QString& sortFormat) {
        int index = fieldIndex(column);
        if (index < 0) {
            return;
        }
        DEBUG_ASSERT(!m_columnSortByIndex.contains(index));
        m_columnSortByIndex.insert(index, sortFormat);
    }

    void insertColumnNameByEnum(
            Column column,
            const QString& name) {
        DEBUG_ASSERT(!m_columnNameByEnum.contains(column) ||
                m_columnNameByEnum[column] == name);
        m_columnNameByEnum.insert(column, name);
    }

    KeyUtils::KeyNotation keyNotation() const {
        return KeyUtils::keyNotationFromNumericValue(
                m_pKeyNotationCP->get());
    }

  private slots:
    void slotSetKeySortOrder(double);

  private:
    QStringList m_columnsByIndex;
    QMap<int, QString> m_columnSortByIndex;
    QMap<QString, int> m_columnIndexByName;
    QMap<Column, QString> m_columnNameByEnum;
    // A mapping from column enum to logical index.
    int m_columnIndexByEnum[NUM_COLUMNS];

    ControlProxy* m_pKeyNotationCP;
};
