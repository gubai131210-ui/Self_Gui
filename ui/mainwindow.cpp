#include "ui/mainwindow.h"
#include "ui_mainwindow.h"

#include "core/model/document.h"
#include "core/serialization/documentserializer.h"
#include "graphics/canvas/canvasscene.h"
#include "graphics/canvas/canvasview.h"
#include "ui/flowbarwidget.h"
#include "ui/nodepalette.h"
#include "ui/nodeinspectordialog.h"
#include "ui/propertypanel.h"
#include "ui/theme/iconprovider.h"
#include "ui/theme/themecontroller.h"
#include "ui/theme/uistyle.h"
#include "ui/theme/operatorpreferences.h"
#include "app/nodebuilder.h"
#include "core/command/documentcommands.h"
#include "foundation/seltversion.h"
#if SELT_HAS_OPENCV
#include "ui/calibrationdialog.h"
#include "ui/imagepreviewwidget.h"
#include "ui/inputsourcedock.h"
#include "ui/measurementpanel.h"
#include "ui/resourcebrowserdock.h"
#include "ui/communicationdock.h"
#include "ui/variabledock.h"
#include "ui/runtimemonitorwidget.h"
#include "plugin_host/pluginhost.h"
#include "communication/clients.h"
#include "vision/model/calibrationstore.h"
#include "vision/project/projectservice.h"
#include "vision/runtime/inputsourceservice.h"
#include "vision/runtime/resultsink.h"
#include "vision/workflow/workflowtemplates.h"
#include "vision/runtime/runcontroller.h"
#include "vision/runtime/runtimediagnostic.h"
#include "vision/validation/graphvalidator.h"
#include "vision/execution/visionpipeline.h"
#include "vision/model/modulevisualstyle.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"
#include "vision/services/templateteachservice.h"
#include "vision/storage/projectresourcestore.h"
#endif

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QUndoGroup>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#if SELT_HAS_OPENCV
#include "graphics/items/nodegraphicsitem.h"
#include "vision/model/modulestatus.h"
#include "vision/registry/visionnoderegistry.h"
#endif

namespace {
constexpr int kWindowLayoutVersion = Selt::UiStyle::windowLayoutVersion;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    resize(1280, 800);
    setDockNestingEnabled(false);
    setDockOptions(QMainWindow::AnimatedDocks
                   | QMainWindow::AllowTabbedDocks
                   | QMainWindow::GroupedDragging);
    setWindowTitle(Selt::VersionInfo::productName());

    m_themeController = new Selt::ThemeController(this);
    m_themeController->loadFromSettings();
    if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance()))
        m_themeController->apply(app);

#if SELT_HAS_OPENCV
    m_projectService = new Selt::ProjectService(this);
    m_document = m_projectService->document();
    m_undoGroup = m_projectService->undoGroup();
    m_undoStack = m_projectService->activeUndoStack();
    m_currentFile = m_projectService->currentFilePath();
#else
    m_document = new Document(this);
    m_undoStack = new QUndoStack(this);
#endif

    m_scene = new CanvasScene(m_document, m_undoStack, this);
    m_view = new CanvasView(m_scene, this);

    setupUiExtra();
    setupActions();
    setupMenus();
    setupToolBar();
    setupDocks();
    setupConnections();
    applyActionIcons();

#if SELT_HAS_OPENCV
    if (m_palette)
        m_palette->reloadCatalog();
    {
        const auto &host = Selt::PluginHost::instance();
        QStringList pluginDiags = host.diagnostics();
        pluginDiags.append(host.deploymentRequirements());
        pluginDiags.append(Selt::Communication::capabilitySummary());
        pluginDiags.append(Selt::Communication::runtimeDeploymentHints());
        if (m_runtimeMonitor && !pluginDiags.isEmpty())
            m_runtimeMonitor->showDiagnostics(pluginDiags);
        if (!pluginDiags.isEmpty())
            statusBar()->showMessage(pluginDiags.last(), 5000);
    }
#endif

    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(60 * 1000);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSave);
    m_autoSaveTimer->start();

#if SELT_HAS_OPENCV
    refreshFlowBar();
#endif
    captureDefaultLayout();
    restoreWindowLayout();
    updateWindowTitle();
    updateRecentFilesMenu();
    updateRunStatus(QStringLiteral("空闲"), 0);
}

MainWindow::~MainWindow()
{
#if SELT_HAS_OPENCV
    delete m_calibrationStore;
    m_calibrationStore = nullptr;
#endif
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!maybeSave()) {
        event->ignore();
        return;
    }
    saveWindowLayout();
    event->accept();
}

void MainWindow::setupUiExtra()
{
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
#if SELT_HAS_OPENCV
    m_flowBar = new FlowBarWidget(central);
    m_flowBar->setObjectName(QStringLiteral("FlowBarWidget"));
    m_flowBar->setMaximumHeight(Selt::UiStyle::flowBarHeight + 8);
    layout->addWidget(m_flowBar);
#endif
    layout->addWidget(m_view, 1);
    m_view->setMinimumWidth(Selt::UiStyle::centralMinWidth);
    setCentralWidget(central);

    m_runStatusLabel = new QLabel(QStringLiteral("空闲"), this);
    m_runStatusLabel->setObjectName(QStringLiteral("RunStatusLabel"));
    m_zoomLabel = new QLabel(QStringLiteral("缩放 100%"), this);
    m_posLabel = new QLabel(QStringLiteral("坐标 (0, 0)"), this);
    statusBar()->addWidget(m_runStatusLabel);
    statusBar()->addPermanentWidget(m_posLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);
}

void MainWindow::setupActions()
{
    m_newAction = new QAction(QStringLiteral("新建"), this);
    m_newAction->setObjectName(QStringLiteral("actionNew"));
    m_newAction->setShortcut(QKeySequence::New);
    m_newAction->setToolTip(QStringLiteral("新建项目"));
    connect(m_newAction, &QAction::triggered, this, &MainWindow::newDocument);

    m_openAction = new QAction(QStringLiteral("打开..."), this);
    m_openAction->setObjectName(QStringLiteral("actionOpen"));
    m_openAction->setShortcut(QKeySequence::Open);
    m_openAction->setToolTip(QStringLiteral("打开工程"));
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openDocument);

    m_saveAction = new QAction(QStringLiteral("保存"), this);
    m_saveAction->setObjectName(QStringLiteral("actionSave"));
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setToolTip(QStringLiteral("保存当前项目"));
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::saveDocument);

    m_saveAsAction = new QAction(QStringLiteral("另存为..."), this);
    m_saveAsAction->setObjectName(QStringLiteral("actionSaveAs"));
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveDocumentAs);

#if SELT_HAS_OPENCV
    m_undoAction = m_undoGroup->createUndoAction(this, QStringLiteral("撤销"));
    m_redoAction = m_undoGroup->createRedoAction(this, QStringLiteral("重做"));
#else
    m_undoAction = m_undoStack->createUndoAction(this, QStringLiteral("撤销"));
    m_redoAction = m_undoStack->createRedoAction(this, QStringLiteral("重做"));
#endif
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_redoAction->setShortcut(QKeySequence::Redo);

    m_selectAllAction = new QAction(QStringLiteral("全选"), this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(m_selectAllAction, &QAction::triggered, m_scene, &CanvasScene::selectAll);

    m_deleteAction = new QAction(QStringLiteral("删除"), this);
    m_deleteAction->setShortcut(QKeySequence::Delete);
    connect(m_deleteAction, &QAction::triggered, m_scene, &CanvasScene::deleteSelection);

    m_copyAction = new QAction(QStringLiteral("复制"), this);
    m_copyAction->setShortcut(QKeySequence::Copy);
    connect(m_copyAction, &QAction::triggered, m_scene, &CanvasScene::copySelection);

    m_pasteAction = new QAction(QStringLiteral("粘贴"), this);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    connect(m_pasteAction, &QAction::triggered, this, [this]() {
        m_scene->pasteClipboard(m_view->mapToScene(m_view->viewport()->rect().center()));
    });

    m_alignLeftAction = new QAction(QStringLiteral("左对齐"), this);
    connect(m_alignLeftAction, &QAction::triggered, this,
            [this]() { m_scene->alignSelection(Qt::AlignLeft); });
    m_alignHCenterAction = new QAction(QStringLiteral("水平居中"), this);
    connect(m_alignHCenterAction, &QAction::triggered, this,
            [this]() { m_scene->alignSelection(Qt::AlignHCenter); });
    m_alignRightAction = new QAction(QStringLiteral("右对齐"), this);
    connect(m_alignRightAction, &QAction::triggered, this,
            [this]() { m_scene->alignSelection(Qt::AlignRight); });
    m_alignTopAction = new QAction(QStringLiteral("顶对齐"), this);
    connect(m_alignTopAction, &QAction::triggered, this,
            [this]() { m_scene->alignSelection(Qt::AlignTop); });
    m_alignVCenterAction = new QAction(QStringLiteral("垂直居中"), this);
    connect(m_alignVCenterAction, &QAction::triggered, this,
            [this]() { m_scene->alignSelection(Qt::AlignVCenter); });
    m_alignBottomAction = new QAction(QStringLiteral("底对齐"), this);
    connect(m_alignBottomAction, &QAction::triggered, this,
            [this]() { m_scene->alignSelection(Qt::AlignBottom); });
    m_distributeHAction = new QAction(QStringLiteral("水平分布"), this);
    connect(m_distributeHAction, &QAction::triggered, this,
            [this]() { m_scene->distributeSelection(Qt::Horizontal); });
    m_distributeVAction = new QAction(QStringLiteral("垂直分布"), this);
    connect(m_distributeVAction, &QAction::triggered, this,
            [this]() { m_scene->distributeSelection(Qt::Vertical); });
    m_autoLayoutAction = new QAction(QStringLiteral("自动布局"), this);
    m_autoLayoutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
    connect(m_autoLayoutAction, &QAction::triggered, m_scene, &CanvasScene::autoLayoutSelection);
    m_breakpointAction = new QAction(QStringLiteral("切换断点"), this);
    m_breakpointAction->setShortcut(QKeySequence(Qt::Key_F9));
    connect(m_breakpointAction, &QAction::triggered, m_scene, &CanvasScene::toggleBreakpointOnSelection);
    m_minimapAction = new QAction(QStringLiteral("导航缩略图"), this);
    m_minimapAction->setCheckable(true);
    m_minimapAction->setChecked(true);
    connect(m_minimapAction, &QAction::toggled, m_view, &CanvasView::setMinimapVisible);
#if SELT_HAS_OPENCV
    m_publishCheckAction = new QAction(QStringLiteral("发布前校验…"), this);
    connect(m_publishCheckAction, &QAction::triggered, this, &MainWindow::runPublishValidation);
#endif

    m_zoomInAction = new QAction(QStringLiteral("放大"), this);
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, m_view, &CanvasView::zoomIn);

    m_zoomOutAction = new QAction(QStringLiteral("缩小"), this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, m_view, &CanvasView::zoomOut);

    m_resetZoomAction = new QAction(QStringLiteral("实际大小"), this);
    connect(m_resetZoomAction, &QAction::triggered, m_view, &CanvasView::resetZoom);

    m_fitAction = new QAction(QStringLiteral("适应全部"), this);
    m_fitAction->setToolTip(QStringLiteral("适应画布内容"));
    connect(m_fitAction, &QAction::triggered, m_view, &CanvasView::fitAll);

    m_snapAction = new QAction(QStringLiteral("网格吸附"), this);
    m_snapAction->setCheckable(true);
    m_snapAction->setChecked(true);
    connect(m_snapAction, &QAction::toggled, this, &MainWindow::toggleSnap);

    m_gridAction = new QAction(QStringLiteral("显示网格"), this);
    m_gridAction->setCheckable(true);
    m_gridAction->setChecked(true);
    connect(m_gridAction, &QAction::toggled, this, &MainWindow::toggleGrid);

    m_runAction = new QAction(QStringLiteral("单次执行"), this);
    m_runAction->setShortcut(QKeySequence(Qt::Key_F5));
    m_runAction->setToolTip(QStringLiteral("单次执行活动流程 (F5)"));
    connect(m_runAction, &QAction::triggered, this, &MainWindow::runVisionPipeline);

    m_loopAction = new QAction(QStringLiteral("连续运行"), this);
    m_loopAction->setToolTip(QStringLiteral("连续运行活动流程"));
    connect(m_loopAction, &QAction::triggered, this, &MainWindow::startVisionLoop);

    m_stopAction = new QAction(QStringLiteral("停止"), this);
    m_stopAction->setShortcut(QKeySequence(Qt::Key_F8));
    m_stopAction->setToolTip(QStringLiteral("停止运行 (F8)"));
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::stopVisionRun);

    m_demoAction = new QAction(QStringLiteral("插入示例流程"), this);
    connect(m_demoAction, &QAction::triggered, this, [this]() {
        insertVisionWorkflowTemplate(QStringLiteral("measure_basic"));
    });

    m_clearResultAction = new QAction(QStringLiteral("清空结果"), this);
    connect(m_clearResultAction, &QAction::triggered, this, &MainWindow::clearVisionResults);

    m_themeGroup = new QActionGroup(this);
    m_themeGroup->setExclusive(true);
    m_themeLightAction = m_themeGroup->addAction(QStringLiteral("浅色主题"));
    m_themeDarkAction = m_themeGroup->addAction(QStringLiteral("深色主题"));
    m_themeSystemAction = m_themeGroup->addAction(QStringLiteral("跟随系统"));
    m_themeLightAction->setCheckable(true);
    m_themeDarkAction->setCheckable(true);
    m_themeSystemAction->setCheckable(true);
    connect(m_themeLightAction, &QAction::triggered, this, &MainWindow::setThemeLight);
    connect(m_themeDarkAction, &QAction::triggered, this, &MainWindow::setThemeDark);
    connect(m_themeSystemAction, &QAction::triggered, this, &MainWindow::setThemeSystem);
    switch (m_themeController->mode()) {
    case Selt::ThemeMode::Light: m_themeLightAction->setChecked(true); break;
    case Selt::ThemeMode::Dark: m_themeDarkAction->setChecked(true); break;
    case Selt::ThemeMode::System: m_themeSystemAction->setChecked(true); break;
    }

    m_resetLayoutAction = new QAction(QStringLiteral("恢复默认布局"), this);
    connect(m_resetLayoutAction, &QAction::triggered, this, &MainWindow::resetLayout);
#if SELT_HAS_OPENCV
    m_toggleVisionOrientationAction = new QAction(QStringLiteral("图像/结果左右布局"), this);
    m_toggleVisionOrientationAction->setCheckable(true);
    connect(m_toggleVisionOrientationAction, &QAction::triggered, this,
            &MainWindow::toggleVisionWorkbenchOrientation);
    m_resetVisionWorkbenchAction = new QAction(QStringLiteral("恢复图像与结果布局"), this);
    connect(m_resetVisionWorkbenchAction, &QAction::triggered, this,
            &MainWindow::resetVisionWorkbenchLayout);
#endif

    m_toggleNodesDockAction = new QAction(QStringLiteral("工具箱"), this);
    m_toggleNodesDockAction->setCheckable(true);
    m_toggleInspectorDockAction = new QAction(QStringLiteral("检查器"), this);
    m_toggleInspectorDockAction->setCheckable(true);
    m_toggleResultsDockAction = new QAction(QStringLiteral("图像与结果"), this);
    m_toggleResultsDockAction->setCheckable(true);
    m_toggleToolBarAction = new QAction(QStringLiteral("主工具栏"), this);
    m_toggleToolBarAction->setCheckable(true);
    m_toggleToolBarAction->setChecked(true);
}

void MainWindow::applyActionIcons()
{
    using Selt::IconId;
    using Selt::IconProvider;
    using Selt::UiStyle;
    const int s = UiStyle::toolIconSize;
    m_newAction->setIcon(IconProvider::icon(IconId::New, s));
    m_openAction->setIcon(IconProvider::icon(IconId::Open, s));
    m_saveAction->setIcon(IconProvider::icon(IconId::Save, s));
    m_saveAsAction->setIcon(IconProvider::icon(IconId::SaveAs, s));
    m_undoAction->setIcon(IconProvider::icon(IconId::Undo, s));
    m_redoAction->setIcon(IconProvider::icon(IconId::Redo, s));
    m_copyAction->setIcon(IconProvider::icon(IconId::Copy, s));
    m_pasteAction->setIcon(IconProvider::icon(IconId::Paste, s));
    m_deleteAction->setIcon(IconProvider::icon(IconId::Delete, s));
    m_zoomInAction->setIcon(IconProvider::icon(IconId::ZoomIn, s));
    m_zoomOutAction->setIcon(IconProvider::icon(IconId::ZoomOut, s));
    m_fitAction->setIcon(IconProvider::icon(IconId::Fit, s));
    m_snapAction->setIcon(IconProvider::icon(IconId::Snap, s));
    m_gridAction->setIcon(IconProvider::icon(IconId::Grid, s));
    m_runAction->setIcon(IconProvider::icon(IconId::Run, s));
    m_loopAction->setIcon(IconProvider::icon(IconId::Loop, s));
    m_stopAction->setIcon(IconProvider::icon(IconId::Stop, s));
    m_clearResultAction->setIcon(IconProvider::icon(IconId::ClearResult, s));
    m_resetLayoutAction->setIcon(IconProvider::icon(IconId::LayoutReset, s));
    m_toggleNodesDockAction->setIcon(IconProvider::icon(IconId::Nodes, s));
    m_toggleInspectorDockAction->setIcon(IconProvider::icon(IconId::Properties, s));
    m_toggleResultsDockAction->setIcon(IconProvider::icon(IconId::Results, s));
}

void MainWindow::setupMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    fileMenu->addAction(m_newAction);
    fileMenu->addAction(m_openAction);
    m_recentMenu = fileMenu->addMenu(QStringLiteral("最近打开"));
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("退出"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *editMenu = menuBar()->addMenu(QStringLiteral("编辑(&E)"));
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
    editMenu->addSeparator();
    editMenu->addAction(m_selectAllAction);
    editMenu->addAction(m_deleteAction);
    editMenu->addAction(m_copyAction);
    editMenu->addAction(m_pasteAction);
    QMenu *alignMenu = editMenu->addMenu(QStringLiteral("对齐"));
    alignMenu->addAction(m_alignLeftAction);
    alignMenu->addAction(m_alignHCenterAction);
    alignMenu->addAction(m_alignRightAction);
    alignMenu->addAction(m_alignTopAction);
    alignMenu->addAction(m_alignVCenterAction);
    alignMenu->addAction(m_alignBottomAction);
    editMenu->addAction(m_distributeHAction);
    editMenu->addAction(m_distributeVAction);
    editMenu->addAction(m_autoLayoutAction);
    editMenu->addAction(m_breakpointAction);

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_resetZoomAction);
    viewMenu->addAction(m_fitAction);
    viewMenu->addAction(m_minimapAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_snapAction);
    viewMenu->addAction(m_gridAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_toggleNodesDockAction);
    viewMenu->addAction(m_toggleInspectorDockAction);
    viewMenu->addAction(m_toggleResultsDockAction);
#if SELT_HAS_OPENCV
    if (m_toggleVisionOrientationAction)
        viewMenu->addAction(m_toggleVisionOrientationAction);
    if (m_resetVisionWorkbenchAction)
        viewMenu->addAction(m_resetVisionWorkbenchAction);
#endif
    viewMenu->addAction(m_toggleToolBarAction);
    viewMenu->addSeparator();
    QMenu *themeMenu = viewMenu->addMenu(Selt::IconProvider::icon(Selt::IconId::Theme), QStringLiteral("主题"));
    themeMenu->addAction(m_themeLightAction);
    themeMenu->addAction(m_themeDarkAction);
    themeMenu->addAction(m_themeSystemAction);
    viewMenu->addAction(m_resetLayoutAction);

    QMenu *runMenu = menuBar()->addMenu(QStringLiteral("执行(&R)"));
    runMenu->addAction(m_runAction);
    runMenu->addAction(m_loopAction);
    runMenu->addAction(m_stopAction);
    runMenu->addSeparator();
    runMenu->addAction(m_demoAction);
#if SELT_HAS_OPENCV
    QMenu *templateMenu = runMenu->addMenu(QStringLiteral("插入流程模板"));
    for (const QString &id : Selt::WorkflowTemplates::templateIds()) {
        const Selt::WorkflowTemplateSpec preview = Selt::WorkflowTemplates::build(id);
        auto *act = templateMenu->addAction(preview.displayName);
        act->setToolTip(preview.description);
        connect(act, &QAction::triggered, this, [this, id]() {
            insertVisionWorkflowTemplate(id);
        });
    }
#endif
    runMenu->addAction(m_clearResultAction);
#if SELT_HAS_OPENCV
    runMenu->addSeparator();
    runMenu->addAction(QStringLiteral("标定管理…"), this, &MainWindow::openCalibrationDialog);
    if (m_publishCheckAction)
        runMenu->addAction(m_publishCheckAction);
#endif

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    helpMenu->addAction(QStringLiteral("关于"), this, [this]() {
        QMessageBox::about(this, QStringLiteral("关于"),
                           QStringLiteral("%1 %2\n"
                                          "视觉模块化流程编辑器（OpenCV）\n"
                                          "架构代号: %3")
                               .arg(Selt::VersionInfo::productName(),
                                    Selt::VersionInfo::productVersionString(),
                                    Selt::VersionInfo::architectureCodename()));
    });
}

void MainWindow::setupToolBar()
{
    m_mainToolBar = addToolBar(QStringLiteral("主工具栏"));
    m_mainToolBar->setObjectName(QStringLiteral("MainToolBar"));
    m_mainToolBar->setMovable(false);
    m_mainToolBar->setFloatable(false);
    m_mainToolBar->setAllowedAreas(Qt::TopToolBarArea);
    m_mainToolBar->setIconSize(QSize(Selt::UiStyle::toolIconSize, Selt::UiStyle::toolIconSize));
    m_mainToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    connect(m_toggleToolBarAction, &QAction::toggled,
            m_mainToolBar, &QToolBar::setVisible);

    m_mainToolBar->addAction(m_newAction);
    m_mainToolBar->addAction(m_openAction);
    m_mainToolBar->addAction(m_saveAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_undoAction);
    m_mainToolBar->addAction(m_redoAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_fitAction);
    m_mainToolBar->addAction(m_gridAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_runAction);
    m_mainToolBar->addAction(m_loopAction);
    m_mainToolBar->addAction(m_stopAction);
}

void MainWindow::setupDocks()
{
    m_palette = new NodePalette(this);
    m_leftDock = new QDockWidget(QStringLiteral("工具箱"), this);
    m_leftDock->setObjectName(QStringLiteral("NodePaletteDock"));
    m_leftDock->setAllowedAreas(Qt::LeftDockWidgetArea);
    m_leftDock->setFeatures(QDockWidget::DockWidgetMovable
                             | QDockWidget::DockWidgetFloatable
                             | QDockWidget::DockWidgetClosable);
    m_leftDock->setMinimumWidth(Selt::UiStyle::leftDockMinWidth);
    m_leftDock->setWidget(m_palette);
    addDockWidget(Qt::LeftDockWidgetArea, m_leftDock);
    resizeDocks({m_leftDock}, {Selt::UiStyle::leftDockDefaultWidth}, Qt::Horizontal);

    m_propertyPanel = new PropertyPanel(this);
    m_propertyPanel->setDocument(m_document);
    m_propertyPanel->setUndoStack(m_undoStack);

#if SELT_HAS_OPENCV
    m_inputSource = new Selt::InputSourceService(this);
    m_inputSourceDock = new InputSourceDock(m_inputSource, this);
#endif

    m_inspectorTabs = new QTabWidget(this);
    m_inspectorTabs->addTab(m_propertyPanel, Selt::IconProvider::icon(Selt::IconId::Properties), QStringLiteral("属性"));
#if SELT_HAS_OPENCV
    m_inspectorTabs->addTab(m_inputSourceDock, Selt::IconProvider::icon(Selt::IconId::Settings), QStringLiteral("输入源"));
    auto *statusPage = new QWidget(m_inspectorTabs);
    auto *statusLayout = new QVBoxLayout(statusPage);
    auto *hint = new QLabel(QStringLiteral(
        "选中节点后可在属性页查看参数与绑定态。\n"
        "运行状态也会显示在状态栏。\n"
        "输入源页可打开相机/目录回放。\n"
        "视图菜单可打开「项目资源」与导航缩略图。\n"
        "F9 切换断点；执行菜单可做发布前校验。"), statusPage);
    hint->setWordWrap(true);
    statusLayout->addWidget(hint);
    auto *pluginTitle = new QLabel(QStringLiteral("插件宿主"), statusPage);
    pluginTitle->setStyleSheet(QStringLiteral("font-weight: bold;"));
    statusLayout->addWidget(pluginTitle);
    auto *pluginInfo = new QLabel(statusPage);
    pluginInfo->setWordWrap(true);
    pluginInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    {
        const auto &host = Selt::PluginHost::instance();
        QStringList lines;
        lines.append(QStringLiteral("已加载插件: %1").arg(host.loadedPlugins().size()));
        for (const auto &meta : host.loadedPlugins()) {
            lines.append(QStringLiteral("- %1 (%2)%3")
                             .arg(meta.pluginId.isEmpty() ? meta.displayName : meta.pluginId,
                                  meta.pluginVersion,
                                  meta.loaded ? QString() : QStringLiteral(" [失败]")));
        }
        if (host.loadedPlugins().isEmpty())
            lines.append(QStringLiteral("（当前无外部插件；内置算子使用统一 ModuleUiSchema）"));
        const QStringList failDiags = host.diagnostics();
        if (!failDiags.isEmpty()) {
            lines.append(QStringLiteral("加载诊断:"));
            lines.append(failDiags);
        }
        const QStringList deploy = host.deploymentRequirements();
        if (!deploy.isEmpty()) {
            lines.append(QStringLiteral("部署要求:"));
            lines.append(deploy);
        }
        pluginInfo->setText(lines.join(QLatin1Char('\n')));
    }
    statusLayout->addWidget(pluginInfo);
    statusLayout->addStretch(1);
    m_inspectorTabs->addTab(statusPage, Selt::IconProvider::icon(Selt::IconId::Settings), QStringLiteral("说明"));
#endif

    m_rightDock = new QDockWidget(QStringLiteral("检查器"), this);
    m_rightDock->setObjectName(QStringLiteral("InspectorDock"));
    m_rightDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    m_rightDock->setFeatures(QDockWidget::DockWidgetMovable
                             | QDockWidget::DockWidgetFloatable
                             | QDockWidget::DockWidgetClosable);
    m_rightDock->setMinimumWidth(Selt::UiStyle::leftDockMinWidth);
    m_rightDock->setWidget(m_inspectorTabs);
    addDockWidget(Qt::RightDockWidgetArea, m_rightDock);
    resizeDocks({m_rightDock}, {Selt::UiStyle::inspectorDockDefaultWidth}, Qt::Horizontal);
    m_rightDock->hide();

#if SELT_HAS_OPENCV
    m_originalPreview = new ImagePreviewWidget(this);
    m_resultPreview = new ImagePreviewWidget(this);
    m_resultPreview->setRoiEditEnabled(true);
    m_originalPreview->setRoiEditEnabled(false);
    m_previewTabs = new QTabWidget(this);
    m_previewTabs->addTab(m_originalPreview, QStringLiteral("编辑"));
    m_previewTabs->addTab(m_resultPreview, QStringLiteral("结果"));

    m_measurementPanel = new MeasurementPanel(this);
    m_runtimeMonitor = new RuntimeMonitorWidget(this);
    m_resultTabs = new QTabWidget(this);
    m_resultTabs->setObjectName(QStringLiteral("VisionResultTabs"));
    m_resultTabs->setElideMode(Qt::ElideRight);
    if (QTabBar *resultTabBar = m_resultTabs->tabBar())
        resultTabBar->setUsesScrollButtons(true);
    m_resultTabs->addTab(m_measurementPanel, QStringLiteral("结果清单"));
    m_resultTabs->addTab(m_runtimeMonitor, QStringLiteral("运行监视"));
    auto *helpPage = new QWidget(m_resultTabs);
    auto *helpLayout = new QVBoxLayout(helpPage);
    m_helpLabel = new QLabel(QStringLiteral("选中画布节点后，此处显示模块说明。"), helpPage);
    m_helpLabel->setWordWrap(true);
    m_helpLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_helpLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    helpLayout->addWidget(m_helpLabel);
    helpLayout->addStretch(1);
    m_resultTabs->addTab(helpPage, QStringLiteral("帮助"));

    auto *visionRoot = new QWidget(this);
    visionRoot->setObjectName(QStringLiteral("VisionWorkbench"));
    auto *visionLayout = new QVBoxLayout(visionRoot);
    visionLayout->setContentsMargins(Selt::UiStyle::panelMargin,
                                     Selt::UiStyle::panelMargin,
                                     Selt::UiStyle::panelMargin,
                                     Selt::UiStyle::panelMargin);
    visionLayout->setSpacing(Selt::UiStyle::compactSpacing);
    m_visionSplitter = new QSplitter(Qt::Vertical, visionRoot);
    m_visionSplitter->setObjectName(QStringLiteral("VisionWorkbenchSplitter"));
    m_visionSplitter->setHandleWidth(Selt::UiStyle::splitterHandleWidth);
    m_visionSplitter->addWidget(m_previewTabs);
    m_visionSplitter->addWidget(m_resultTabs);
    m_visionSplitter->setStretchFactor(0, 3);
    m_visionSplitter->setStretchFactor(1, 2);
    m_visionSplitter->setSizes({Selt::UiStyle::visionPreviewDefaultHeight,
                                Selt::UiStyle::visionResultDefaultHeight});
    m_visionSplitter->setChildrenCollapsible(false);
    m_previewTabs->setMinimumHeight(120);
    m_resultTabs->setMinimumHeight(100);
    m_previewTabs->setMinimumWidth(160);
    m_resultTabs->setMinimumWidth(140);
    visionLayout->addWidget(m_visionSplitter);

    m_visionDock = new QDockWidget(QStringLiteral("图像与结果"), this);
    m_visionDock->setObjectName(QStringLiteral("VisionWorkbenchDock"));
    m_visionDock->setAllowedAreas(Qt::RightDockWidgetArea);
    m_visionDock->setFeatures(QDockWidget::DockWidgetMovable
                              | QDockWidget::DockWidgetFloatable
                              | QDockWidget::DockWidgetClosable);
    m_visionDock->setMinimumWidth(Selt::UiStyle::rightDockMinWidth);
    m_visionDock->setWidget(visionRoot);
    addDockWidget(Qt::RightDockWidgetArea, m_visionDock);
    resizeDocks({m_visionDock}, {Selt::UiStyle::rightDockDefaultWidth}, Qt::Horizontal);
    splitDockWidget(m_visionDock, m_rightDock, Qt::Horizontal);

    // Legacy bottom dock kept for old windowState restore; content lives in vision dock.
    m_bottomDock = new QDockWidget(QStringLiteral("运行结果(兼容)"), this);
    m_bottomDock->setObjectName(QStringLiteral("VisionResultDock"));
    m_bottomDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_bottomDock->setFeatures(QDockWidget::DockWidgetClosable);
    m_bottomDock->setWidget(new QLabel(QStringLiteral("结果区已移至右侧「图像与结果」。"), m_bottomDock));
    addDockWidget(Qt::BottomDockWidgetArea, m_bottomDock);
    m_bottomDock->hide();

    m_resourceBrowser = new ResourceBrowserDock(this);
    if (m_projectService && m_projectService->project())
        m_resourceBrowser->setStore(&m_projectService->project()->resources());
    m_resourceDock = new QDockWidget(QStringLiteral("项目资源"), this);
    m_resourceDock->setObjectName(QStringLiteral("ResourceBrowserDock"));
    m_resourceDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_resourceDock->setFeatures(QDockWidget::DockWidgetMovable
                                | QDockWidget::DockWidgetFloatable
                                | QDockWidget::DockWidgetClosable);
    m_resourceDock->setWidget(m_resourceBrowser);
    addDockWidget(Qt::LeftDockWidgetArea, m_resourceDock);
    m_resourceDock->hide();

    m_communicationDock = new CommunicationDock(this);
    m_communicationDockWidget = new QDockWidget(QStringLiteral("通讯配置"), this);
    m_communicationDockWidget->setObjectName(QStringLiteral("CommunicationDock"));
    m_communicationDockWidget->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_communicationDockWidget->setFeatures(QDockWidget::DockWidgetMovable
                                           | QDockWidget::DockWidgetFloatable
                                           | QDockWidget::DockWidgetClosable);
    m_communicationDockWidget->setWidget(m_communicationDock);
    addDockWidget(Qt::LeftDockWidgetArea, m_communicationDockWidget);
    m_communicationDockWidget->hide();
    connect(m_communicationDock, &CommunicationDock::diagnosticsReady, this, [this](const QStringList &lines) {
        if (m_runtimeMonitor)
            m_runtimeMonitor->showDiagnostics(lines);
        if (!lines.isEmpty())
            statusBar()->showMessage(lines.last(), 4000);
    });

    m_variableDock = new VariableDock(this);
    if (m_projectService && m_projectService->project())
        m_variableDock->setProject(m_projectService->project());
    m_variableDockWidget = new QDockWidget(QStringLiteral("变量管理"), this);
    m_variableDockWidget->setObjectName(QStringLiteral("VariableDock"));
    m_variableDockWidget->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_variableDockWidget->setFeatures(QDockWidget::DockWidgetMovable
                                      | QDockWidget::DockWidgetFloatable
                                      | QDockWidget::DockWidgetClosable);
    m_variableDockWidget->setWidget(m_variableDock);
    addDockWidget(Qt::LeftDockWidgetArea, m_variableDockWidget);
    m_variableDockWidget->hide();
    connect(m_variableDock, &VariableDock::diagnosticsReady, this, [this](const QStringList &lines) {
        if (m_runtimeMonitor)
            m_runtimeMonitor->showDiagnostics(lines);
        if (!lines.isEmpty())
            statusBar()->showMessage(lines.last(), 4000);
    });

    if (QMenu *viewMenu = menuBar()->findChild<QMenu *>(QString(), Qt::FindDirectChildrenOnly)) {
        Q_UNUSED(viewMenu);
    }
    // Insert after docks exist so toggleViewAction is valid.
    for (QAction *action : menuBar()->actions()) {
        QMenu *menu = action->menu();
        if (!menu || !menu->title().contains(QStringLiteral("视图")))
            continue;
        QAction *toggleResources = m_resourceDock->toggleViewAction();
        toggleResources->setText(QStringLiteral("项目资源"));
        menu->insertAction(m_toggleToolBarAction, toggleResources);
        QAction *toggleComm = m_communicationDockWidget->toggleViewAction();
        toggleComm->setText(QStringLiteral("通讯配置"));
        menu->insertAction(m_toggleToolBarAction, toggleComm);
        if (m_variableDockWidget) {
            QAction *toggleVars = m_variableDockWidget->toggleViewAction();
            toggleVars->setText(QStringLiteral("变量管理"));
            menu->insertAction(m_toggleToolBarAction, toggleVars);
        }
        break;
    }

    m_runController = new Selt::RunController(this);
    m_runController->setDocument(m_document);
    if (m_projectService && m_projectService->project())
        m_runController->setProjectVariables(&m_projectService->project()->variables());
    if (m_projectService && m_projectService->project())
        m_runController->setResourceStore(&m_projectService->project()->resources());
    syncRunVariableScopes();
    m_runController->setInputSource(m_inputSource);

    m_calibrationStore = new CalibrationStore;
    {
        QSettings settings(QStringLiteral("Selt"), QStringLiteral("SeltVisionStudio"));
        const QByteArray raw = settings.value(QStringLiteral("industrial/calibrations")).toByteArray();
        if (!raw.isEmpty()) {
            const QJsonDocument doc = QJsonDocument::fromJson(raw);
            if (doc.isObject())
                *m_calibrationStore = CalibrationStore::fromJson(doc.object());
        }
        if (m_calibrationStore->hasActive())
            m_runController->setCalibration(m_calibrationStore->active());
    }
#endif

#if SELT_HAS_OPENCV
    if (m_projectService && m_projectService->project())
        m_propertyPanel->setProjectVariables(&m_projectService->project()->variables());
#endif

    m_toggleNodesDockAction->setChecked(true);
    m_toggleInspectorDockAction->setChecked(false);
#if SELT_HAS_OPENCV
    m_toggleResultsDockAction->setChecked(true);
#else
    m_toggleResultsDockAction->setChecked(false);
#endif
    connect(m_toggleNodesDockAction, &QAction::toggled, m_leftDock, &QDockWidget::setVisible);
    connect(m_leftDock, &QDockWidget::visibilityChanged, m_toggleNodesDockAction, &QAction::setChecked);
    connect(m_toggleInspectorDockAction, &QAction::toggled, m_rightDock, &QDockWidget::setVisible);
    connect(m_rightDock, &QDockWidget::visibilityChanged, m_toggleInspectorDockAction, &QAction::setChecked);
    connect(m_mainToolBar, &QToolBar::visibilityChanged,
            m_toggleToolBarAction, &QAction::setChecked);
#if SELT_HAS_OPENCV
    connect(m_toggleResultsDockAction, &QAction::toggled, m_visionDock, &QDockWidget::setVisible);
    connect(m_visionDock, &QDockWidget::visibilityChanged, m_toggleResultsDockAction, &QAction::setChecked);
#else
    m_toggleResultsDockAction->setEnabled(false);
#endif
}

void MainWindow::setupConnections()
{
#if SELT_HAS_OPENCV
    connect(m_projectService, &Selt::ProjectService::modifiedChanged, this, [this](bool) {
        updateWindowTitle();
    });
    connect(m_projectService, &Selt::ProjectService::titleChanged, this, [this](const QString &) {
        updateWindowTitle();
    });
    connect(m_projectService, &Selt::ProjectService::flowListChanged, this, &MainWindow::refreshFlowBar);
    connect(m_projectService, &Selt::ProjectService::activeFlowChanged, this, &MainWindow::onActiveFlowChanged);
    connect(m_projectService, &Selt::ProjectService::undoStackChanged, this, &MainWindow::onUndoStackChanged);
    connect(m_projectService, &Selt::ProjectService::statusMessage, this, [this](const QString &msg) {
        statusBar()->showMessage(msg, 4000);
    });
    connect(m_projectService, &Selt::ProjectService::projectReset, this, [this]() {
        clearVisionResults();
        m_scene->clearSelection();
        refreshFlowBar();
        updateWindowTitle();
        if (m_projectService && m_projectService->project()) {
            m_propertyPanel->setProjectVariables(&m_projectService->project()->variables());
            if (m_runController) {
                m_runController->setProjectVariables(&m_projectService->project()->variables());
                m_runController->setResourceStore(&m_projectService->project()->resources());
                syncRunVariableScopes();
            }
        }
    });
    if (m_flowBar) {
        connect(m_flowBar, &FlowBarWidget::activeFlowChosen, this, &MainWindow::onFlowChosen);
        connect(m_flowBar, &FlowBarWidget::createFlowRequested, this, &MainWindow::onCreateFlowRequested);
        connect(m_flowBar, &FlowBarWidget::renameFlowRequested, this, &MainWindow::onRenameFlowRequested);
        connect(m_flowBar, &FlowBarWidget::removeFlowRequested, this, &MainWindow::onRemoveFlowRequested);
    }
#else
    connect(m_document, &Document::modifiedChanged, this, [this](bool) { updateWindowTitle(); });
    connect(m_document, &Document::titleChanged, this, [this](const QString &) { updateWindowTitle(); });
#endif
    connect(m_view, &CanvasView::zoomChanged, this, &MainWindow::updateStatusZoom);
    connect(m_view, &CanvasView::mouseScenePosChanged, this, &MainWindow::updateStatusPos);
    connect(m_scene, &CanvasScene::selectionNodeChanged, this, &MainWindow::onSelectionNodeChanged);
    connect(m_scene, &CanvasScene::nodeDoubleClicked,
            this, &MainWindow::openNodeInspector);
    connect(m_scene, &CanvasScene::runNodeRequested, this, [this](const QString &nodeId) {
#if SELT_HAS_OPENCV
        if (m_runController)
            m_runController->runSingleNode(nodeId);
#else
        Q_UNUSED(nodeId);
#endif
    });
    connect(m_scene, &CanvasScene::runFromNodeRequested, this, [this](const QString &nodeId) {
#if SELT_HAS_OPENCV
        if (m_runController)
            m_runController->runFromNode(nodeId);
#else
        Q_UNUSED(nodeId);
#endif
    });
    connect(m_scene, &CanvasScene::inspectUpstreamRequested, this,
            [this](const QString &nodeId) {
                QStringList upstream;
                for (const ConnectionModel &connection : m_document->connections()) {
                    if (connection.targetNodeId == nodeId)
                        upstream.append(connection.sourceNodeId);
                }
                statusBar()->showMessage(
                    upstream.isEmpty()
                        ? QStringLiteral("该节点没有上游依赖")
                        : QStringLiteral("上游依赖: %1").arg(upstream.join(QStringLiteral(", "))),
                    5000);
            });
    connect(m_scene, &CanvasScene::statusMessage, this, [this](const QString &msg) {
        statusBar()->showMessage(msg, 3000);
    });
    connect(m_scene, &CanvasScene::connectionDataTypeFilterRequested, m_palette,
            &NodePalette::setDataTypeFilter);
    connect(m_palette, &NodePalette::nodeTypeActivated, this, &MainWindow::onPaletteActivated);
    connect(m_palette, &NodePalette::nodeTypeSelected, this, [this](const QString &type) {
        if (!m_helpLabel || type.isEmpty())
            return;
#if SELT_HAS_OPENCV
        const ModuleDescriptor desc = VisionNodeRegistry::descriptor(type);
        const QString help = Selt::helpTextOrDescription(desc.helpText, desc.description);
        m_helpLabel->setText(QStringLiteral("<b>%1</b><br/>分类：%2<br/><br/>%3<br/><i>双击工具箱项可创建到画布</i>")
                                 .arg(desc.displayName.toHtmlEscaped(),
                                      desc.category.toHtmlEscaped(),
                                      help.toHtmlEscaped()));
#else
        Q_UNUSED(type);
#endif
    });
    connect(m_document, &Document::nodeUpdated, this, [this](const NodeModel &node) {
        if (node.id != m_propertyPanel->property("currentNodeId").toString())
            return;
        // 参数面板写回过程中禁止同步重建，避免销毁正在响应的 ROI「启用」复选框
        if (m_propertyPanel->isApplyingChanges())
            return;
        m_propertyPanel->setSelectedNode(node.id);
        if (m_nodeInspectorDialog && m_nodeInspectorDialog->nodeId() == node.id)
            m_nodeInspectorDialog->refresh();
#if SELT_HAS_OPENCV
        syncSelectedModuleStatus(node.id);
#endif
    });
#if SELT_HAS_OPENCV
    connect(m_measurementPanel, &MeasurementPanel::outputNodeChanged,
            this, &MainWindow::onVisionOutputNodeChanged);
    connect(m_resultPreview, &ImagePreviewWidget::roiChanged,
            this, &MainWindow::onResultRoiChanged);
    connect(m_resultPreview, &ImagePreviewWidget::teachTemplateRequested,
            this, &MainWindow::onTeachTemplateRequested);
    connect(m_runController, &Selt::RunController::runFinished,
            this, &MainWindow::onRunFinished);
    connect(m_runController, &Selt::RunController::runProgress,
            this, &MainWindow::onRunProgress);
    connect(m_runController, &Selt::RunController::busyChanged,
            this, &MainWindow::onRunBusyChanged);
    connect(m_runController, &Selt::RunController::runStarted, this, [this](Selt::RuntimeMode) {
        clearNodeRunStatuses();
        onRunBusyChanged(true);
    });
    connect(m_runController, &Selt::RunController::diagnosticsChanged,
            this, [this](const QStringList &lines) {
                if (m_runtimeMonitor)
                    m_runtimeMonitor->showDiagnostics(lines);
            });
    connect(m_runController, &Selt::RunController::tasksChanged,
            this, [this](const QVector<Selt::ThreadTaskSnapshot> &tasks) {
                if (m_runtimeMonitor)
                    m_runtimeMonitor->showTasks(tasks);
            });
    if (m_inputSource) {
        connect(m_inputSource, &Selt::InputSourceService::diagnosticsChanged,
                this, [this](const QVector<Selt::RuntimeDiagnostic> &diags) {
                    if (!m_runtimeMonitor || diags.isEmpty())
                        return;
                    QStringList lines;
                    for (const Selt::RuntimeDiagnostic &d : diags) {
                        lines.append(QStringLiteral("[%1/%2] %3")
                                         .arg(Selt::runtimeFailureKindDisplayName(d.kind),
                                              d.code, d.message));
                    }
                    m_runtimeMonitor->showDiagnostics(lines);
                });
    }
    onRunBusyChanged(false);
#endif
}

void MainWindow::updateRunStatus(const QString &text, int statusKind)
{
    if (!m_runStatusLabel)
        return;
    m_runStatusLabel->setText(text);
    QString bg;
    switch (statusKind) {
    case 1:
        bg = Selt::UiStyle::runningBlue().name();
        break;
    case 2:
        bg = Selt::UiStyle::successGreen().name();
        break;
    case 3:
        bg = Selt::UiStyle::failRed().name();
        break;
    default:
        bg.clear();
        break;
    }
    if (bg.isEmpty()) {
        m_runStatusLabel->setStyleSheet(QString());
    } else {
        m_runStatusLabel->setStyleSheet(
            QStringLiteral("background:%1;color:#ffffff;border-radius:10px;padding:2px 10px;")
                .arg(bg));
    }
}

void MainWindow::openNodeInspector(const QString &nodeId)
{
    if (!m_document || nodeId.isEmpty())
        return;

    if (!m_nodeInspectorDialog) {
        const Selt::ProjectVariableStore *variables = nullptr;
#if SELT_HAS_OPENCV
        if (m_projectService && m_projectService->project())
            variables = &m_projectService->project()->variables();
#endif
        m_nodeInspectorDialog = new NodeInspectorDialog(
            m_document, m_undoStack, variables, this);
        connect(m_nodeInspectorDialog, &NodeInspectorDialog::runRequested,
                this, &MainWindow::runVisionPipeline);
#if SELT_HAS_OPENCV
        connect(m_runController, &Selt::RunController::busyChanged,
                m_nodeInspectorDialog, &NodeInspectorDialog::setRunEnabled);
        m_nodeInspectorDialog->setRunEnabled(!m_runController->isBusy());
#endif
    } else {
        m_nodeInspectorDialog->setDocument(m_document);
        m_nodeInspectorDialog->setUndoStack(m_undoStack);
#if SELT_HAS_OPENCV
        const Selt::ProjectVariableStore *variables = nullptr;
        if (m_projectService && m_projectService->project())
            variables = &m_projectService->project()->variables();
        m_nodeInspectorDialog->setProjectVariables(variables);
#endif
    }

    m_nodeInspectorDialog->setNodeId(nodeId);
    m_nodeInspectorDialog->show();
    m_nodeInspectorDialog->raise();
    m_nodeInspectorDialog->activateWindow();
}

void MainWindow::setThemeLight()
{
    m_themeController->setMode(Selt::ThemeMode::Light);
}

void MainWindow::setThemeDark()
{
    m_themeController->setMode(Selt::ThemeMode::Dark);
}

void MainWindow::setThemeSystem()
{
    m_themeController->setMode(Selt::ThemeMode::System);
}

void MainWindow::captureDefaultLayout()
{
    applyDefaultDockVisibility();
    m_defaultGeometry = saveGeometry();
    m_defaultState = saveState(kWindowLayoutVersion);
}

void MainWindow::saveWindowLayout() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("ui/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("ui/windowState"), saveState(kWindowLayoutVersion));
    settings.setValue(QStringLiteral("ui/layoutVersion"), kWindowLayoutVersion);
#if SELT_HAS_OPENCV
    saveVisionWorkbenchLayout();
#endif
}

#if SELT_HAS_OPENCV
void MainWindow::setVisionWorkbenchOrientation(Qt::Orientation orientation)
{
    if (!m_visionSplitter)
        return;
    if (m_visionSplitter->orientation() == orientation)
        return;
    m_visionSplitter->setOrientation(orientation);
    if (orientation == Qt::Horizontal) {
        m_visionSplitter->setSizes({Selt::UiStyle::visionPreviewDefaultWidth,
                                    Selt::UiStyle::visionResultDefaultWidth});
    } else {
        m_visionSplitter->setSizes({Selt::UiStyle::visionPreviewDefaultHeight,
                                    Selt::UiStyle::visionResultDefaultHeight});
    }
    if (m_toggleVisionOrientationAction)
        m_toggleVisionOrientationAction->setChecked(orientation == Qt::Horizontal);
    saveVisionWorkbenchLayout();
}

void MainWindow::toggleVisionWorkbenchOrientation()
{
    if (!m_visionSplitter)
        return;
    const Qt::Orientation next = m_visionSplitter->orientation() == Qt::Vertical
        ? Qt::Horizontal
        : Qt::Vertical;
    setVisionWorkbenchOrientation(next);
    statusBar()->showMessage(next == Qt::Horizontal
                                 ? QStringLiteral("图像与结果：左右布局")
                                 : QStringLiteral("图像与结果：上下布局"),
                             2500);
}

void MainWindow::resetVisionWorkbenchLayout()
{
    if (!m_visionSplitter)
        return;
    m_visionSplitter->setOrientation(Qt::Vertical);
    m_visionSplitter->setSizes({Selt::UiStyle::visionPreviewDefaultHeight,
                                Selt::UiStyle::visionResultDefaultHeight});
    if (m_toggleVisionOrientationAction)
        m_toggleVisionOrientationAction->setChecked(false);
    if (m_visionDock) {
        m_visionDock->show();
        resizeDocks({m_visionDock}, {Selt::UiStyle::rightDockDefaultWidth}, Qt::Horizontal);
    }
    saveVisionWorkbenchLayout();
    statusBar()->showMessage(QStringLiteral("已恢复图像与结果默认布局"), 2500);
}

void MainWindow::saveVisionWorkbenchLayout() const
{
    if (!m_visionSplitter)
        return;
    QSettings settings;
    settings.setValue(QStringLiteral("ui/visionSplitterOrientation"),
                      static_cast<int>(m_visionSplitter->orientation()));
    settings.setValue(QStringLiteral("ui/visionSplitterState"), m_visionSplitter->saveState());
}

void MainWindow::restoreVisionWorkbenchLayout()
{
    if (!m_visionSplitter)
        return;
    QSettings settings;
    const int orientation = settings.value(QStringLiteral("ui/visionSplitterOrientation"),
                                           static_cast<int>(Qt::Vertical))
                                .toInt();
    m_visionSplitter->setOrientation(orientation == static_cast<int>(Qt::Horizontal)
                                         ? Qt::Horizontal
                                         : Qt::Vertical);
    const QByteArray state = settings.value(QStringLiteral("ui/visionSplitterState")).toByteArray();
    if (!state.isEmpty())
        m_visionSplitter->restoreState(state);
    else if (m_visionSplitter->orientation() == Qt::Horizontal) {
        m_visionSplitter->setSizes({Selt::UiStyle::visionPreviewDefaultWidth,
                                    Selt::UiStyle::visionResultDefaultWidth});
    } else {
        m_visionSplitter->setSizes({Selt::UiStyle::visionPreviewDefaultHeight,
                                    Selt::UiStyle::visionResultDefaultHeight});
    }
    if (m_toggleVisionOrientationAction) {
        m_toggleVisionOrientationAction->setChecked(m_visionSplitter->orientation()
                                                    == Qt::Horizontal);
    }
}
#endif

void MainWindow::applyDefaultDockVisibility()
{
    if (m_leftDock)
        m_leftDock->show();
    if (m_rightDock)
        m_rightDock->hide();
#if SELT_HAS_OPENCV
    if (m_visionDock)
        m_visionDock->show();
    if (m_bottomDock)
        m_bottomDock->hide();
    if (m_leftDock && m_visionDock) {
        resizeDocks({m_leftDock, m_visionDock},
                    {Selt::UiStyle::leftDockDefaultWidth, Selt::UiStyle::rightDockDefaultWidth},
                    Qt::Horizontal);
    } else if (m_leftDock) {
        resizeDocks({m_leftDock}, {Selt::UiStyle::leftDockDefaultWidth}, Qt::Horizontal);
    }
#else
    if (m_leftDock)
        resizeDocks({m_leftDock}, {Selt::UiStyle::leftDockDefaultWidth}, Qt::Horizontal);
#endif
    syncDockToggleActions();
}

void MainWindow::syncDockToggleActions()
{
    if (m_toggleNodesDockAction && m_leftDock)
        m_toggleNodesDockAction->setChecked(m_leftDock->isVisible());
    if (m_toggleInspectorDockAction && m_rightDock)
        m_toggleInspectorDockAction->setChecked(m_rightDock->isVisible());
#if SELT_HAS_OPENCV
    if (m_toggleResultsDockAction && m_visionDock)
        m_toggleResultsDockAction->setChecked(m_visionDock->isVisible());
#else
    if (m_toggleResultsDockAction)
        m_toggleResultsDockAction->setChecked(false);
#endif
    if (m_toggleToolBarAction && m_mainToolBar)
        m_toggleToolBarAction->setChecked(m_mainToolBar->isVisible());
}

void MainWindow::restoreWindowLayout()
{
    QSettings settings;
    const QByteArray geometry = settings.value(QStringLiteral("ui/geometry")).toByteArray();
    const QByteArray state = settings.value(QStringLiteral("ui/windowState")).toByteArray();
    const int savedVersion = settings.value(QStringLiteral("ui/layoutVersion"), 0).toInt();

    bool geometryOk = false;
    if (!geometry.isEmpty())
        geometryOk = restoreGeometry(geometry);

    bool stateOk = false;
    if (!state.isEmpty() && savedVersion == kWindowLayoutVersion)
        stateOk = restoreState(state, kWindowLayoutVersion);

    if (!geometryOk && !m_defaultGeometry.isEmpty())
        restoreGeometry(m_defaultGeometry);

    if (!stateOk) {
        if (!m_defaultState.isEmpty())
            restoreState(m_defaultState, kWindowLayoutVersion);
        applyDefaultDockVisibility();
    } else {
#if SELT_HAS_OPENCV
        // Legacy bottom dock must never steal layout after a successful restore.
        if (m_bottomDock)
            m_bottomDock->hide();
        restoreVisionWorkbenchLayout();
#endif
        syncDockToggleActions();
    }
}

void MainWindow::resetLayout()
{
    if (!m_defaultGeometry.isEmpty())
        restoreGeometry(m_defaultGeometry);
    if (!m_defaultState.isEmpty())
        restoreState(m_defaultState, kWindowLayoutVersion);
    applyDefaultDockVisibility();
#if SELT_HAS_OPENCV
    resetVisionWorkbenchLayout();
#endif

    QSettings settings;
    settings.setValue(QStringLiteral("ui/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("ui/windowState"), saveState(kWindowLayoutVersion));
    settings.setValue(QStringLiteral("ui/layoutVersion"), kWindowLayoutVersion);
    statusBar()->showMessage(QStringLiteral("已恢复默认布局，旧布局状态已覆盖"), 3000);
}

void MainWindow::newDocument()
{
    if (!maybeSave())
        return;
#if SELT_HAS_OPENCV
    m_projectService->newProject();
    m_document = m_projectService->document();
    m_undoStack = m_projectService->activeUndoStack();
    m_scene->setUndoStack(m_undoStack);
    m_propertyPanel->setDocument(m_document);
    m_propertyPanel->setUndoStack(m_undoStack);
    if (m_nodeInspectorDialog) {
        m_nodeInspectorDialog->setDocument(m_document);
        m_nodeInspectorDialog->setUndoStack(m_undoStack);
    }
    if (m_projectService && m_projectService->project())
        m_propertyPanel->setProjectVariables(&m_projectService->project()->variables());
    if (m_nodeInspectorDialog && m_projectService && m_projectService->project())
        m_nodeInspectorDialog->setProjectVariables(&m_projectService->project()->variables());
    if (m_runController) {
        m_runController->setDocument(m_document);
        if (m_projectService && m_projectService->project()) {
            m_runController->setProjectVariables(&m_projectService->project()->variables());
            m_runController->setResourceStore(&m_projectService->project()->resources());
            syncRunVariableScopes();
        }
    }
    m_currentFile.clear();
    clearVisionResults();
    m_view->resetZoom();
#else
    m_undoStack->clear();
    m_document->clear();
    setCurrentFile(QString());
#endif
    updateRunStatus(QStringLiteral("空闲"), 0);
    updateWindowTitle();
}

void MainWindow::openDocument()
{
    if (!maybeSave())
        return;
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开工程"),
        QString(),
        QStringLiteral("Selt 工程 (*.selt);;JSON (*.json);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;
    loadFromPath(path);
}

void MainWindow::saveDocument()
{
    if (m_currentFile.isEmpty())
        saveDocumentAs();
    else
        saveToPath(m_currentFile);
}

void MainWindow::saveDocumentAs()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("另存为"),
        m_currentFile.isEmpty() ? QStringLiteral("未命名.selt") : m_currentFile,
        QStringLiteral("Selt 工程 (*.selt);;JSON (*.json)"));
    if (path.isEmpty())
        return;
    saveToPath(path);
}

bool MainWindow::maybeSave()
{
#if SELT_HAS_OPENCV
    if (!m_projectService->isModified())
        return true;
#else
    if (!m_document->isModified())
        return true;
#endif
    const auto ret = QMessageBox::warning(
        this, QStringLiteral("未保存的更改"),
        QStringLiteral("文档已修改，是否保存？"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (ret == QMessageBox::Save) {
        saveDocument();
#if SELT_HAS_OPENCV
        return !m_projectService->isModified();
#else
        return !m_document->isModified();
#endif
    }
    return ret == QMessageBox::Discard;
}

bool MainWindow::saveToPath(const QString &path)
{
    QString error;
#if SELT_HAS_OPENCV
    if (path.endsWith(QStringLiteral(".seltproject"), Qt::CaseInsensitive)) {
        if (!m_projectService->saveContainer(path, &error)) {
            QMessageBox::critical(this, QStringLiteral("保存失败"), error);
            return false;
        }
        setCurrentFile(path);
        addRecentFile(path);
        statusBar()->showMessage(QStringLiteral("已保存工程容器: %1").arg(path), 3000);
        return true;
    }
    if (!m_projectService->saveLegacySelt(path, &error)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), error);
        return false;
    }
    setCurrentFile(path);
#else
    if (!DocumentSerializer::saveToFile(*m_document, path, &error)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), error);
        return false;
    }
    m_document->markClean();
    setCurrentFile(path);
#endif
    addRecentFile(path);
    statusBar()->showMessage(QStringLiteral("已保存: %1").arg(path), 3000);
    return true;
}

bool MainWindow::loadFromPath(const QString &path)
{
#if SELT_HAS_OPENCV
    if (path.endsWith(QStringLiteral(".seltproject"), Qt::CaseInsensitive)) {
        Selt::ProjectContainerReport report;
        if (!m_projectService->openContainer(path, &report)) {
            QMessageBox::critical(this, QStringLiteral("打开失败"),
                                  report.error.isEmpty()
                                      ? QStringLiteral("无法打开工程容器")
                                      : report.error);
            return false;
        }
        m_document = m_projectService->document();
        m_undoStack = m_projectService->activeUndoStack();
        m_scene->setUndoStack(m_undoStack);
        m_propertyPanel->setDocument(m_document);
        m_propertyPanel->setUndoStack(m_undoStack);
        if (m_nodeInspectorDialog) {
            m_nodeInspectorDialog->setDocument(m_document);
            m_nodeInspectorDialog->setUndoStack(m_undoStack);
        }
        if (m_projectService->project()) {
            m_propertyPanel->setProjectVariables(&m_projectService->project()->variables());
            if (m_nodeInspectorDialog)
                m_nodeInspectorDialog->setProjectVariables(
                    &m_projectService->project()->variables());
        }
        if (m_runController) {
            m_runController->setDocument(m_document);
            m_runController->setProjectVariables(&m_projectService->project()->variables());
            m_runController->setResourceStore(&m_projectService->project()->resources());
            syncRunVariableScopes();
        }
        clearVisionResults();
        m_scene->clearSelection();
        setCurrentFile(path);
        addRecentFile(path);
        refreshFlowBar();
        if (m_runtimeMonitor && !report.diagnostics.isEmpty())
            m_runtimeMonitor->showDiagnostics(report.diagnostics);
        statusBar()->showMessage(
            QStringLiteral("已打开工程容器: %1").arg(path), 5000);
        return true;
    }
    Selt::MigrationReport report;
    if (!m_projectService->openLegacySelt(path, &report)) {
        QMessageBox::critical(this,
                              QStringLiteral("打开失败"),
                              report.error.isEmpty() ? QStringLiteral("无法打开工程") : report.error);
        return false;
    }
    m_document = m_projectService->document();
    m_undoStack = m_projectService->activeUndoStack();
    m_scene->setUndoStack(m_undoStack);
    m_propertyPanel->setDocument(m_document);
    m_propertyPanel->setUndoStack(m_undoStack);
    if (m_nodeInspectorDialog) {
        m_nodeInspectorDialog->setDocument(m_document);
        m_nodeInspectorDialog->setUndoStack(m_undoStack);
    }
    if (m_projectService && m_projectService->project())
        m_propertyPanel->setProjectVariables(&m_projectService->project()->variables());
    if (m_nodeInspectorDialog && m_projectService && m_projectService->project())
        m_nodeInspectorDialog->setProjectVariables(&m_projectService->project()->variables());
    if (m_runController) {
        m_runController->setDocument(m_document);
        if (m_projectService && m_projectService->project()) {
            m_runController->setProjectVariables(&m_projectService->project()->variables());
            m_runController->setResourceStore(&m_projectService->project()->resources());
            syncRunVariableScopes();
        }
    }
    clearVisionResults();
    m_scene->clearSelection();
    setCurrentFile(path);
    addRecentFile(path);
    refreshFlowBar();
    QString msg = QStringLiteral("已打开: %1").arg(path);
    if (!report.notes.isEmpty())
        msg += QStringLiteral("（%1）").arg(report.notes.join(QStringLiteral("; ")));
    statusBar()->showMessage(msg, 5000);
    return true;
#else
    QString error;
    Document temp;
    if (!DocumentSerializer::loadFromFile(temp, path, &error)) {
        QMessageBox::critical(this, QStringLiteral("打开失败"), error);
        return false;
    }

    m_undoStack->clear();
    QVector<NodeModel> nodes = temp.nodes();
    QVector<ConnectionModel> connections = temp.connections();
    VisionNodeRegistry::upgradeGraph(nodes, connections);
    m_document->replaceAll(temp.title(), temp.settings(), nodes, connections);
    setCurrentFile(path);
    addRecentFile(path);
    statusBar()->showMessage(QStringLiteral("已打开: %1").arg(path), 3000);
    return true;
#endif
}

void MainWindow::setCurrentFile(const QString &path)
{
    m_currentFile = path;
#if SELT_HAS_OPENCV
    m_projectService->setCurrentFilePath(path);
    if (!path.isEmpty()) {
        const QString base = QFileInfo(path).completeBaseName();
        if (m_projectService->project()->title() != base)
            m_projectService->project()->setTitle(base);
        m_projectService->project()->markClean();
        m_document->markClean();
    }
#else
    if (!path.isEmpty()) {
        const QString base = QFileInfo(path).completeBaseName();
        if (m_document->title() != base) {
            const bool wasModified = m_document->isModified();
            m_document->setTitle(base);
            if (!wasModified)
                m_document->markClean();
        }
    }
#endif
    updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
#if SELT_HAS_OPENCV
    QString title = m_projectService->windowTitleBase();
    if (!m_currentFile.isEmpty())
        title += QStringLiteral(" - ") + m_currentFile;
    if (m_projectService->isModified())
        title += QStringLiteral(" *");
    title += QStringLiteral(" - Selt");
#else
    QString title = m_document->title();
    if (!m_currentFile.isEmpty())
        title += QStringLiteral(" - ") + m_currentFile;
    if (m_document->isModified())
        title += QStringLiteral(" *");
    title += QStringLiteral(" - Selt");
#endif
    setWindowTitle(title);
}

void MainWindow::updateStatusZoom(qreal factor)
{
    m_zoomLabel->setText(QStringLiteral("缩放 %1%").arg(qRound(factor * 100)));
}

void MainWindow::updateStatusPos(const QPointF &pos)
{
    m_posLabel->setText(QStringLiteral("坐标 (%1, %2)").arg(qRound(pos.x())).arg(qRound(pos.y())));
}

void MainWindow::onSelectionNodeChanged(const QString &nodeId)
{
    m_propertyPanel->setProperty("currentNodeId", nodeId);
    m_propertyPanel->setSelectedNode(nodeId);
#if SELT_HAS_OPENCV
    syncSelectedModuleStatus(nodeId);
    if (m_helpLabel) {
        if (m_document && m_document->hasNode(nodeId)) {
            const NodeModel node = m_document->node(nodeId);
            const ModuleDescriptor desc = VisionNodeRegistry::descriptor(node.type);
            const QString help = Selt::helpTextOrDescription(desc.helpText, desc.description);
            m_helpLabel->setText(QStringLiteral("<b>%1</b><br/>分类：%2<br/><br/>%3")
                                     .arg(desc.displayName.toHtmlEscaped(),
                                          desc.category.toHtmlEscaped(),
                                          help.toHtmlEscaped()));
        } else {
            m_helpLabel->setText(QStringLiteral("选中画布节点后，此处显示模块说明。"));
        }
    }
    if (m_hasVisionResult && !nodeId.isEmpty() && m_lastVisionContext.moduleResults.contains(nodeId)) {
        m_measurementPanel->setSelectedOutputNodeId(nodeId);
        refreshResultPreview(nodeId);
    } else if (m_document && m_document->hasNode(nodeId)) {
        const NodeModel node = m_document->node(nodeId);
        const RoiRect roi = RoiRect::fromJson(node.parameters.value(QStringLiteral("roi")).toObject());
        m_resultPreview->setRoi(roi);
    }
#endif
}

void MainWindow::onPaletteActivated(const QString &type)
{
    Selt::OperatorPreferences::pushRecent(type);
    const QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    m_scene->createNodeAt(type, center);
    if (m_palette)
        m_palette->reloadCatalog();
}

void MainWindow::toggleSnap(bool enabled)
{
    DocumentSettings s = m_document->settings();
    s.snapToGrid = enabled;
    m_document->setSettings(s);
}

void MainWindow::toggleGrid(bool enabled)
{
    DocumentSettings s = m_document->settings();
    s.showGrid = enabled;
    m_document->setSettings(s);
}

void MainWindow::addRecentFile(const QString &path)
{
    QSettings settings(QStringLiteral("Selt"), QStringLiteral("Selt_Gui"));
    QStringList files = settings.value(QStringLiteral("recentFiles")).toStringList();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > 8)
        files.removeLast();
    settings.setValue(QStringLiteral("recentFiles"), files);
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
    if (!m_recentMenu)
        return;
    m_recentMenu->clear();
    QSettings settings(QStringLiteral("Selt"), QStringLiteral("Selt_Gui"));
    const QStringList files = settings.value(QStringLiteral("recentFiles")).toStringList();
    for (const QString &file : files) {
        QAction *action = m_recentMenu->addAction(file);
        connect(action, &QAction::triggered, this, &MainWindow::openRecentFile);
    }
    m_recentMenu->setEnabled(!files.isEmpty());
}

void MainWindow::openRecentFile()
{
    auto *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;
    if (!maybeSave())
        return;
    loadFromPath(action->text());
}

void MainWindow::autoSave()
{
#if SELT_HAS_OPENCV
    if (!m_projectService->isModified())
        return;
    const QString autosavePath = m_currentFile.isEmpty()
        ? QStringLiteral("autosave.selt.tmp")
        : (m_currentFile + QStringLiteral(".tmp"));
    m_projectService->syncDocumentToActiveFlow();
    QString error;
    DocumentSerializer::saveToFile(*m_projectService->document(), autosavePath, &error);
#else
    if (!m_document->isModified())
        return;
    const QString autosavePath = m_currentFile.isEmpty()
        ? QStringLiteral("autosave.selt.tmp")
        : (m_currentFile + QStringLiteral(".tmp"));
    QString error;
    DocumentSerializer::saveToFile(*m_document, autosavePath, &error);
#endif
}

#if SELT_HAS_OPENCV
void MainWindow::refreshFlowBar()
{
    if (!m_flowBar || !m_projectService)
        return;
    VisionProject *project = m_projectService->project();
    m_flowBar->setFlows(project->flowIds(), project->flowNames(), project->activeFlowId());
}

void MainWindow::captureActiveViewport()
{
    if (!m_projectService || !m_view)
        return;
    Selt::FlowViewportState state;
    state.zoom = m_view->zoomFactor();
    state.center = m_view->viewCenter();
    state.hScroll = m_view->horizontalScroll();
    state.vScroll = m_view->verticalScroll();
    state.valid = true;
    m_projectService->setViewportState(m_projectService->project()->activeFlowId(), state);
}

void MainWindow::restoreViewportForFlow(const QString &flowId)
{
    if (!m_projectService || !m_view)
        return;
    const Selt::FlowViewportState state = m_projectService->viewportState(flowId);
    if (!state.valid) {
        m_view->resetZoom();
        return;
    }
    m_view->setZoomFactor(state.zoom);
    m_view->setViewCenter(state.center);
    m_view->setScrollOffsets(state.hScroll, state.vScroll);
}

void MainWindow::onFlowChosen(const QString &flowId)
{
    if (!m_projectService || flowId == m_projectService->project()->activeFlowId())
        return;
    captureActiveViewport();
    m_projectService->setActiveFlow(flowId);
}

void MainWindow::onCreateFlowRequested()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("新建流程"), QStringLiteral("流程名称:"),
        QLineEdit::Normal, QStringLiteral("流程"), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    captureActiveViewport();
    m_projectService->createFlow(name.trimmed());
    clearVisionResults();
    m_scene->clearSelection();
    restoreViewportForFlow(m_projectService->project()->activeFlowId());
    refreshFlowBar();
    updateWindowTitle();
}

void MainWindow::onRenameFlowRequested()
{
    VisionFlow *flow = m_projectService->project()->activeFlow();
    if (!flow)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("重命名流程"), QStringLiteral("流程名称:"),
        QLineEdit::Normal, flow->name(), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    m_projectService->renameFlow(flow->flowId(), name.trimmed());
    refreshFlowBar();
    updateWindowTitle();
}

void MainWindow::onRemoveFlowRequested()
{
    VisionProject *project = m_projectService->project();
    if (project->flowCount() <= 1) {
        QMessageBox::information(this, QStringLiteral("无法删除"),
                                 QStringLiteral("项目至少需要保留一个流程。"));
        return;
    }
    VisionFlow *flow = project->activeFlow();
    if (!flow)
        return;
    const auto ret = QMessageBox::question(
        this, QStringLiteral("删除流程"),
        QStringLiteral("确定删除流程「%1」吗？此操作不可撤销。").arg(flow->name()));
    if (ret != QMessageBox::Yes)
        return;
    captureActiveViewport();
    const QString removedId = flow->flowId();
    m_projectService->removeFlow(removedId);
    clearVisionResults();
    m_scene->clearSelection();
    restoreViewportForFlow(project->activeFlowId());
    refreshFlowBar();
    updateWindowTitle();
}

void MainWindow::onActiveFlowChanged(const QString &flowId)
{
    clearVisionResults();
    m_scene->clearSelection();
    m_propertyPanel->setSelectedNode(QString());
    restoreViewportForFlow(flowId);
#if SELT_HAS_OPENCV
    syncRunVariableScopes();
#endif
    refreshFlowBar();
    updateWindowTitle();
}

void MainWindow::onUndoStackChanged(QUndoStack *stack)
{
    m_undoStack = stack;
    m_scene->setUndoStack(stack);
    m_propertyPanel->setUndoStack(stack);
}

Document *MainWindow::document() const
{
    return m_document;
}

#if SELT_HAS_OPENCV
void MainWindow::syncRunVariableScopes()
{
    if (!m_runController || !m_projectService || !m_projectService->project())
        return;
    VisionProject *project = m_projectService->project();
    m_runController->setProjectVariables(&project->variables());
    if (VisionFlow *flow = project->activeFlow())
        m_runController->setFlowVariables(&flow->flowVariables(), flow->flowId());
    else
        m_runController->setFlowVariables(nullptr);
}
#endif

QUndoStack *MainWindow::undoStack() const
{
    return m_undoStack;
}

void MainWindow::applyVisionContext(const VisionContext &context, bool focusFailedNode)
{
    m_lastVisionContext = context;
    m_hasVisionResult = true;
    updateNodeRunStatuses(context);

    if (!context.originalImage.isEmpty())
        m_originalPreview->setImage(context.originalImage.toQImage(), QStringLiteral("原图"));

    const QString preferred = focusFailedNode && !context.failedNodeId.isEmpty()
        ? context.failedNodeId
        : (context.executionOrder.isEmpty() ? QString() : context.executionOrder.last());
    m_measurementPanel->showContext(context, preferred);
    refreshResultPreview(m_measurementPanel->selectedOutputNodeId());

    if (focusFailedNode && !context.failedNodeId.isEmpty()) {
        if (auto *item = m_scene->nodeItem(context.failedNodeId)) {
            m_scene->clearSelection();
            item->setSelected(true);
            m_view->centerOn(item);
        }
    }

    syncSelectedModuleStatus(m_propertyPanel->property("currentNodeId").toString());
}

void MainWindow::refreshResultPreview(const QString &nodeId)
{
    if (!m_hasVisionResult)
        return;

    VisionImage image;
    OverlayList overlays;
    QString title = QStringLiteral("结果图");

    if (nodeId.isEmpty()) {
        if (!m_lastVisionContext.resultImage.isEmpty())
            image = m_lastVisionContext.resultImage;
        else if (!m_lastVisionContext.binaryImage.isEmpty())
            image = m_lastVisionContext.binaryImage;
        else
            image = m_lastVisionContext.currentImage;
        overlays = m_lastVisionContext.overlaysForNode(
            m_lastVisionContext.executionOrder.isEmpty()
                ? QString()
                : m_lastVisionContext.executionOrder.last());
        if (overlays.isEmpty()) {
            for (auto it = m_lastVisionContext.moduleResults.cbegin();
                 it != m_lastVisionContext.moduleResults.cend(); ++it) {
                if (!it.value().overlays.isEmpty()) {
                    overlays = it.value().overlays;
                    break;
                }
            }
        }
    } else {
        const ModuleRunResult result = m_lastVisionContext.moduleResult(nodeId);
        image = result.outputImage;
        overlays = result.overlays;
        title = result.displayName.isEmpty() ? QStringLiteral("模块输出") : result.displayName;
    }

    if (!image.isEmpty())
        m_resultPreview->setImage(image.toQImage(), title);
    m_resultPreview->setOverlays(overlays);

    const QString selectedId = m_propertyPanel->property("currentNodeId").toString();
    if (m_document && m_document->hasNode(selectedId)) {
        const NodeModel node = m_document->node(selectedId);
        m_resultPreview->setRoi(RoiRect::fromJson(node.parameters.value(QStringLiteral("roi")).toObject()));
        m_resultPreview->setRoiEditEnabled(node.parameters.contains(QStringLiteral("roi")));
    }
}

void MainWindow::updateNodeRunStatuses(const VisionContext &context)
{
    for (const NodeModel &node : m_document->nodes()) {
        auto *item = m_scene->nodeItem(node.id);
        if (!item)
            continue;
        if (!context.moduleResults.contains(node.id)) {
            item->setRunStatus(NodeRunVisualStatus::Idle);
            item->clearRunSummary();
            continue;
        }
        const ModuleRunResult result = context.moduleResult(node.id);
        updateNodeRunStatus(node.id, result.status);
        QString summary;
        if (!result.errorMessage.isEmpty())
            summary = result.errorMessage;
        else if (!result.outputSummary.isEmpty())
            summary = result.outputSummary.constBegin().value();
        else if (result.elapsedMs > 0)
            summary = QStringLiteral("%1 ms").arg(result.elapsedMs);
        item->setRunSummary(summary, result.elapsedMs);
    }
}

void MainWindow::updateNodeRunStatus(const QString &nodeId, ModuleStatus status)
{
    auto *item = m_scene ? m_scene->nodeItem(nodeId) : nullptr;
    if (!item)
        return;
    switch (status) {
    case ModuleStatus::Running:
        item->setRunStatus(NodeRunVisualStatus::Running);
        break;
    case ModuleStatus::Success:
        item->setRunStatus(NodeRunVisualStatus::Success);
        break;
    case ModuleStatus::Failed:
        item->setRunStatus(NodeRunVisualStatus::Failed);
        break;
    case ModuleStatus::Disabled:
        item->setRunStatus(NodeRunVisualStatus::Disabled);
        break;
    case ModuleStatus::Idle:
    default:
        item->setRunStatus(NodeRunVisualStatus::Idle);
        break;
    }
}

void MainWindow::clearNodeRunStatuses()
{
    for (const NodeModel &node : m_document->nodes()) {
        if (auto *item = m_scene->nodeItem(node.id)) {
            item->setRunStatus(NodeRunVisualStatus::Idle);
            item->clearRunSummary();
        }
    }
}

void MainWindow::updateRunActionsEnabled()
{
    const bool busy = m_runController && m_runController->isBusy();
    const bool looping = m_runController
        && m_runController->isBusy()
        && m_runController->mode() == Selt::RuntimeMode::Loop;
    if (m_runAction)
        m_runAction->setEnabled(!busy);
    if (m_loopAction) {
        m_loopAction->setCheckable(true);
        m_loopAction->setChecked(looping);
        m_loopAction->setEnabled(!busy);
    }
    if (m_stopAction)
        m_stopAction->setEnabled(busy);
}

void MainWindow::onRunProgress(const QString &nodeId, ModuleStatus status)
{
    updateNodeRunStatus(nodeId, status);
}

void MainWindow::onRunBusyChanged(bool busy)
{
    Q_UNUSED(busy);
    updateRunActionsEnabled();
}

void MainWindow::syncSelectedModuleStatus(const QString &nodeId)
{
    if (!m_hasVisionResult || nodeId.isEmpty() || !m_lastVisionContext.moduleResults.contains(nodeId)) {
        m_propertyPanel->setModuleStatusText(QStringLiteral("未执行"));
        if (m_nodeInspectorDialog && m_nodeInspectorDialog->nodeId() == nodeId) {
            m_nodeInspectorDialog->setModuleStatusText(QStringLiteral("未执行"));
            m_nodeInspectorDialog->setRuntimeSummaryText(QStringLiteral("无结果"));
        }
        return;
    }
    const ModuleRunResult result = m_lastVisionContext.moduleResult(nodeId);
    QString text = moduleStatusToString(result.status);
    if (result.elapsedMs > 0)
        text += QStringLiteral(" (%1 ms)").arg(result.elapsedMs);
    if (!result.errorMessage.isEmpty())
        text += QStringLiteral(" - %1").arg(result.errorMessage);
    m_propertyPanel->setModuleStatusText(text);
    if (m_nodeInspectorDialog && m_nodeInspectorDialog->nodeId() == nodeId) {
        m_nodeInspectorDialog->setModuleStatusText(text);
        QStringList parts;
        if (!result.inputSummary.isEmpty()) {
            QStringList inputs;
            for (auto it = result.inputSummary.cbegin(); it != result.inputSummary.cend(); ++it)
                inputs.append(QStringLiteral("%1=%2").arg(it.key(), it.value()));
            parts.append(QStringLiteral("输入: %1").arg(inputs.join(QStringLiteral(", "))));
        }
        if (!result.outputSummary.isEmpty()) {
            QStringList outputs;
            for (auto it = result.outputSummary.cbegin(); it != result.outputSummary.cend(); ++it)
                outputs.append(QStringLiteral("%1=%2").arg(it.key(), it.value()));
            parts.append(QStringLiteral("输出: %1").arg(outputs.join(QStringLiteral(", "))));
        }
        parts.append(QStringLiteral("快照: %1")
                         .arg(m_lastVisionContext.snapshotCreatedAt.isValid()
                                  ? m_lastVisionContext.snapshotCreatedAt.toString(QStringLiteral("hh:mm:ss.zzz"))
                                  : QStringLiteral("-")));
        m_nodeInspectorDialog->setRuntimeSummaryText(parts.join(QStringLiteral(" | ")));
    }
}

void MainWindow::onVisionOutputNodeChanged(const QString &nodeId)
{
    refreshResultPreview(nodeId);
}

void MainWindow::onResultRoiChanged(const RoiRect &roi)
{
    const QString nodeId = m_propertyPanel->property("currentNodeId").toString();
    if (nodeId.isEmpty() || !m_document || !m_document->hasNode(nodeId))
        return;
    const NodeModel node = m_document->node(nodeId);
    if (!node.parameters.contains(QStringLiteral("roi")))
        return;
    m_propertyPanel->applyRoiParameter(QStringLiteral("roi"), roi.toJson());
}

void MainWindow::onTeachTemplateRequested()
{
    if (!m_resultPreview || !m_projectService || !m_projectService->project())
        return;

    const QImage image = m_resultPreview->currentImage();
    const Selt::ExtendedRoi roi = m_resultPreview->currentExtendedRoi();
    Selt::ProjectResourceStore &store = m_projectService->project()->resources();

    QString root = store.projectRoot();
    if (root.isEmpty()) {
        if (!m_currentFile.isEmpty())
            root = QFileInfo(m_currentFile).absolutePath();
        else
            root = QDir::temp().filePath(QStringLiteral("selt_untitled_project"));
        store.setProjectRoot(root);
    }

    const Selt::TemplateTeachResult result =
        Selt::TemplateTeachService::teachFromRoi(image, roi, store, QStringLiteral("教示模板"));
    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("教示模板失败"), result.error);
        return;
    }

    m_projectService->project()->setModified(true);
    m_runController->setResourceStore(&store);

    // Prefer writing resource id into selected templateMatch / templateSource node.
    QString nodeId = m_propertyPanel->property("currentNodeId").toString();
    const auto isTemplateNode = [](const NodeModel &node) {
        return node.type == VisionNodeIds::templateMatch()
            || node.type == VisionNodeIds::templateSource();
    };
    if (nodeId.isEmpty() || !m_document || !m_document->hasNode(nodeId)
        || !isTemplateNode(m_document->node(nodeId))) {
        if (m_document) {
            for (const NodeModel &node : m_document->nodes()) {
                if (isTemplateNode(node)) {
                    nodeId = node.id;
                    break;
                }
            }
        }
    }
    if (!nodeId.isEmpty() && m_document && m_document->hasNode(nodeId)) {
        NodeModel node = m_document->node(nodeId);
        if (node.parameters.contains(QStringLiteral("templateResourceId"))
            || node.type == VisionNodeIds::templateMatch()
            || node.type == VisionNodeIds::templateSource()) {
            node.parameters.insert(QStringLiteral("templateResourceId"), result.resourceId);
            if (!result.relativePath.isEmpty())
                node.parameters.insert(QStringLiteral("templatePath"), result.relativePath);
            else if (!result.absolutePath.isEmpty())
                node.parameters.insert(QStringLiteral("templatePath"), result.absolutePath);
            m_document->updateNode(node);
            if (m_nodeInspectorDialog && m_nodeInspectorDialog->nodeId() == nodeId)
                m_nodeInspectorDialog->refresh();
            onSelectionNodeChanged(nodeId);
        }
    }

    if (m_runtimeMonitor) {
        m_runtimeMonitor->showDiagnostics(
            {QStringLiteral("已教示模板资源 %1 → %2").arg(result.resourceId, result.relativePath)});
    }
    updateRunStatus(QStringLiteral("模板已教示"), 0);
    QMessageBox::information(this, QStringLiteral("教示模板"),
                             QStringLiteral("模板已保存到项目资源:\n%1\n资源ID: %2")
                                 .arg(result.relativePath, result.resourceId));
}

void MainWindow::runVisionPipeline()
{
    if (!m_runController)
        return;
    if (m_runController->isBusy()) {
        statusBar()->showMessage(QStringLiteral("当前正在运行，请先停止"), 3000);
        return;
    }
    updateRunStatus(QStringLiteral("运行中…"), 1);
    showResultsDock(true);
    m_runController->runOnce();
}

void MainWindow::openCalibrationDialog()
{
    if (!m_calibrationStore)
        return;
    if (m_runController && m_runController->isBusy()) {
        QMessageBox::information(this, QStringLiteral("标定管理"),
                                 QStringLiteral("当前运行使用快照标定；本次修改将在下一次运行生效。"));
    }
    CalibrationDialog dialog(m_calibrationStore, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    QSettings settings(QStringLiteral("Selt"), QStringLiteral("SeltVisionStudio"));
    settings.setValue(QStringLiteral("industrial/calibrations"),
                      QJsonDocument(m_calibrationStore->toJson()).toJson(QJsonDocument::Compact));
    if (m_runController) {
        if (m_calibrationStore->hasActive())
            m_runController->setCalibration(m_calibrationStore->active());
        else
            m_runController->clearCalibration();
    }
    statusBar()->showMessage(QStringLiteral("标定已更新"), 3000);
}

void MainWindow::runPublishValidation()
{
    if (!document())
        return;
    QStringList missing;
    if (m_projectService && m_projectService->project()) {
        m_projectService->project()->resources().refreshExistence();
        missing = m_projectService->project()->resources().missingDiagnostics();
        if (m_resourceBrowser)
            m_resourceBrowser->setStore(&m_projectService->project()->resources());
    }
    const QVector<Selt::GraphDiagnostic> diags =
        Selt::GraphValidator::validateForPublish(*document(), missing);
    for (const Selt::GraphDiagnostic &d : diags) {
        if (d.severity == Selt::GraphDiagnosticSeverity::Warning && !d.nodeId.isEmpty())
            m_scene->setValidationWarning(d.nodeId, true);
    }
    const QStringList lines = Selt::GraphValidator::toMessages(diags);
    const bool hasError = Selt::GraphValidator::hasErrors(diags);
    if (m_runtimeMonitor)
        m_runtimeMonitor->showDiagnostics(lines);
    showResultsDock(true);
    QMessageBox::information(
        this,
        QStringLiteral("发布前校验"),
        hasError
            ? QStringLiteral("校验未通过（%1 条）：\n%2")
                  .arg(lines.size())
                  .arg(lines.mid(0, 12).join(QLatin1Char('\n')))
            : QStringLiteral("校验通过（信息/警告 %1 条）。\n%2")
                  .arg(lines.size())
                  .arg(lines.mid(0, 8).join(QLatin1Char('\n'))));
}

void MainWindow::startVisionLoop()
{
    if (!m_runController)
        return;
    if (!m_runController->startLoop(0)) {
        statusBar()->showMessage(QStringLiteral("无法启动连续运行（可能已在运行）"), 3000);
        updateRunStatus(QStringLiteral("空闲"), 0);
        return;
    }
    updateRunStatus(QStringLiteral("连续运行中"), 1);
    showResultsDock(true);
    statusBar()->showMessage(QStringLiteral("连续运行中… 按 F8 停止"), 0);
}

void MainWindow::stopVisionRun()
{
    if (m_runController)
        m_runController->stop();
    updateRunStatus(QStringLiteral("已停止"), 0);
    updateRunActionsEnabled();
    statusBar()->showMessage(QStringLiteral("已停止"), 3000);
}

void MainWindow::onRunFinished(bool ok, const VisionContext &context)
{
    if (m_runtimeMonitor)
        m_runtimeMonitor->showExecution(context);
    applyVisionContext(context, !ok);
    showResultsDock(true);
    updateRunActionsEnabled();
    if (!ok) {
        updateRunStatus(QStringLiteral("失败"), 3);
        if (!context.failedNodeId.isEmpty() && m_scene) {
            if (NodeGraphicsItem *item = m_scene->nodeItem(context.failedNodeId)) {
                m_scene->clearSelection();
                item->setSelected(true);
                m_view->centerOn(item);
                onSelectionNodeChanged(context.failedNodeId);
            }
            statusBar()->showMessage(
                QStringLiteral("已定位到失败节点: %1").arg(context.failedNodeId), 5000);
        }
        QMessageBox::warning(this, QStringLiteral("视觉流程执行失败"), context.errorMessage);
        return;
    }
    updateRunStatus(QStringLiteral("成功"), 2);
    statusBar()->showMessage(QStringLiteral("执行完成 (%1 ms)").arg(context.elapsedMs), 5000);
}

void MainWindow::insertVisionDemoFlow()
{
    insertVisionWorkflowTemplate(QStringLiteral("measure_basic"));
}

void MainWindow::insertVisionWorkflowTemplate(const QString &templateId)
{
    if (!m_document || !m_undoStack)
        return;

    const Selt::WorkflowTemplateSpec spec =
        Selt::WorkflowTemplates::build(templateId, QPointF(40, 40));
    m_undoStack->beginMacro(QStringLiteral("插入流程模板: %1").arg(spec.displayName));
    for (const NodeModel &node : spec.nodes)
        m_undoStack->push(new AddNodeCommand(m_document, node));
    for (const ConnectionModel &conn : spec.connections)
        m_undoStack->push(new AddConnectionCommand(m_document, conn));
    m_undoStack->endMacro();

    statusBar()->showMessage(
        QStringLiteral("已插入「%1」。%2")
            .arg(spec.displayName, spec.description),
        6000);
}

void MainWindow::clearVisionResults()
{
    m_hasVisionResult = false;
    m_lastVisionContext = VisionContext{};
    m_originalPreview->clearImage();
    m_resultPreview->clearImage();
    m_measurementPanel->clearResults();
    if (m_runtimeMonitor)
        m_runtimeMonitor->clearAll();
    clearNodeRunStatuses();
    const QString selectedId = m_propertyPanel
        ? m_propertyPanel->property("currentNodeId").toString()
        : QString();
    syncSelectedModuleStatus(selectedId);
    if (m_helpLabel && selectedId.isEmpty())
        m_helpLabel->setText(QStringLiteral("选中画布节点后，此处显示模块说明。"));
    updateRunStatus(QStringLiteral("空闲"), 0);
}

void MainWindow::showResultsDock(bool visible)
{
    if (m_visionDock) {
        m_visionDock->setVisible(visible);
        if (m_toggleResultsDockAction)
            m_toggleResultsDockAction->setChecked(visible);
        return;
    }
    if (!m_bottomDock)
        return;
    m_bottomDock->setVisible(visible);
    if (m_toggleResultsDockAction)
        m_toggleResultsDockAction->setChecked(visible);
}
#else
void MainWindow::runVisionPipeline()
{
    QMessageBox::information(this, QStringLiteral("视觉功能未启用"),
                             QStringLiteral("未找到 MinGW 版 OpenCV。\n"
                                            "请编译 OpenCV 后重新配置 CMake，或设置 SELT_ENABLE_VISION=ON。"));
}

void MainWindow::startVisionLoop()
{
    runVisionPipeline();
}

void MainWindow::stopVisionRun()
{
}

void MainWindow::insertVisionDemoFlow()
{
    runVisionPipeline();
}

void MainWindow::insertVisionWorkflowTemplate(const QString &)
{
    runVisionPipeline();
}

void MainWindow::clearVisionResults()
{
}
#endif
