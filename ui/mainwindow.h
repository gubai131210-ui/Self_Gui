#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

#ifndef SELT_HAS_OPENCV
#define SELT_HAS_OPENCV 0
#endif

#if SELT_HAS_OPENCV
#include "vision/model/roi.h"
#include "vision/model/visioncontext.h"
#endif

class QCloseEvent;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class Document;
class CanvasScene;
class CanvasView;
class NodePalette;
class PropertyPanel;
class ImagePreviewWidget;
class MeasurementPanel;
class QUndoStack;
class QLabel;
class QTimer;
class QSettings;
class QMenu;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void newDocument();
    void openDocument();
    void saveDocument();
    void saveDocumentAs();
    void updateWindowTitle();
    void updateStatusZoom(qreal factor);
    void updateStatusPos(const QPointF &pos);
    void onSelectionNodeChanged(const QString &nodeId);
    void onPaletteActivated(const QString &type);
    void toggleSnap(bool enabled);
    void toggleGrid(bool enabled);
    void openRecentFile();
    void runVisionPipeline();
    void insertVisionDemoFlow();
    void clearVisionResults();
#if SELT_HAS_OPENCV
    void onVisionOutputNodeChanged(const QString &nodeId);
    void onResultRoiChanged(const RoiRect &roi);
#endif

private:
    void setupUiExtra();
    void setupActions();
    void setupMenus();
    void setupToolBar();
    void setupDocks();
    void setupConnections();
    bool maybeSave();
    bool saveToPath(const QString &path);
    bool loadFromPath(const QString &path);
    void setCurrentFile(const QString &path);
    void addRecentFile(const QString &path);
    void updateRecentFilesMenu();
    void autoSave();
#if SELT_HAS_OPENCV
    void applyVisionContext(const VisionContext &context, bool focusFailedNode);
    void refreshResultPreview(const QString &nodeId);
    void updateNodeRunStatuses(const VisionContext &context);
    void clearNodeRunStatuses();
    void syncSelectedModuleStatus(const QString &nodeId);
#endif

    Ui::MainWindow *ui{nullptr};
    Document *m_document{nullptr};
    QUndoStack *m_undoStack{nullptr};
    CanvasScene *m_scene{nullptr};
    CanvasView *m_view{nullptr};
    NodePalette *m_palette{nullptr};
    PropertyPanel *m_propertyPanel{nullptr};
    ImagePreviewWidget *m_originalPreview{nullptr};
    ImagePreviewWidget *m_resultPreview{nullptr};
    MeasurementPanel *m_measurementPanel{nullptr};
    QLabel *m_zoomLabel{nullptr};
    QLabel *m_posLabel{nullptr};
    QString m_currentFile;
    QMenu *m_recentMenu{nullptr};
    QTimer *m_autoSaveTimer{nullptr};
#if SELT_HAS_OPENCV
    VisionContext m_lastVisionContext;
    bool m_hasVisionResult{false};
#endif
};

#endif // MAINWINDOW_H
