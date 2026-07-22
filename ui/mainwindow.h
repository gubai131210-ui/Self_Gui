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
class FlowBarWidget;
class ImagePreviewWidget;
class MeasurementPanel;
class RuntimeMonitorWidget;
class InputSourceDock;
class NodeInspectorDialog;
class CalibrationStore;
class ResourceBrowserDock;
class CommunicationDock;
class VariableDock;
class QUndoStack;
class QUndoGroup;
class QSplitter;
class QLabel;
class QTimer;
class QMenu;
class QAction;
class QActionGroup;
class QDockWidget;
class QTabWidget;
class QToolBar;
namespace Selt {
class ThemeController;
}
#if SELT_HAS_OPENCV
namespace Selt {
class RunController;
class ProjectService;
class InputSourceService;
}
#endif

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
    void startVisionLoop();
    void stopVisionRun();
    void insertVisionDemoFlow();
    void insertVisionWorkflowTemplate(const QString &templateId);
    void clearVisionResults();
    void setThemeLight();
    void setThemeDark();
    void setThemeSystem();
    void resetLayout();
#if SELT_HAS_OPENCV
    void onVisionOutputNodeChanged(const QString &nodeId);
    void onResultRoiChanged(const RoiRect &roi);
    void onTeachTemplateRequested();
    void onRunFinished(bool ok, const VisionContext &context);
    void onRunProgress(const QString &nodeId, ModuleStatus status);
    void onRunBusyChanged(bool busy);
    void onCreateFlowRequested();
    void onRenameFlowRequested();
    void onRemoveFlowRequested();
    void onFlowChosen(const QString &flowId);
    void refreshFlowBar();
    void onActiveFlowChanged(const QString &flowId);
    void onUndoStackChanged(QUndoStack *stack);
    void openCalibrationDialog();
    void runPublishValidation();
#endif

private:
    void setupUiExtra();
    void setupActions();
    void setupMenus();
    void setupToolBar();
    void setupDocks();
    void setupConnections();
    void applyActionIcons();
    void updateRunStatus(const QString &text, int statusKind);
    void openNodeInspector(const QString &nodeId);
    void saveWindowLayout() const;
    void restoreWindowLayout();
    void captureDefaultLayout();
    void applyDefaultDockVisibility();
    void syncDockToggleActions();
#if SELT_HAS_OPENCV
    void setVisionWorkbenchOrientation(Qt::Orientation orientation);
    void toggleVisionWorkbenchOrientation();
    void resetVisionWorkbenchLayout();
    void saveVisionWorkbenchLayout() const;
    void restoreVisionWorkbenchLayout();
#endif
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
    void updateNodeRunStatus(const QString &nodeId, ModuleStatus status);
    void clearNodeRunStatuses();
    void updateRunActionsEnabled();
    void syncSelectedModuleStatus(const QString &nodeId);
    void captureActiveViewport();
    void restoreViewportForFlow(const QString &flowId);
    void showResultsDock(bool visible);
    void syncRunVariableScopes();
    Document *document() const;
    QUndoStack *undoStack() const;
#endif

    Ui::MainWindow *ui{nullptr};
    Document *m_document{nullptr};
    QUndoStack *m_undoStack{nullptr};
    QUndoGroup *m_undoGroup{nullptr};
    CanvasScene *m_scene{nullptr};
    CanvasView *m_view{nullptr};
    NodePalette *m_palette{nullptr};
    PropertyPanel *m_propertyPanel{nullptr};
    FlowBarWidget *m_flowBar{nullptr};
    ImagePreviewWidget *m_originalPreview{nullptr};
    ImagePreviewWidget *m_resultPreview{nullptr};
    MeasurementPanel *m_measurementPanel{nullptr};
    RuntimeMonitorWidget *m_runtimeMonitor{nullptr};
    InputSourceDock *m_inputSourceDock{nullptr};
    ResourceBrowserDock *m_resourceBrowser{nullptr};
    CommunicationDock *m_communicationDock{nullptr};
    VariableDock *m_variableDock{nullptr};
    NodeInspectorDialog *m_nodeInspectorDialog{nullptr};
    QLabel *m_zoomLabel{nullptr};
    QLabel *m_posLabel{nullptr};
    QLabel *m_runStatusLabel{nullptr};
    QString m_currentFile;
    QMenu *m_recentMenu{nullptr};
    QTimer *m_autoSaveTimer{nullptr};
    QToolBar *m_mainToolBar{nullptr};
    QDockWidget *m_leftDock{nullptr};
    QDockWidget *m_rightDock{nullptr};
    QDockWidget *m_bottomDock{nullptr};
    QDockWidget *m_visionDock{nullptr};
    QDockWidget *m_resourceDock{nullptr};
    QDockWidget *m_communicationDockWidget{nullptr};
    QDockWidget *m_variableDockWidget{nullptr};
    QTabWidget *m_inspectorTabs{nullptr};
    QTabWidget *m_previewTabs{nullptr};
    QTabWidget *m_resultTabs{nullptr};
    QSplitter *m_visionSplitter{nullptr};
    QLabel *m_helpLabel{nullptr};
    QByteArray m_defaultGeometry;
    QByteArray m_defaultState;

    QAction *m_newAction{nullptr};
    QAction *m_openAction{nullptr};
    QAction *m_saveAction{nullptr};
    QAction *m_saveAsAction{nullptr};
    QAction *m_undoAction{nullptr};
    QAction *m_redoAction{nullptr};
    QAction *m_selectAllAction{nullptr};
    QAction *m_deleteAction{nullptr};
    QAction *m_copyAction{nullptr};
    QAction *m_pasteAction{nullptr};
    QAction *m_alignLeftAction{nullptr};
    QAction *m_alignHCenterAction{nullptr};
    QAction *m_alignRightAction{nullptr};
    QAction *m_alignTopAction{nullptr};
    QAction *m_alignVCenterAction{nullptr};
    QAction *m_alignBottomAction{nullptr};
    QAction *m_distributeHAction{nullptr};
    QAction *m_distributeVAction{nullptr};
    QAction *m_autoLayoutAction{nullptr};
    QAction *m_breakpointAction{nullptr};
    QAction *m_minimapAction{nullptr};
    QAction *m_publishCheckAction{nullptr};
    QAction *m_zoomInAction{nullptr};
    QAction *m_zoomOutAction{nullptr};
    QAction *m_resetZoomAction{nullptr};
    QAction *m_fitAction{nullptr};
    QAction *m_snapAction{nullptr};
    QAction *m_gridAction{nullptr};
    QAction *m_runAction{nullptr};
    QAction *m_loopAction{nullptr};
    QAction *m_stopAction{nullptr};
    QAction *m_demoAction{nullptr};
    QAction *m_clearResultAction{nullptr};
    QAction *m_themeLightAction{nullptr};
    QAction *m_themeDarkAction{nullptr};
    QAction *m_themeSystemAction{nullptr};
    QAction *m_resetLayoutAction{nullptr};
    QAction *m_toggleVisionOrientationAction{nullptr};
    QAction *m_resetVisionWorkbenchAction{nullptr};
    QAction *m_toggleNodesDockAction{nullptr};
    QAction *m_toggleInspectorDockAction{nullptr};
    QAction *m_toggleResultsDockAction{nullptr};
    QAction *m_toggleToolBarAction{nullptr};
    QActionGroup *m_themeGroup{nullptr};

    Selt::ThemeController *m_themeController{nullptr};
#if SELT_HAS_OPENCV
    VisionContext m_lastVisionContext;
    bool m_hasVisionResult{false};
    Selt::RunController *m_runController{nullptr};
    Selt::ProjectService *m_projectService{nullptr};
    Selt::InputSourceService *m_inputSource{nullptr};
    CalibrationStore *m_calibrationStore{nullptr};
#endif
};

#endif // MAINWINDOW_H
