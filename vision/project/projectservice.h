#ifndef PROJECTSERVICE_H
#define PROJECTSERVICE_H

#include "core/model/document.h"
#include "vision/domain/visionproject.h"
#include "vision/storage/projectcontainer.h"
#include "vision/storage/projectstorage.h"

#include <QHash>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QUndoGroup>
#include <QUndoStack>

namespace Selt {

struct FlowViewportState
{
    qreal zoom{1.0};
    QPointF center;
    int hScroll{0};
    int vScroll{0};
    bool valid{false};
};

/// Application-layer coordinator for VisionProject + Document canvas adapter.
/// Not a global singleton; owned by MainWindow (or tests).
class ProjectService : public QObject
{
    Q_OBJECT
public:
    explicit ProjectService(QObject *parent = nullptr);

    VisionProject *project() const { return m_project; }
    Document *document() const { return m_document; }
    QUndoGroup *undoGroup() const { return m_undoGroup; }
    QUndoStack *activeUndoStack() const;

    QString currentFilePath() const { return m_filePath; }
    void setCurrentFilePath(const QString &path);

    bool isModified() const;
    QString windowTitleBase() const;

    void newProject();
    bool openLegacySelt(const QString &path, MigrationReport *report = nullptr);
    bool saveLegacySelt(const QString &path, QString *error = nullptr);
    bool openContainer(const QString &path, ProjectContainerReport *report = nullptr);
    bool saveContainer(const QString &path, QString *error = nullptr);

    VisionFlow *createFlow(const QString &name = QStringLiteral("流程"));
    bool renameFlow(const QString &flowId, const QString &name);
    bool removeFlow(const QString &flowId);
    bool setActiveFlow(const QString &flowId);

    void syncDocumentToActiveFlow();
    void syncActiveFlowToDocument();

    FlowViewportState viewportState(const QString &flowId) const;
    void setViewportState(const QString &flowId, const FlowViewportState &state);

signals:
    void projectReset();
    void titleChanged(const QString &title);
    void modifiedChanged(bool modified);
    void flowListChanged();
    void activeFlowChanged(const QString &flowId);
    void undoStackChanged(QUndoStack *stack);
    void statusMessage(const QString &message);
    void filePathChanged(const QString &path);

private:
    class SyncGuard
    {
    public:
        explicit SyncGuard(ProjectService *service)
            : m_service(service)
        {
            ++m_service->m_syncDepth;
        }
        ~SyncGuard() { --m_service->m_syncDepth; }

    private:
        ProjectService *m_service;
    };

    void connectProjectSignals();
    void connectDocumentSignals();
    QUndoStack *ensureUndoStack(const QString &flowId);
    void activateUndoStack(const QString &flowId);
    void clearUndoStacks();
    void emitModified();

    VisionProject *m_project{nullptr};
    Document *m_document{nullptr};
    QUndoGroup *m_undoGroup{nullptr};
    QHash<QString, QUndoStack *> m_undoStacks;
    QHash<QString, FlowViewportState> m_viewports;
    QString m_filePath;
    int m_syncDepth{0};
};

} // namespace Selt

#endif // PROJECTSERVICE_H
