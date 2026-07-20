#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

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
class QUndoStack;
class QLabel;
class QTimer;
class QSettings;

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

    Ui::MainWindow *ui{nullptr};
    Document *m_document{nullptr};
    QUndoStack *m_undoStack{nullptr};
    CanvasScene *m_scene{nullptr};
    CanvasView *m_view{nullptr};
    NodePalette *m_palette{nullptr};
    PropertyPanel *m_propertyPanel{nullptr};
    QLabel *m_zoomLabel{nullptr};
    QLabel *m_posLabel{nullptr};
    QString m_currentFile;
    QMenu *m_recentMenu{nullptr};
    QTimer *m_autoSaveTimer{nullptr};
};

#endif // MAINWINDOW_H
