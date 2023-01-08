#include "library/autodj/stemsmixfeature.h"

#include <QFile>
#include <QMenu>
#include <QtDebug>

#include "controllers/keyboard/keyboardeventfilter.h"
#include "library/library.h"
#include "library/parser.h"
#include "library/autodj/stemsmixtablemodel.h"
#include "library/queryutil.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/treeitem.h"
#include "moc_stemsMixfeature.cpp"
#include "sources/soundsourceproxy.h"
#include "util/db/dbconnection.h"
#include "util/dnd.h"
#include "util/duration.h"
#include "widget/wlibrary.h"
#include "widget/wlibrarysidebar.h"
#include "widget/wlibrarytextbrowser.h"

namespace {

QString createStemsMixLabel(
        const QString& name,
        int count,
        int duration) {
    return QStringLiteral("%1 (%2) %3")
            .arg(name,
                    QString::number(count),
                    mixxx::Duration::formatTime(
                            duration, mixxx::Duration::Precision::SECONDS));
}

} // anonymous namespace

StemsMixFeature::StemsMixFeature(Library* pLibrary, UserSettingsPointer pConfig)
        : BaseStemsMixFeature(pLibrary,
                  pConfig,
                  new StemsMixTableModel(nullptr,
                          pLibrary->trackCollectionManager(),
                          "mixxx.db.model.stemsMix"),
                  QStringLiteral("STEMSMIXHOME"),
                  QStringLiteral("stemsMix")) {
    // construct child model
    std::unique_ptr<TreeItem> pRootItem = TreeItem::newRoot(this);
    m_pSidebarModel->setRootItem(std::move(pRootItem));
    constructChildModel(kInvalidStemsMixId);
}

QVariant StemsMixFeature::title() {
    return tr("StemsMixs");
}

void StemsMixFeature::onRightClick(const QPoint& globalPos) {
    m_lastRightClickedIndex = QModelIndex();
    QMenu menu(m_pSidebarWidget);
    menu.addAction(m_pCreateStemsMixAction);
    menu.addSeparator();
    menu.addAction(m_pCreateImportStemsMixAction);
    menu.exec(globalPos);
}

void StemsMixFeature::onRightClickChild(
        const QPoint& globalPos, const QModelIndex& index) {
    //Save the model index so we can get it in the action slots...
    m_lastRightClickedIndex = index;
    int stemsMixId = stemsMixIdFromIndex(index);

    bool locked = m_stemsMixDao.isStemsMixLocked(stemsMixId);
    m_pDeleteStemsMixAction->setEnabled(!locked);
    m_pRenameStemsMixAction->setEnabled(!locked);

    m_pLockStemsMixAction->setText(locked ? tr("Unlock") : tr("Lock"));

    QMenu menu(m_pSidebarWidget);
    menu.addAction(m_pCreateStemsMixAction);
    menu.addSeparator();
    menu.addAction(m_pRenameStemsMixAction);
    menu.addAction(m_pDuplicateStemsMixAction);
    menu.addAction(m_pDeleteStemsMixAction);
    menu.addAction(m_pLockStemsMixAction);
    menu.addSeparator();
    menu.addAction(m_pAddToAutoDJAction);
    menu.addAction(m_pAddToAutoDJTopAction);
    menu.addAction(m_pAddToAutoDJReplaceAction);
    menu.addSeparator();
    menu.addAction(m_pAnalyzeStemsMixAction);
    menu.addSeparator();
    menu.addAction(m_pImportStemsMixAction);
    menu.addAction(m_pExportStemsMixAction);
    menu.addAction(m_pExportTrackFilesAction);
    menu.exec(globalPos);
}

bool StemsMixFeature::dropAcceptChild(
        const QModelIndex& index, const QList<QUrl>& urls, QObject* pSource) {
    int stemsMixId = stemsMixIdFromIndex(index);
    VERIFY_OR_DEBUG_ASSERT(stemsMixId >= 0) {
        return false;
    }
    // If a track is dropped onto a stemsMix's name, but the track isn't in the
    // library, then add the track to the library before adding it to the
    // stemsMix.
    // pSource != nullptr it is a drop from inside Mixxx and indicates all
    // tracks already in the DB
    QList<TrackId> trackIds = m_pLibrary->trackCollectionManager()
                                      ->resolveTrackIdsFromUrls(urls, !pSource);
    if (!trackIds.size()) {
        return false;
    }

    // Return whether appendTracksToStemsMix succeeded.
    return m_stemsMixDao.appendTracksToStemsMix(trackIds, stemsMixId);
}

bool StemsMixFeature::dragMoveAcceptChild(const QModelIndex& index, const QUrl& url) {
    int stemsMixId = stemsMixIdFromIndex(index);
    bool locked = m_stemsMixDao.isStemsMixLocked(stemsMixId);

    bool formatSupported = SoundSourceProxy::isUrlSupported(url) ||
            true; //TODO: Parser::isStemsMixFilenameSupported(url.toLocalFile());
    return !locked && formatSupported;
}

QList<BaseStemsMixFeature::IdAndLabel> StemsMixFeature::createStemsMixLabels() {
    QSqlDatabase database =
            m_pLibrary->trackCollectionManager()->internalCollection()->database();

    QList<BaseStemsMixFeature::IdAndLabel> stemsMixLabels;
    QString queryString = QStringLiteral(
            "CREATE TEMPORARY VIEW IF NOT EXISTS StemsMixsCountsDurations "
            "AS SELECT "
            "  StemsMixs.id AS id, "
            "  StemsMixs.name AS name, "
            "  LOWER(StemsMixs.name) AS sort_name, "
            "  COUNT(case library.mixxx_deleted when 0 then 1 else null end) "
            "    AS count, "
            "  SUM(case library.mixxx_deleted "
            "    when 0 then library.duration else 0 end) AS durationSeconds "
            "FROM StemsMixs "
            "LEFT JOIN StemsMixTracks "
            "  ON StemsMixTracks.stemsmix_id = StemsMixs.id "
            "LEFT JOIN library "
            "  ON StemsMixTracks.track_id = library.id "
            "  WHERE StemsMixs.hidden = 0 "
            "  GROUP BY StemsMixs.id");
    queryString.append(
            mixxx::DbConnection::collateLexicographically(
                    " ORDER BY sort_name"));
    QSqlQuery query(database);
    if (!query.exec(queryString)) {
        LOG_FAILED_QUERY(query);
    }

    // Setup the sidebar stemsMix model
    QSqlTableModel stemsMixTableModel(this, database);
    stemsMixTableModel.setTable("StemsMixsCountsDurations");
    stemsMixTableModel.select();
    while (stemsMixTableModel.canFetchMore()) {
        stemsMixTableModel.fetchMore();
    }
    QSqlRecord record = stemsMixTableModel.record();
    int nameColumn = record.indexOf("name");
    int idColumn = record.indexOf("id");
    int countColumn = record.indexOf("count");
    int durationColumn = record.indexOf("durationSeconds");

    for (int row = 0; row < stemsMixTableModel.rowCount(); ++row) {
        int id =
                stemsMixTableModel
                        .data(stemsMixTableModel.index(row, idColumn))
                        .toInt();
        QString name =
                stemsMixTableModel
                        .data(stemsMixTableModel.index(row, nameColumn))
                        .toString();
        int count =
                stemsMixTableModel
                        .data(stemsMixTableModel.index(row, countColumn))
                        .toInt();
        int duration =
                stemsMixTableModel
                        .data(stemsMixTableModel.index(row, durationColumn))
                        .toInt();
        BaseStemsMixFeature::IdAndLabel idAndLabel;
        idAndLabel.id = id;
        idAndLabel.label = createStemsMixLabel(name, count, duration);
        stemsMixLabels.append(idAndLabel);
    }
    return stemsMixLabels;
}

QString StemsMixFeature::fetchStemsMixLabel(int stemsMixId) {
    // Setup the sidebar stemsMix model
    QSqlDatabase database =
            m_pLibrary->trackCollectionManager()->internalCollection()->database();
    QSqlTableModel stemsMixTableModel(this, database);
    stemsMixTableModel.setTable("StemsMixsCountsDurations");
    QString filter = "id=" + QString::number(stemsMixId);
    stemsMixTableModel.setFilter(filter);
    stemsMixTableModel.select();
    while (stemsMixTableModel.canFetchMore()) {
        stemsMixTableModel.fetchMore();
    }
    QSqlRecord record = stemsMixTableModel.record();
    int nameColumn = record.indexOf("name");
    int countColumn = record.indexOf("count");
    int durationColumn = record.indexOf("durationSeconds");

    DEBUG_ASSERT(stemsMixTableModel.rowCount() <= 1);
    if (stemsMixTableModel.rowCount() > 0) {
        QString name =
                stemsMixTableModel.data(stemsMixTableModel.index(0, nameColumn))
                        .toString();
        int count = stemsMixTableModel
                            .data(stemsMixTableModel.index(0, countColumn))
                            .toInt();
        int duration =
                stemsMixTableModel
                        .data(stemsMixTableModel.index(0, durationColumn))
                        .toInt();
        return createStemsMixLabel(name, count, duration);
    }
    return QString();
}

/// Purpose: When inserting or removing stemsMixs,
/// we require the sidebar model not to reset.
/// This method queries the database and does dynamic insertion
/// @param selectedId entry which should be selected
QModelIndex StemsMixFeature::constructChildModel(int selectedId) {
    QList<TreeItem*> data_list;
    int selectedRow = -1;

    int row = 0;
    const QList<IdAndLabel> stemsMixLabels = createStemsMixLabels();
    for (const auto& idAndLabel : stemsMixLabels) {
        int stemsMixId = idAndLabel.id;
        QString stemsMixLabel = idAndLabel.label;

        if (selectedId == stemsMixId) {
            // save index for selection
            selectedRow = row;
        }

        // Create the TreeItem whose parent is the invisible root item
        TreeItem* item = new TreeItem(stemsMixLabel, stemsMixId);
        item->setBold(m_stemsMixIdsOfSelectedTrack.contains(stemsMixId));

        decorateChild(item, stemsMixId);
        data_list.append(item);

        ++row;
    }

    // Append all the newly created TreeItems in a dynamic way to the childmodel
    m_pSidebarModel->insertTreeItemRows(data_list, 0);
    if (selectedRow == -1) {
        return QModelIndex();
    }
    return m_pSidebarModel->index(selectedRow, 0);
}

void StemsMixFeature::decorateChild(TreeItem* item, int stemsMixId) {
    if (m_stemsMixDao.isStemsMixLocked(stemsMixId)) {
        item->setIcon(
                QIcon(":/images/library/ic_library_locked_tracklist.svg"));
    } else {
        item->setIcon(QIcon());
    }
}

void StemsMixFeature::slotStemsMixTableChanged(int stemsMixId) {
    //qDebug() << "slotStemsMixTableChanged() stemsMixId:" << stemsMixId;
    enum StemsMixDAO::HiddenType type = m_stemsMixDao.getHiddenType(stemsMixId);
    if (type == StemsMixDAO::PLHT_NOT_HIDDEN ||
            type == StemsMixDAO::PLHT_UNKNOWN) { // In case of a deleted StemsMix
        clearChildModel();
        m_lastRightClickedIndex = constructChildModel(stemsMixId);
    }
}

void StemsMixFeature::slotStemsMixContentChanged(QSet<int> stemsMixIds) {
    for (const auto stemsMixId : qAsConst(stemsMixIds)) {
        enum StemsMixDAO::HiddenType type =
                m_stemsMixDao.getHiddenType(stemsMixId);
        if (type == StemsMixDAO::PLHT_NOT_HIDDEN ||
                type == StemsMixDAO::PLHT_UNKNOWN) { // In case of a deleted StemsMix
            updateChildModel(stemsMixId);
        }
    }
}

void StemsMixFeature::slotStemsMixTableRenamed(
        int stemsMixId, const QString& newName) {
    Q_UNUSED(newName);
    //qDebug() << "slotStemsMixTableChanged() stemsMixId:" << stemsMixId;
    enum StemsMixDAO::HiddenType type = m_stemsMixDao.getHiddenType(stemsMixId);
    if (type == StemsMixDAO::PLHT_NOT_HIDDEN ||
            type == StemsMixDAO::PLHT_UNKNOWN) { // In case of a deleted StemsMix
        clearChildModel();
        m_lastRightClickedIndex = constructChildModel(stemsMixId);
        if (type != StemsMixDAO::PLHT_UNKNOWN) {
            activateStemsMix(stemsMixId);
        }
    }
}

QString StemsMixFeature::getRootViewHtml() const {
    QString stemsMixsTitle = tr("StemsMixs");
    QString stemsMixsSummary =
            tr("StemsMixs are ordered lists of tracks that allow you to plan "
               "your DJ sets.");
    QString stemsMixsSummary2 =
            tr("Some DJs construct stemsMixs before they perform live, but "
               "others prefer to build them on-the-fly.");
    QString stemsMixsSummary3 =
            tr("When using a stemsMix during a live DJ set, remember to always "
               "pay close attention to how your audience reacts to the music "
               "you've chosen to play.");
    QString stemsMixsSummary4 =
            tr("It may be necessary to skip some tracks in your prepared "
               "stemsMix or add some different tracks in order to maintain the "
               "energy of your audience.");
    QString createStemsMixLink = tr("Create New StemsMix");

    QString html;
    html.append(QStringLiteral("<h2>%1</h2>").arg(stemsMixsTitle));
    html.append(QStringLiteral("<p>%1</p>").arg(stemsMixsSummary));
    html.append(QStringLiteral("<p>%1</p>").arg(stemsMixsSummary2));
    html.append(QStringLiteral("<p>%1<br>%2</p>").arg(stemsMixsSummary3, stemsMixsSummary4));
    html.append(QStringLiteral("<a style=\"color:#0496FF;\" href=\"create\">%1</a>")
                        .arg(createStemsMixLink));
    return html;
}
