#include "ui/mainwindow.h"
#include "ui_mainwindow.h"

#include "core/model/document.h"
#include "core/serialization/documentserializer.h"
#include "graphics/canvas/canvasscene.h"
#include "graphics/canvas/canvasview.h"
#include "ui/nodepalette.h"
#include "ui/propertypanel.h"
#if SELT_HAS_OPENCV
#include "ui/imagepreviewwidget.h"
#include "ui/measurementpanel.h"
#endif

#include "app/nodebuilder.h"
#include "core/command/documentcommands.h"
#if SELT_HAS_OPENCV
#include "vision/execution/visionpipeline.h"
#include "vision/registry/visionnodeids.h"
#endif

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>
#include <QVBoxLayout>
#if SELT_HAS_OPENCV
#include "graphics/items/nodegraphicsitem.h"
#include "vision/model/modulestatus.h"
#include "vision/registry/visionnoderegistry.h"
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    resize(1280, 800);
    setWindowTitle(QStringLiteral("Selt 视觉模块化流程编辑器"));

    m_document = new Document(this);
    m_undoStack = new QUndoStack(this);
    m_scene = new CanvasScene(m_document, m_undoStack, this);
    m_view = new CanvasView(m_scene, this);

    setupUiExtra();
    setupActions();
    setupMenus();
    setupToolBar();
    setupDocks();
    setupConnections();

    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(60 * 1000);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSave);
    m_autoSaveTimer->start();

    updateWindowTitle();
    updateRecentFilesMenu();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave())
        event->accept();
    else
        event->ignore();
}

void MainWindow::setupUiExtra()
{
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);
    setCentralWidget(central);

    m_zoomLabel = new QLabel(QStringLiteral("缩放 100%"), this);
    m_posLabel = new QLabel(QStringLiteral("坐标 (0, 0)"), this);
    statusBar()->addPermanentWidget(m_posLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);
}

void MainWindow::setupActions()
{
    // Actions created in menus/toolbar below
}

void MainWindow::setupMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    fileMenu->addAction(QStringLiteral("新建"), QKeySequence::New, this, &MainWindow::newDocument);
    fileMenu->addAction(QStringLiteral("打开..."), QKeySequence::Open, this, &MainWindow::openDocument);
    m_recentMenu = fileMenu->addMenu(QStringLiteral("最近打开"));
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("保存"), QKeySequence::Save, this, &MainWindow::saveDocument);
    fileMenu->addAction(QStringLiteral("另存为..."), QKeySequence::SaveAs, this, &MainWindow::saveDocumentAs);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("退出"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *editMenu = menuBar()->addMenu(QStringLiteral("编辑(&E)"));
    QAction *undoAction = m_undoStack->createUndoAction(this, QStringLiteral("撤销"));
    undoAction->setShortcut(QKeySequence::Undo);
    QAction *redoAction = m_undoStack->createRedoAction(this, QStringLiteral("重做"));
    redoAction->setShortcut(QKeySequence::Redo);
    editMenu->addAction(undoAction);
    editMenu->addAction(redoAction);
    editMenu->addSeparator();
    editMenu->addAction(QStringLiteral("全选"), QKeySequence::SelectAll, m_scene, &CanvasScene::selectAll);
    editMenu->addAction(QStringLiteral("删除"), QKeySequence::Delete, m_scene, &CanvasScene::deleteSelection);
    editMenu->addAction(QStringLiteral("复制"), QKeySequence::Copy, m_scene, &CanvasScene::copySelection);
    editMenu->addAction(QStringLiteral("粘贴"), QKeySequence::Paste, this, [this]() {
        m_scene->pasteClipboard(m_view->mapToScene(m_view->viewport()->rect().center()));
    });

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
    viewMenu->addAction(QStringLiteral("放大"), QKeySequence::ZoomIn, m_view, &CanvasView::zoomIn);
    viewMenu->addAction(QStringLiteral("缩小"), QKeySequence::ZoomOut, m_view, &CanvasView::zoomOut);
    viewMenu->addAction(QStringLiteral("实际大小"), this, [this]() { m_view->resetZoom(); });
    viewMenu->addAction(QStringLiteral("适应全部"), this, [this]() { m_view->fitAll(); });
    viewMenu->addSeparator();
    QAction *snapAction = viewMenu->addAction(QStringLiteral("网格吸附"));
    snapAction->setCheckable(true);
    snapAction->setChecked(true);
    connect(snapAction, &QAction::toggled, this, &MainWindow::toggleSnap);
    QAction *gridAction = viewMenu->addAction(QStringLiteral("显示网格"));
    gridAction->setCheckable(true);
    gridAction->setChecked(true);
    connect(gridAction, &QAction::toggled, this, &MainWindow::toggleGrid);

#if SELT_HAS_OPENCV
    QMenu *visionMenu = menuBar()->addMenu(QStringLiteral("执行(&R)"));
    visionMenu->addAction(QStringLiteral("单次执行"), QKeySequence(Qt::Key_F5), this, &MainWindow::runVisionPipeline);
    visionMenu->addAction(QStringLiteral("插入示例流程"), this, &MainWindow::insertVisionDemoFlow);
    visionMenu->addAction(QStringLiteral("清空结果"), this, &MainWindow::clearVisionResults);
#endif

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    helpMenu->addAction(QStringLiteral("关于"), this, [this]() {
        QMessageBox::about(this, QStringLiteral("关于"),
                           QStringLiteral("Selt 视觉模块化流程编辑器\n"
                                          "拖拽视觉工具搭建流程，配置模块参数，单次执行并查看图像/测量结果。"));
    });
}

void MainWindow::setupToolBar()
{
    QToolBar *tb = addToolBar(QStringLiteral("主工具栏"));
    tb->addAction(QStringLiteral("新建"), this, &MainWindow::newDocument);
    tb->addAction(QStringLiteral("打开"), this, &MainWindow::openDocument);
    tb->addAction(QStringLiteral("保存"), this, &MainWindow::saveDocument);
    tb->addSeparator();
    tb->addAction(m_undoStack->createUndoAction(this, QStringLiteral("撤销")));
    tb->addAction(m_undoStack->createRedoAction(this, QStringLiteral("重做")));
    tb->addSeparator();
    tb->addAction(QStringLiteral("放大"), m_view, &CanvasView::zoomIn);
    tb->addAction(QStringLiteral("缩小"), m_view, &CanvasView::zoomOut);
    tb->addAction(QStringLiteral("适应"), m_view, &CanvasView::fitAll);
#if SELT_HAS_OPENCV
    tb->addSeparator();
    tb->addAction(QStringLiteral("单次执行"), this, &MainWindow::runVisionPipeline);
#endif
}

void MainWindow::setupDocks()
{
    m_palette = new NodePalette(this);
    auto *leftDock = new QDockWidget(QStringLiteral("视觉工具箱"), this);
    leftDock->setObjectName(QStringLiteral("NodePaletteDock"));
    leftDock->setWidget(m_palette);
    addDockWidget(Qt::LeftDockWidgetArea, leftDock);

    m_propertyPanel = new PropertyPanel(this);
    m_propertyPanel->setDocument(m_document);
    m_propertyPanel->setUndoStack(m_undoStack);
    auto *rightDock = new QDockWidget(QStringLiteral("模块参数"), this);
    rightDock->setObjectName(QStringLiteral("PropertyDock"));
    rightDock->setWidget(m_propertyPanel);
    addDockWidget(Qt::RightDockWidgetArea, rightDock);

#if SELT_HAS_OPENCV
    m_originalPreview = new ImagePreviewWidget(this);
    m_resultPreview = new ImagePreviewWidget(this);
    m_resultPreview->setRoiEditEnabled(true);
    m_originalPreview->setRoiEditEnabled(false);
    auto *previewTabs = new QTabWidget(this);
    previewTabs->addTab(m_originalPreview, QStringLiteral("原图"));
    previewTabs->addTab(m_resultPreview, QStringLiteral("结果/模块输出"));
    auto *bottomDock = new QDockWidget(QStringLiteral("视觉结果"), this);
    bottomDock->setObjectName(QStringLiteral("VisionResultDock"));
    auto *bottomWidget = new QWidget(bottomDock);
    auto *bottomLayout = new QHBoxLayout(bottomWidget);
    m_measurementPanel = new MeasurementPanel(bottomWidget);
    bottomLayout->addWidget(previewTabs, 2);
    bottomLayout->addWidget(m_measurementPanel, 1);
    bottomDock->setWidget(bottomWidget);
    addDockWidget(Qt::BottomDockWidgetArea, bottomDock);
#endif
}

void MainWindow::setupConnections()
{
    connect(m_document, &Document::modifiedChanged, this, [this](bool) { updateWindowTitle(); });
    connect(m_document, &Document::titleChanged, this, [this](const QString &) { updateWindowTitle(); });
    connect(m_view, &CanvasView::zoomChanged, this, &MainWindow::updateStatusZoom);
    connect(m_view, &CanvasView::mouseScenePosChanged, this, &MainWindow::updateStatusPos);
    connect(m_scene, &CanvasScene::selectionNodeChanged, this, &MainWindow::onSelectionNodeChanged);
    connect(m_scene, &CanvasScene::statusMessage, this, [this](const QString &msg) {
        statusBar()->showMessage(msg, 3000);
    });
    connect(m_palette, &NodePalette::nodeTypeActivated, this, &MainWindow::onPaletteActivated);
    connect(m_document, &Document::nodeUpdated, this, [this](const NodeModel &node) {
        if (node.id != m_propertyPanel->property("currentNodeId").toString())
            return;
        // 参数面板写回过程中禁止同步重建，避免销毁正在响应的 ROI「启用」复选框
        if (m_propertyPanel->isApplyingChanges())
            return;
        m_propertyPanel->setSelectedNode(node.id);
#if SELT_HAS_OPENCV
        syncSelectedModuleStatus(node.id);
#endif
    });
#if SELT_HAS_OPENCV
    connect(m_measurementPanel, &MeasurementPanel::outputNodeChanged,
            this, &MainWindow::onVisionOutputNodeChanged);
    connect(m_resultPreview, &ImagePreviewWidget::roiChanged,
            this, &MainWindow::onResultRoiChanged);
#endif
}

void MainWindow::newDocument()
{
    if (!maybeSave())
        return;
    m_undoStack->clear();
    m_document->clear();
    setCurrentFile(QString());
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
    if (!m_document->isModified())
        return true;
    const auto ret = QMessageBox::warning(
        this, QStringLiteral("未保存的更改"),
        QStringLiteral("文档已修改，是否保存？"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (ret == QMessageBox::Save) {
        saveDocument();
        return !m_document->isModified();
    }
    return ret == QMessageBox::Discard;
}

bool MainWindow::saveToPath(const QString &path)
{
    QString error;
    if (!DocumentSerializer::saveToFile(*m_document, path, &error)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), error);
        return false;
    }
    m_document->markClean();
    setCurrentFile(path);
    addRecentFile(path);
    statusBar()->showMessage(QStringLiteral("已保存: %1").arg(path), 3000);
    return true;
}

bool MainWindow::loadFromPath(const QString &path)
{
    QString error;
    Document temp;
    if (!DocumentSerializer::loadFromFile(temp, path, &error)) {
        QMessageBox::critical(this, QStringLiteral("打开失败"), error);
        return false;
    }

    m_undoStack->clear();
    m_document->replaceAll(temp.title(), temp.settings(), temp.nodes(), temp.connections());
    setCurrentFile(path);
    addRecentFile(path);
    statusBar()->showMessage(QStringLiteral("已打开: %1").arg(path), 3000);
    return true;
}

void MainWindow::setCurrentFile(const QString &path)
{
    m_currentFile = path;
    if (!path.isEmpty()) {
        const QString base = QFileInfo(path).completeBaseName();
        if (m_document->title() != base) {
            // Avoid dirty flag from incidental title sync after load/save.
            const bool wasModified = m_document->isModified();
            m_document->setTitle(base);
            if (!wasModified)
                m_document->markClean();
        }
    }
    updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    QString title = m_document->title();
    if (!m_currentFile.isEmpty())
        title += QStringLiteral(" - ") + m_currentFile;
    if (m_document->isModified())
        title += QStringLiteral(" *");
    title += QStringLiteral(" - Selt");
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
    const QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    m_scene->createNodeAt(type, center);
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
    if (!m_document->isModified())
        return;
    const QString autosavePath = m_currentFile.isEmpty()
        ? QStringLiteral("autosave.selt.tmp")
        : (m_currentFile + QStringLiteral(".tmp"));
    QString error;
    DocumentSerializer::saveToFile(*m_document, autosavePath, &error);
}

#if SELT_HAS_OPENCV
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
            continue;
        }
        switch (context.moduleResult(node.id).status) {
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
}

void MainWindow::clearNodeRunStatuses()
{
    for (const NodeModel &node : m_document->nodes()) {
        if (auto *item = m_scene->nodeItem(node.id))
            item->setRunStatus(NodeRunVisualStatus::Idle);
    }
}

void MainWindow::syncSelectedModuleStatus(const QString &nodeId)
{
    if (!m_hasVisionResult || nodeId.isEmpty() || !m_lastVisionContext.moduleResults.contains(nodeId)) {
        m_propertyPanel->setModuleStatusText(QStringLiteral("未执行"));
        return;
    }
    const ModuleRunResult result = m_lastVisionContext.moduleResult(nodeId);
    QString text = moduleStatusToString(result.status);
    if (result.elapsedMs > 0)
        text += QStringLiteral(" (%1 ms)").arg(result.elapsedMs);
    if (!result.errorMessage.isEmpty())
        text += QStringLiteral(" - %1").arg(result.errorMessage);
    m_propertyPanel->setModuleStatusText(text);
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

void MainWindow::runVisionPipeline()
{
    VisionContext context;
    const bool ok = VisionPipeline::execute(*m_document, context, ExecutionMode::Once);
    applyVisionContext(context, !ok);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("视觉流程执行失败"), context.errorMessage);
        return;
    }
    statusBar()->showMessage(QStringLiteral("单次执行完成 (%1 ms)").arg(context.elapsedMs), 5000);
}

void MainWindow::insertVisionDemoFlow()
{
    if (!m_document || !m_undoStack)
        return;

    m_undoStack->beginMacro(QStringLiteral("插入示例视觉流程"));
    const QPointF base(0, 0);
    NodeModel loader = NodeBuilder::create(VisionNodeIds::imageLoader(), base);
    NodeModel gray = NodeBuilder::create(VisionNodeIds::grayscale(), base + QPointF(220, 0));
    NodeModel threshold = NodeBuilder::create(VisionNodeIds::threshold(), base + QPointF(440, 0));
    NodeModel measure = NodeBuilder::create(VisionNodeIds::rectangleMeasure(), base + QPointF(660, 0));

    m_undoStack->push(new AddNodeCommand(m_document, loader));
    m_undoStack->push(new AddNodeCommand(m_document, gray));
    m_undoStack->push(new AddNodeCommand(m_document, threshold));
    m_undoStack->push(new AddNodeCommand(m_document, measure));

    ConnectionModel c1;
    c1.id = Document::createId(QStringLiteral("conn"));
    c1.sourceNodeId = loader.id;
    c1.sourcePortId = QStringLiteral("out");
    c1.targetNodeId = gray.id;
    c1.targetPortId = QStringLiteral("in");
    m_undoStack->push(new AddConnectionCommand(m_document, c1));

    ConnectionModel c2 = c1;
    c2.id = Document::createId(QStringLiteral("conn"));
    c2.sourceNodeId = gray.id;
    c2.targetNodeId = threshold.id;
    m_undoStack->push(new AddConnectionCommand(m_document, c2));

    ConnectionModel c3 = c1;
    c3.id = Document::createId(QStringLiteral("conn"));
    c3.sourceNodeId = threshold.id;
    c3.targetNodeId = measure.id;
    m_undoStack->push(new AddConnectionCommand(m_document, c3));
    m_undoStack->endMacro();

    statusBar()->showMessage(QStringLiteral("已插入示例视觉流程，请在图片输入节点中设置图片路径"), 5000);
}

void MainWindow::clearVisionResults()
{
    m_hasVisionResult = false;
    m_lastVisionContext = VisionContext{};
    m_originalPreview->clearImage();
    m_resultPreview->clearImage();
    m_measurementPanel->clearResults();
    clearNodeRunStatuses();
    syncSelectedModuleStatus(m_propertyPanel->property("currentNodeId").toString());
}
#else
void MainWindow::runVisionPipeline()
{
    QMessageBox::information(this, QStringLiteral("视觉功能未启用"),
                             QStringLiteral("未找到 MinGW 版 OpenCV。\n"
                                            "请编译 OpenCV 后重新配置 CMake，或设置 SELT_ENABLE_VISION=ON。"));
}

void MainWindow::insertVisionDemoFlow()
{
    runVisionPipeline();
}

void MainWindow::clearVisionResults()
{
}
#endif
