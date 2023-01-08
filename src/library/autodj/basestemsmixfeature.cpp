#include "library/autodj/basestemsmixfeature.h"

#include <qlist.h>

#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>

#include "controllers/keyboard/keyboardeventfilter.h"
#include "library/export/trackexportwizard.h"
#include "library/library.h"
#include "library/library_prefs.h"
#include "library/parser.h"
#include "library/parsercsv.h"
#include "library/parserm3u.h"
#include "library/parserpls.h"
#include "library/autodj/stemsmixtablemodel.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/treeitem.h"
#include "library/treeitemmodel.h"
#include "moc_basestemsMixfeature.cpp"
#include "track/track.h"
#include "track/trackid.h"
#include "util/assert.h"
#include "util/file.h"
#include "widget/wlibrary.h"
#include "widget/wlibrarysidebar.h"
#include "widget/wlibrarytextbrowser.h"

namespace {
constexpr QChar kUnsafeFilenameReplacement = '-';
const ConfigKey kConfigKeyLastImportExportStemsMixDirectory(
        "[Library]", "LastImportExportStemsMixDirectory");

} // anonymous namespace

using namespace mixxx::library::prefs;

BaseStemsMixFeature::BaseStemsMixFeature(
        Library* pLibrary,
        UserSettingsPointer pConfig,
        StemsMixTableModel* pModel,
        const QString& rootViewName,
        const QString& iconName)
        : BaseTrackSetFeature(pLibrary, pConfig, rootViewName, iconName),
          m_stemsMixDao(pLibrary->trackCollectionManager()
                                ->internalCollection()
                                ->getStemsMixDAO()),
          m_pStemsMixTableModel(pModel) {
    pModel->setParent(this);

    initActions();
}

void BaseStemsMixFeature::initActions() {
    m_pCreateStemsMixAction = new QAction(tr("Create New StemsMix"), this);
    connect(m_pCreateStemsMixAction,
            &QAction::triggered,
            this,
            &BaseStemsMixFeature::slotCreateStemsMix);

    m_pRenameStemsMixAction = new QAction(tr("Rename"), this);
    connect(m_pRenameStemsMixAction,
            &QAction::triggered,
            this,
            &BaseStemsMixFeature::slotRenameStemsMix);
    m_pDuplicateStemsMixAction = new QAction(tr("Duplicate"), this);
    connect(m_pDuplicateStemsMixAction,
            &QAction::triggered,
            this,
            &BaseStemsMixFeature::slotDuplicateStemsMix);
    m_pDeleteStemsMixAction = new QAction(tr("Remove"), this);
    connect(m_pDeleteStemsMixAction,
            &QAction::triggered,
            this,
            &BaseStemsMixFeature::slotDeleteStemsMix);
    m_pLockStemsMixAction = new QAction(tr("Lock"), this);
    connect(m_pLockStemsMixAction,
            &QAction::triggered,
            this,
            &BaseStemsMixFeature::slotToggleStemsMixLock);

    m_pAddToAutoDJAction = new QAction(tr("Add to Auto DJ Queue (bottom)"), this);
    connect(m_pAddToAutoDJAction,
            &QAction::triggered,
            this,
            &BaseStemsMixFeature::slotAddToAutoDJ);
    m_pAddToAutoDJTopAction = new QAction(tr("Add to Auto DJ Queue (top)"), this);
    connect(m_pAddToAutoDJTopAction,
            &QAction::triggered,
            this,
            &BaseStemsMixFeature::slotAddToAutoDJTop);
    m_pAddToAutoDJReplaceAction = new QAction(tr("Add to Auto DJ Queue (replace)"), this);
    connect(m_pAddToAutoDJReplaceAction,
            &QAction::triggered,
            this,
            &BaseStemsMixFeature::slotAddToAutoDJReplace);

    m_pAnalyzeStemsMixAction = new QAction(tr("Analyze entire StemsMix"), this);
    connect(m_pAnalyzeStemsMixAction,
            &QAction::triggered,
            this,
            &BaseStemsMixFeature::slotAnalyzeStemsMix);

    m_pImportStemsMixAction = new QAction(tr("Import StemsMix"), this);
    connect(m_pImportStemsMixAction,
            &QAction::triggered,
            this,
            &BaseStemsMixFeature::slotImportStemsMix);
    m_pCreateImportStemsMixAction = new QAction(tr("Import StemsMix"), this);
    connect(m_pCreateImportStemsMixAction,
            &QAction::triggered,
            this,
            &BaseStemsMixFeature::slotCreateImportStemsMix);
    m_pExportStemsMixAction = new QAction(tr("Export StemsMix"), this);
    connect(m_pExportStemsMixAction,
            &QAction::triggered,
            this,
            &BaseStemsMixFeature::slotExportStemsMix);
    m_pExportTrackFilesAction = new QAction(tr("Export Track Files"), this);
    connect(m_pExportTrackFilesAction,
            &QAction::triggered,
            this,
            &BaseStemsMixFeature::slotExportTrackFiles);

    connect(&m_stemsMixDao,
            &StemsMixDAO::added,
            this,
            &BaseStemsMixFeature::slotStemsMixTableChangedAndSelect);
    connect(&m_stemsMixDao,
            &StemsMixDAO::lockChanged,
            this,
            &BaseStemsMixFeature::slotStemsMixTableChangedAndScrollTo);
    connect(&m_stemsMixDao,
            &StemsMixDAO::deleted,
            this,
            &BaseStemsMixFeature::slotStemsMixTableChanged);
    connect(&m_stemsMixDao,
            &StemsMixDAO::tracksChanged,
            this,
            &BaseStemsMixFeature::slotStemsMixContentChanged);
    connect(&m_stemsMixDao,
            &StemsMixDAO::renamed,
            this,
            &BaseStemsMixFeature::slotStemsMixTableRenamed);

    connect(m_pLibrary,
            &Library::trackSelected,
            this,
            [this](const TrackPointer& pTrack) {
                const auto trackId = pTrack ? pTrack->getId() : TrackId{};
                slotTrackSelected(trackId);
            });
    connect(m_pLibrary,
            &Library::switchToView,
            this,
            &BaseStemsMixFeature::slotResetSelectedTrack);
}

int BaseStemsMixFeature::stemsMixIdFromIndex(const QModelIndex& index) {
    TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
    if (item == nullptr) {
        return kInvalidStemsMixId;
    }

    bool ok = false;
    int stemsMixId = item->getData().toInt(&ok);
    if (ok) {
        return stemsMixId;
    } else {
        return kInvalidStemsMixId;
    }
}

void BaseStemsMixFeature::selectStemsMixInSidebar(int stemsMixId, bool select) {
    if (!m_pSidebarWidget) {
        return;
    }
    if (stemsMixId == kInvalidStemsMixId) {
        return;
    }
    QModelIndex index = indexFromStemsMixId(stemsMixId);
    if (index.isValid() && m_pSidebarWidget) {
        m_pSidebarWidget->selectChildIndex(index, select);
    }
}

void BaseStemsMixFeature::activateChild(const QModelIndex& index) {
    //qDebug() << "BaseStemsMixFeature::activateChild()" << index;
    int stemsMixId = stemsMixIdFromIndex(index);
    if (stemsMixId == kInvalidStemsMixId) {
        // This happens if user clicks on group nodes
        // like the year folder in the history feature
        return;
    }
    emit saveModelState();
    m_pStemsMixTableModel->setTableModel(stemsMixId);
    emit showTrackModel(m_pStemsMixTableModel);
    emit enableCoverArtDisplay(true);
}

void BaseStemsMixFeature::activateStemsMix(int stemsMixId) {
    VERIFY_OR_DEBUG_ASSERT(stemsMixId != kInvalidStemsMixId) {
        return;
    }
    QModelIndex index = indexFromStemsMixId(stemsMixId);
    //qDebug() << "BaseStemsMixFeature::activateStemsMix()" << stemsMixId << index;
    VERIFY_OR_DEBUG_ASSERT(index.isValid()) {
        return;
    }
    emit saveModelState();
    m_lastRightClickedIndex = index;
    m_pStemsMixTableModel->setTableModel(stemsMixId);
    emit showTrackModel(m_pStemsMixTableModel);
    emit enableCoverArtDisplay(true);
    // Update selection
    emit featureSelect(this, m_lastRightClickedIndex);
    if (!m_pSidebarWidget) {
        return;
    }
    m_pSidebarWidget->selectChildIndex(m_lastRightClickedIndex);
}

void BaseStemsMixFeature::slotRenameStemsMix() {
    int stemsMixId = stemsMixIdFromIndex(m_lastRightClickedIndex);
    if (stemsMixId == kInvalidStemsMixId) {
        return;
    }
    QString oldName = m_stemsMixDao.getStemsMixName(stemsMixId);
    bool locked = m_stemsMixDao.isStemsMixLocked(stemsMixId);

    if (locked) {
        qDebug() << "Skipping stemsMix rename because stemsMix" << stemsMixId
                 << "is locked.";
        return;
    }
    QString newName;
    bool validNameGiven = false;

    while (!validNameGiven) {
        bool ok = false;
        newName = QInputDialog::getText(nullptr,
                tr("Rename StemsMix"),
                tr("Enter new name for stemsMix:"),
                QLineEdit::Normal,
                oldName,
                &ok)
                          .trimmed();
        if (!ok || oldName == newName) {
            return;
        }

        int existingId = m_stemsMixDao.getStemsMixIdFromName(newName);

        if (existingId != kInvalidStemsMixId) {
            QMessageBox::warning(nullptr,
                    tr("Renaming StemsMix Failed"),
                    tr("A stemsMix by that name already exists."));
        } else if (newName.isEmpty()) {
            QMessageBox::warning(nullptr,
                    tr("Renaming StemsMix Failed"),
                    tr("A stemsMix cannot have a blank name."));
        } else {
            validNameGiven = true;
        }
    }

    m_stemsMixDao.renameStemsMix(stemsMixId, newName);
}

void BaseStemsMixFeature::slotDuplicateStemsMix() {
    int oldStemsMixId = stemsMixIdFromIndex(m_lastRightClickedIndex);
    if (oldStemsMixId == kInvalidStemsMixId) {
        return;
    }

    QString oldName = m_stemsMixDao.getStemsMixName(oldStemsMixId);

    QString name;
    bool validNameGiven = false;

    while (!validNameGiven) {
        bool ok = false;
        name = QInputDialog::getText(nullptr,
                tr("Duplicate StemsMix"),
                tr("Enter name for new stemsMix:"),
                QLineEdit::Normal,
                //: Appendix to default name when duplicating a stemsMix
                oldName + tr("_copy", "//:"),
                &ok)
                       .trimmed();
        if (!ok || oldName == name) {
            return;
        }

        int existingId = m_stemsMixDao.getStemsMixIdFromName(name);

        if (existingId != kInvalidStemsMixId) {
            QMessageBox::warning(nullptr,
                    tr("StemsMix Creation Failed"),
                    tr("A stemsMix by that name already exists."));
        } else if (name.isEmpty()) {
            QMessageBox::warning(nullptr,
                    tr("StemsMix Creation Failed"),
                    tr("A stemsMix cannot have a blank name."));
        } else {
            validNameGiven = true;
        }
    }

    int newStemsMixId = m_stemsMixDao.createStemsMix(name);

    if (newStemsMixId != kInvalidStemsMixId &&
            m_stemsMixDao.copyStemsMixTracks(oldStemsMixId, newStemsMixId)) {
        activateStemsMix(newStemsMixId);
    }
}

void BaseStemsMixFeature::slotToggleStemsMixLock() {
    int stemsMixId = stemsMixIdFromIndex(m_lastRightClickedIndex);
    if (stemsMixId == kInvalidStemsMixId) {
        return;
    }
    bool locked = !m_stemsMixDao.isStemsMixLocked(stemsMixId);

    if (!m_stemsMixDao.setStemsMixLocked(stemsMixId, locked)) {
        qDebug() << "Failed to toggle lock of stemsMixId " << stemsMixId;
    }
}

void BaseStemsMixFeature::slotCreateStemsMix() {
    QString name;
    bool validNameGiven = false;

    while (!validNameGiven) {
        bool ok = false;
        name = QInputDialog::getText(nullptr,
                tr("Create New StemsMix"),
                tr("Enter name for new stemsMix:"),
                QLineEdit::Normal,
                tr("New StemsMix"),
                &ok)
                       .trimmed();
        if (!ok) {
            return;
        }

        int existingId = m_stemsMixDao.getStemsMixIdFromName(name);

        if (existingId != kInvalidStemsMixId) {
            QMessageBox::warning(nullptr,
                    tr("StemsMix Creation Failed"),
                    tr("A stemsMix by that name already exists."));
        } else if (name.isEmpty()) {
            QMessageBox::warning(nullptr,
                    tr("StemsMix Creation Failed"),
                    tr("A stemsMix cannot have a blank name."));
        } else {
            validNameGiven = true;
        }
    }

    int stemsMixId = m_stemsMixDao.createStemsMix(name);

    if (stemsMixId != kInvalidStemsMixId) {
        activateStemsMix(stemsMixId);
    } else {
        QMessageBox::warning(nullptr,
                tr("StemsMix Creation Failed"),
                tr("An unknown error occurred while creating stemsMix: ") + name);
    }
}

/// Returns a stemsMix that is a sibling inside the same parent
/// as the start index
int BaseStemsMixFeature::getSiblingStemsMixIdOf(QModelIndex& start) {
    QModelIndex nextIndex = start.sibling(start.row() + 1, start.column());
    if (!nextIndex.isValid() && start.row() > 0) {
        // No stemsMix below, looking above.
        nextIndex = start.sibling(start.row() - 1, start.column());
    }
    if (nextIndex.isValid()) {
        TreeItem* pTreeItem = m_pSidebarModel->getItem(nextIndex);
        DEBUG_ASSERT(pTreeItem != nullptr);
        if (!pTreeItem->hasChildren()) {
            return stemsMixIdFromIndex(nextIndex);
        }
    }
    return kInvalidStemsMixId;
}

void BaseStemsMixFeature::slotDeleteStemsMix() {
    //qDebug() << "slotDeleteStemsMix() row:" << m_lastRightClickedIndex.data();
    if (!m_lastRightClickedIndex.isValid()) {
        return;
    }

    int stemsMixId = stemsMixIdFromIndex(m_lastRightClickedIndex);
    if (stemsMixId == kInvalidStemsMixId) {
        return;
    }

    // we will switch to the sibling if the deleted stemsMix is currently active
    bool wasActive = m_pStemsMixTableModel->getStemsMix() == stemsMixId;

    VERIFY_OR_DEBUG_ASSERT(stemsMixId >= 0) {
        return;
    }

    bool locked = m_stemsMixDao.isStemsMixLocked(stemsMixId);
    if (locked) {
        qDebug() << "Skipping stemsMix deletion because stemsMix" << stemsMixId << "is locked.";
        return;
    }

    int siblingId = getSiblingStemsMixIdOf(m_lastRightClickedIndex);

    QMessageBox::StandardButton btn = QMessageBox::question(nullptr,
            tr("Confirm Deletion"),
            tr("Do you really want to delete this stemsMix?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
    if (btn == QMessageBox::No) {
        return;
    }

    m_stemsMixDao.deleteStemsMix(stemsMixId);

    if (siblingId == kInvalidStemsMixId) {
        return;
    }
    if (wasActive) {
        activateStemsMix(siblingId);
    } else if (m_pSidebarWidget) {
        m_pSidebarWidget->selectChildIndex(indexFromStemsMixId(siblingId), false);
    }
}

//TODO
void BaseStemsMixFeature::slotImportStemsMix() {
    return;
    /*
    //qDebug() << "slotImportStemsMix() row:" << m_lastRightClickedIndex.data();
    const QString stemsMixFile = getStemsMixFile();
    if (stemsMixFile.isEmpty()) {
        return;
    }

    // Update the import/export stemsMix directory
    QString fileDirectory(stemsMixFile);
    fileDirectory.truncate(stemsMixFile.lastIndexOf("/"));
    m_pConfig->set(kConfigKeyLastImportExportStemsMixDirectory,
            ConfigValue(fileDirectory));

    slotImportStemsMixFile(stemsMixFile);
    activateChild(m_lastRightClickedIndex);
    */
}

void BaseStemsMixFeature::slotImportStemsMixFile(const QString& stemsMix_file) {
    // The user has picked a new directory via a file dialog. This means the
    // system sandboxer (if we are sandboxed) has granted us permission to this
    // folder. We don't need access to this file on a regular basis so we do not
    // register a security bookmark.

    QList<QString> locations = Parser::parse(stemsMix_file);
    // Iterate over the List that holds locations of stemsMix entries
    m_pStemsMixTableModel->addTracks(QModelIndex(), locations);
}

void BaseStemsMixFeature::slotCreateImportStemsMix() {
    // Get file to read
    const QStringList stemsMixFiles = LibraryFeature::getStemsMixFiles();
    if (stemsMixFiles.isEmpty()) {
        return;
    }

    // Set last import directory
    QString fileDirectory(stemsMixFiles.first());
    fileDirectory.truncate(stemsMixFiles.first().lastIndexOf("/"));
    m_pConfig->set(kConfigKeyLastImportExportStemsMixDirectory,
            ConfigValue(fileDirectory));

    int lastStemsMixId = kInvalidStemsMixId;

    // For each selected element create a different stemsMix.
    for (const QString& stemsMixFile : stemsMixFiles) {
        const QFileInfo fileInfo(stemsMixFile);
        // Get a valid name
        const QString baseName = fileInfo.baseName();
        QString name;

        bool validNameGiven = false;
        int i = 0;
        while (!validNameGiven) {
            name = baseName;
            if (i != 0) {
                name += QString::number(i);
            }

            // Check name
            int existingId = m_stemsMixDao.getStemsMixIdFromName(name);

            validNameGiven = (existingId == kInvalidStemsMixId);
            ++i;
        }

        lastStemsMixId = m_stemsMixDao.createStemsMix(name);
        if (lastStemsMixId != kInvalidStemsMixId) {
            emit saveModelState();
            m_pStemsMixTableModel->setTableModel(lastStemsMixId);
        } else {
            QMessageBox::warning(nullptr,
                    tr("StemsMix Creation Failed"),
                    tr("An unknown error occurred while creating stemsMix: ") + name);
            return;
        }

        slotImportStemsMixFile(stemsMixFile);
    }
    activateStemsMix(lastStemsMixId);
}

void BaseStemsMixFeature::slotExportStemsMix() {
    int stemsMixId = stemsMixIdFromIndex(m_lastRightClickedIndex);
    if (stemsMixId == kInvalidStemsMixId) {
        return;
    }
    QString stemsMixName = m_stemsMixDao.getStemsMixName(stemsMixId);
    // replace separator character with something generic
    stemsMixName = stemsMixName.replace(QDir::separator(), kUnsafeFilenameReplacement);
    qDebug() << "Export stemsMix" << stemsMixName;

    QString lastStemsMixDirectory = m_pConfig->getValue(
            kConfigKeyLastImportExportStemsMixDirectory,
            QStandardPaths::writableLocation(QStandardPaths::MusicLocation));

    // Open a dialog to let the user choose the file location for stemsMix export.
    // The location is set to the last used directory for import/export and the file
    // name to the stemsMix name.
    const QString fileLocation = getFilePathWithVerifiedExtensionFromFileDialog(
            tr("Export StemsMix"),
            lastStemsMixDirectory.append("/").append(stemsMixName).append(".m3u"),
            tr("M3U StemsMix (*.m3u);;M3U8 StemsMix (*.m3u8);;"
               "PLS StemsMix (*.pls);;Text CSV (*.csv);;Readable Text (*.txt)"),
            tr("M3U StemsMix (*.m3u)"));
    // Exit method if the file name is empty because the user cancelled the save dialog.
    if (fileLocation.isEmpty()) {
        return;
    }

    // Update the import/export stemsMix directory
    QString fileDirectory(fileLocation);
    fileDirectory.truncate(fileLocation.lastIndexOf("/"));
    m_pConfig->set(kConfigKeyLastImportExportStemsMixDirectory,
            ConfigValue(fileDirectory));

    // The user has picked a new directory via a file dialog. This means the
    // system sandboxer (if we are sandboxed) has granted us permission to this
    // folder. We don't need access to this file on a regular basis so we do not
    // register a security bookmark.

    // Create a new table model since the main one might have an active search.
    // This will only export songs that we think exist on default
    QScopedPointer<StemsMixTableModel> pStemsMixTableModel(
            new StemsMixTableModel(this,
                    m_pLibrary->trackCollectionManager(),
                    "mixxx.db.model.stemsMix_export"));

    emit saveModelState();
    pStemsMixTableModel->setTableModel(m_pStemsMixTableModel->getStemsMix());
    pStemsMixTableModel->setSort(
            pStemsMixTableModel->fieldIndex(
                    ColumnCache::COLUMN_STEMSMIXTRACKSTABLE_POSITION),
            Qt::AscendingOrder);
    pStemsMixTableModel->select();

    // check config if relative paths are desired
    bool useRelativePath = m_pConfig->getValue<bool>(
            kUseRelativePathOnExportConfigKey);

    if (fileLocation.endsWith(".csv", Qt::CaseInsensitive)) {
        ParserCsv::writeCSVFile(fileLocation, pStemsMixTableModel.data(), useRelativePath);
    } else if (fileLocation.endsWith(".txt", Qt::CaseInsensitive)) {
        if (m_stemsMixDao.getHiddenType(pStemsMixTableModel->getStemsMix()) ==
                StemsMixDAO::PLHT_SET_LOG) {
            ParserCsv::writeReadableTextFile(fileLocation, pStemsMixTableModel.data(), true);
        } else {
            ParserCsv::writeReadableTextFile(fileLocation, pStemsMixTableModel.data(), false);
        }
    } else {
        // Create and populate a list of files of the stemsMix
        QList<QString> stemsMixItems;
        int rows = pStemsMixTableModel->rowCount();
        for (int i = 0; i < rows; ++i) {
            QModelIndex index = pStemsMixTableModel->index(i, 0);
            stemsMixItems << pStemsMixTableModel->getTrackLocation(index);
        }
        exportStemsMixItemsIntoFile(
                fileLocation,
                stemsMixItems,
                useRelativePath);
    }
}

void BaseStemsMixFeature::slotExportTrackFiles() {
    QScopedPointer<StemsMixTableModel> pStemsMixTableModel(
            new StemsMixTableModel(this,
                    m_pLibrary->trackCollectionManager(),
                    "mixxx.db.model.stemsMix_export"));

    emit saveModelState();
    pStemsMixTableModel->setTableModel(m_pStemsMixTableModel->getStemsMix());
    pStemsMixTableModel->setSort(pStemsMixTableModel->fieldIndex(
                                         ColumnCache::COLUMN_STEMSMIXTRACKSTABLE_POSITION),
            Qt::AscendingOrder);
    pStemsMixTableModel->select();

    int rows = pStemsMixTableModel->rowCount();
    TrackPointerList tracks;
    for (int i = 0; i < rows; ++i) {
        QModelIndex index = pStemsMixTableModel->index(i, 0);
        tracks.push_back(pStemsMixTableModel->getTrack(index));
    }

    TrackExportWizard track_export(nullptr, m_pConfig, tracks);
    track_export.exportTracks();
}

void BaseStemsMixFeature::slotAddToAutoDJ() {
    //qDebug() << "slotAddToAutoDJ() row:" << m_lastRightClickedIndex.data();
    addToAutoDJ(StemsMixDAO::AutoDJSendLoc::BOTTOM);
}

void BaseStemsMixFeature::slotAddToAutoDJTop() {
    //qDebug() << "slotAddToAutoDJTop() row:" << m_lastRightClickedIndex.data();
    addToAutoDJ(StemsMixDAO::AutoDJSendLoc::TOP);
}

void BaseStemsMixFeature::slotAddToAutoDJReplace() {
    //qDebug() << "slotAddToAutoDJReplace() row:" << m_lastRightClickedIndex.data();
    addToAutoDJ(StemsMixDAO::AutoDJSendLoc::REPLACE);
}

void BaseStemsMixFeature::addToAutoDJ(StemsMixDAO::AutoDJSendLoc loc) {
    //qDebug() << "slotAddToAutoDJ() row:" << m_lastRightClickedIndex.data();
    if (m_lastRightClickedIndex.isValid()) {
        int stemsMixId = stemsMixIdFromIndex(m_lastRightClickedIndex);
        if (stemsMixId >= 0) {
            // Insert this stemsMix
            m_stemsMixDao.addStemsMixToAutoDJQueue(stemsMixId, loc);
        }
    }
}

void BaseStemsMixFeature::slotAnalyzeStemsMix() {
    if (m_lastRightClickedIndex.isValid()) {
        int stemsMixId = stemsMixIdFromIndex(m_lastRightClickedIndex);
        if (stemsMixId >= 0) {
            QList<TrackId> ids = m_stemsMixDao.getTrackIds(stemsMixId);
            QList<AnalyzerScheduledTrack> tracks;
            for (auto id : ids) {
                tracks.append(id);
            }
            emit analyzeTracks(tracks);
        }
    }
}

TreeItemModel* BaseStemsMixFeature::sidebarModel() const {
    return m_pSidebarModel;
}

void BaseStemsMixFeature::bindLibraryWidget(WLibrary* libraryWidget,
        KeyboardEventFilter* keyboard) {
    Q_UNUSED(keyboard);
    WLibraryTextBrowser* edit = new WLibraryTextBrowser(libraryWidget);
    edit->setHtml(getRootViewHtml());
    edit->setOpenLinks(false);
    connect(edit,
            &WLibraryTextBrowser::anchorClicked,
            this,
            &BaseStemsMixFeature::htmlLinkClicked);
    libraryWidget->registerView(m_rootViewName, edit);
}

void BaseStemsMixFeature::bindSidebarWidget(WLibrarySidebar* pSidebarWidget) {
    // store the sidebar widget pointer for later use in onRightClickChild
    DEBUG_ASSERT(!m_pSidebarWidget);
    m_pSidebarWidget = pSidebarWidget;
}

void BaseStemsMixFeature::htmlLinkClicked(const QUrl& link) {
    if (QString(link.path()) == "create") {
        slotCreateStemsMix();
    } else {
        qDebug() << "Unknown stemsMix link clicked" << link.path();
    }
}

void BaseStemsMixFeature::updateChildModel(int stemsMixId) {
    QString stemsMixLabel = fetchStemsMixLabel(stemsMixId);

    QVariant variantId = QVariant(stemsMixId);

    for (int row = 0; row < m_pSidebarModel->rowCount(); ++row) {
        QModelIndex index = m_pSidebarModel->index(row, 0);
        TreeItem* pTreeItem = m_pSidebarModel->getItem(index);
        DEBUG_ASSERT(pTreeItem != nullptr);
        if (!pTreeItem->hasChildren() && // leaf node
                pTreeItem->getData() == variantId) {
            pTreeItem->setLabel(stemsMixLabel);
            decorateChild(pTreeItem, stemsMixId);
        }
    }
}

/**
  * Clears the child model dynamically, but the invisible root item remains
  */
void BaseStemsMixFeature::clearChildModel() {
    m_pSidebarModel->removeRows(0, m_pSidebarModel->rowCount());
}

QModelIndex BaseStemsMixFeature::indexFromStemsMixId(int stemsMixId) {
    QVariant variantId = QVariant(stemsMixId);
    QModelIndexList results = m_pSidebarModel->match(
            m_pSidebarModel->getRootIndex(),
            TreeItemModel::kDataRole,
            variantId,
            1,
            Qt::MatchWrap | Qt::MatchExactly | Qt::MatchRecursive);
    if (!results.isEmpty()) {
        return results.front();
    }
    return QModelIndex();
}

void BaseStemsMixFeature::slotTrackSelected(TrackId trackId) {
    m_selectedTrackId = trackId;
    m_stemsMixDao.getStemsMixsTrackIsIn(m_selectedTrackId, &m_stemsMixIdsOfSelectedTrack);

    for (int row = 0; row < m_pSidebarModel->rowCount(); ++row) {
        QModelIndex index = m_pSidebarModel->index(row, 0);
        TreeItem* pTreeItem = m_pSidebarModel->getItem(index);
        DEBUG_ASSERT(pTreeItem != nullptr);
        markTreeItem(pTreeItem);
    }

    m_pSidebarModel->triggerRepaint();
}

void BaseStemsMixFeature::markTreeItem(TreeItem* pTreeItem) {
    bool ok;
    int stemsMixId = pTreeItem->getData().toInt(&ok);
    if (ok) {
        bool shouldBold = m_stemsMixIdsOfSelectedTrack.contains(stemsMixId);
        pTreeItem->setBold(shouldBold);
        if (shouldBold && pTreeItem->hasParent()) {
            TreeItem* item = pTreeItem;
            // extra parents, because -Werror=parentheses
            while ((item = item->parent())) {
                item->setBold(true);
            }
        }
    }
    if (pTreeItem->hasChildren()) {
        QList<TreeItem*> children = pTreeItem->children();

        for (int i = 0; i < children.size(); i++) {
            markTreeItem(children.at(i));
        }
    }
}

void BaseStemsMixFeature::slotResetSelectedTrack() {
    slotTrackSelected(TrackId{});
}
