#include "ui/mainwindow.h"
#include "ui_mainwindow.h"

#include "core/model/document.h"
#include "core/registry/nodefactory.h"
#include "core/serialization/documentserializer.h"
#include "graphics/canvas/canvasscene.h"
#include "graphics/canvas/canvasview.h"
#include "ui/nodepalette.h"
#include "ui/propertypanel.h"

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    resize(1280, 800);
    setWindowTitle(QStringLiteral("Selt 节点画布编辑器"));

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

    QMenu *alignMenu = menuBar()->addMenu(QStringLiteral("对齐(&A)"));
    alignMenu->addAction(QStringLiteral("左对齐"), this, [this]() { m_scene->alignSelection(Qt::AlignLeft); });
    alignMenu->addAction(QStringLiteral("右对齐"), this, [this]() { m_scene->alignSelection(Qt::AlignRight); });
    alignMenu->addAction(QStringLiteral("顶对齐"), this, [this]() { m_scene->alignSelection(Qt::AlignTop); });
    alignMenu->addAction(QStringLiteral("底对齐"), this, [this]() { m_scene->alignSelection(Qt::AlignBottom); });
    alignMenu->addAction(QStringLiteral("水平居中"), this, [this]() { m_scene->alignSelection(Qt::AlignHCenter); });
    alignMenu->addAction(QStringLiteral("垂直居中"), this, [this]() { m_scene->alignSelection(Qt::AlignVCenter); });
    alignMenu->addSeparator();
    alignMenu->addAction(QStringLiteral("水平分布"), this, [this]() { m_scene->distributeSelection(Qt::Horizontal); });
    alignMenu->addAction(QStringLiteral("垂直分布"), this, [this]() { m_scene->distributeSelection(Qt::Vertical); });
    alignMenu->addSeparator();
    alignMenu->addAction(QStringLiteral("置于顶层"), this, [this]() { m_scene->bringSelectionForward(true); });
    alignMenu->addAction(QStringLiteral("上移一层"), this, [this]() { m_scene->bringSelectionForward(false); });
    alignMenu->addAction(QStringLiteral("下移一层"), this, [this]() { m_scene->sendSelectionBackward(false); });
    alignMenu->addAction(QStringLiteral("置于底层"), this, [this]() { m_scene->sendSelectionBackward(true); });
    alignMenu->addSeparator();
    alignMenu->addAction(QStringLiteral("组合"), this, [this]() { m_scene->groupSelection(); });
    alignMenu->addAction(QStringLiteral("取消组合"), this, [this]() { m_scene->ungroupSelection(); });

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    helpMenu->addAction(QStringLiteral("关于"), this, [this]() {
        QMessageBox::about(this, QStringLiteral("关于"),
                           QStringLiteral("Selt 节点画布编辑器\n支持拖拽节点、连线、缩放、属性编辑与 JSON 工程文件。"));
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
}

void MainWindow::setupDocks()
{
    m_palette = new NodePalette(this);
    auto *leftDock = new QDockWidget(QStringLiteral("节点工具箱"), this);
    leftDock->setObjectName(QStringLiteral("NodePaletteDock"));
    leftDock->setWidget(m_palette);
    addDockWidget(Qt::LeftDockWidgetArea, leftDock);

    m_propertyPanel = new PropertyPanel(this);
    m_propertyPanel->setDocument(m_document);
    m_propertyPanel->setUndoStack(m_undoStack);
    auto *rightDock = new QDockWidget(QStringLiteral("属性"), this);
    rightDock->setObjectName(QStringLiteral("PropertyDock"));
    rightDock->setWidget(m_propertyPanel);
    addDockWidget(Qt::RightDockWidgetArea, rightDock);
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
        if (node.id == m_propertyPanel->property("currentNodeId").toString())
            m_propertyPanel->setSelectedNode(node.id);
    });
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
