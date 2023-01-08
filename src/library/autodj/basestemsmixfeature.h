#pragma once

#include <QAction>
#include <QList>
#include <QModelIndex>
#include <QObject>
#include <QPair>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QUrl>

#include "library/dao/stemsmixdao.h"
#include "library/trackset/basetracksetfeature.h"
#include "track/trackid.h"

class WLibrary;
class KeyboardEventFilter;
class StemsMixTableModel;
class TrackCollectionManager;
class TreeItem;
class WLibrarySidebar;

constexpr int kInvalidStemsMixId = -1;

class BaseStemsMixFeature : public BaseTrackSetFeature {
    Q_OBJECT

  public:
    BaseStemsMixFeature(Library* pLibrary,
            UserSettingsPointer pConfig,
            StemsMixTableModel* pModel,
            const QString& rootViewName,
            const QString& iconName);
    ~BaseStemsMixFeature() override = default;

    TreeItemModel* sidebarModel() const override;

    void bindLibraryWidget(WLibrary* libraryWidget,
            KeyboardEventFilter* keyboard) override;
    void bindSidebarWidget(WLibrarySidebar* pSidebarWidget) override;
    void selectStemsMixInSidebar(int stemsMixId, bool select = true);
    int getSiblingStemsMixIdOf(QModelIndex& start);

  public slots:
    void activateChild(const QModelIndex& index) override;
    virtual void activateStemsMix(int stemsMixId);
    virtual void htmlLinkClicked(const QUrl& link);

    virtual void slotStemsMixTableChanged(int stemsMixId) = 0;
    void slotStemsMixTableChangedAndSelect(int stemsMixId) {
        slotStemsMixTableChanged(stemsMixId);
        selectStemsMixInSidebar(stemsMixId);
    };
    void slotStemsMixTableChangedAndScrollTo(int stemsMixId) {
        slotStemsMixTableChanged(stemsMixId);
        selectStemsMixInSidebar(stemsMixId, false);
    };
    virtual void slotStemsMixTableRenamed(int stemsMixId, const QString& newName) = 0;
    virtual void slotStemsMixContentChanged(QSet<int> stemsMixIds) = 0;
    void slotCreateStemsMix();

  protected slots:
    void slotDeleteStemsMix();
    void slotDuplicateStemsMix();
    void slotAddToAutoDJ();
    void slotAddToAutoDJTop();
    void slotAddToAutoDJReplace();
    void slotRenameStemsMix();
    void slotToggleStemsMixLock();
    void slotImportStemsMix();
    void slotImportStemsMixFile(const QString& stemsMix_file);
    void slotCreateImportStemsMix();
    void slotExportStemsMix();
    // Copy all of the tracks in a stemsMix to a new directory.
    void slotExportTrackFiles();
    void slotAnalyzeStemsMix();

  protected:
    struct IdAndLabel {
        int id;
        QString label;
    };

    virtual void updateChildModel(int selected_id);
    virtual void clearChildModel();
    virtual QString fetchStemsMixLabel(int stemsMixId) = 0;
    virtual void decorateChild(TreeItem* pChild, int stemsMixId) = 0;
    virtual void addToAutoDJ(StemsMixDAO::AutoDJSendLoc loc);

    int stemsMixIdFromIndex(const QModelIndex& index);
    // Get the QModelIndex of a stemsMix based on its id.  Returns QModelIndex()
    // on failure.
    QModelIndex indexFromStemsMixId(int stemsMixId);

    StemsMixDAO& m_stemsMixDao;
    QModelIndex m_lastRightClickedIndex;
    QPointer<WLibrarySidebar> m_pSidebarWidget;

    QAction* m_pCreateStemsMixAction;
    QAction* m_pDeleteStemsMixAction;
    QAction* m_pAddToAutoDJAction;
    QAction* m_pAddToAutoDJTopAction;
    QAction* m_pAddToAutoDJReplaceAction;
    QAction* m_pRenameStemsMixAction;
    QAction* m_pLockStemsMixAction;
    QAction* m_pImportStemsMixAction;
    QAction* m_pCreateImportStemsMixAction;
    QAction* m_pExportStemsMixAction;
    QAction* m_pExportTrackFilesAction;
    QAction* m_pDuplicateStemsMixAction;
    QAction* m_pAnalyzeStemsMixAction;

    StemsMixTableModel* m_pStemsMixTableModel;
    QSet<int> m_stemsMixIdsOfSelectedTrack;

  private slots:
    void slotTrackSelected(TrackId trackId);
    void slotResetSelectedTrack();

  private:
    void initActions();
    virtual QString getRootViewHtml() const = 0;
    void markTreeItem(TreeItem* pTreeItem);

    TrackId m_selectedTrackId;
};
