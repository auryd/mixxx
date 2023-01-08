#pragma once

#include <QIcon>
#include <QModelIndex>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QUrl>
#include <QVariant>

#include "library/autodj/basestemsmixfeature.h"
#include "preferences/usersettings.h"

class TrackCollection;
class TreeItem;
class WLibrarySidebar;

class StemsMixFeature : public BaseStemsMixFeature {
    Q_OBJECT

  public:
    StemsMixFeature(
            Library* pLibrary,
            UserSettingsPointer pConfig);
    ~StemsMixFeature() override = default;

    QVariant title() override;

    bool dropAcceptChild(const QModelIndex& index,
            const QList<QUrl>& urls,
            QObject* pSource) override;
    bool dragMoveAcceptChild(const QModelIndex& index, const QUrl& url) override;

  public slots:
    void onRightClick(const QPoint& globalPos) override;
    void onRightClickChild(const QPoint& globalPos, const QModelIndex& index) override;

  private slots:
    void slotStemsMixTableChanged(int stemsMixId) override;
    void slotStemsMixContentChanged(QSet<int> stemsMixIds) override;
    void slotStemsMixTableRenamed(int stemsMixId, const QString& newName) override;

  protected:
    QString fetchStemsMixLabel(int stemsMixId) override;
    void decorateChild(TreeItem* pChild, int stemsMixId) override;
    QList<IdAndLabel> createStemsMixLabels();
    QModelIndex constructChildModel(int selectedId);

  private:
    QString getRootViewHtml() const override;
};
