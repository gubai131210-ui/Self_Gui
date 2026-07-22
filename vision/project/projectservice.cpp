#include "vision/project/projectservice.h"

#include "core/serialization/documentserializer.h"
#include "vision/registry/visionnoderegistry.h"

namespace Selt {

ProjectService::ProjectService(QObject *parent)
    : QObject(parent)
    , m_project(new VisionProject(this))
    , m_document(new Document(this))
    , m_undoGroup(new QUndoGroup(this))
{
    connectProjectSignals();
    connectDocumentSignals();
    syncActiveFlowToDocument();
    activateUndoStack(m_project->activeFlowId());
    m_project->markClean();
    m_document->markClean();
}

QUndoStack *ProjectService::activeUndoStack() const
{
    return m_undoGroup->activeStack();
}

void ProjectService::setCurrentFilePath(const QString &path)
{
    if (m_filePath == path)
        return;
    m_filePath = path;
    emit filePathChanged(m_filePath);
}

bool ProjectService::isModified() const
{
    return m_project->isModified() || m_document->isModified();
}

QString ProjectService::windowTitleBase() const
{
    const QString projectTitle = m_project->title();
    VisionFlow *flow = m_project->activeFlow();
    const QString flowName = flow ? flow->name() : QString();
    if (flowName.isEmpty())
        return projectTitle;
    return QStringLiteral("%1 — %2").arg(projectTitle, flowName);
}

void ProjectService::newProject()
{
    SyncGuard guard(this);
    clearUndoStacks();
    m_viewports.clear();
    m_filePath.clear();

    // Recreate a clean project domain object.
    m_project->deleteLater();
    m_project = new VisionProject(this);
    connectProjectSignals();

    syncActiveFlowToDocument();
    activateUndoStack(m_project->activeFlowId());
    m_project->markClean();
    m_document->markClean();

    emit filePathChanged(m_filePath);
    emit projectReset();
    emit flowListChanged();
    emit activeFlowChanged(m_project->activeFlowId());
    emit titleChanged(m_project->title());
    emitModified();
}

bool ProjectService::openLegacySelt(const QString &path, MigrationReport *report)
{
    MigrationReport localReport;
    MigrationReport *out = report ? report : &localReport;

    auto *loaded = new VisionProject(this);
    if (!ProjectStorage::loadLegacySelt(path, *loaded, out)) {
        loaded->deleteLater();
        return false;
    }

    SyncGuard guard(this);
    clearUndoStacks();
    m_viewports.clear();
    m_project->deleteLater();
    m_project = loaded;
    connectProjectSignals();
    m_filePath = path;

    syncActiveFlowToDocument();
    activateUndoStack(m_project->activeFlowId());
    m_project->markClean();
    m_document->markClean();

    emit filePathChanged(m_filePath);
    emit projectReset();
    emit flowListChanged();
    emit activeFlowChanged(m_project->activeFlowId());
    emit titleChanged(m_project->title());
    emitModified();

    if (!out->notes.isEmpty())
        emit statusMessage(out->notes.join(QStringLiteral("; ")));
    return true;
}

bool ProjectService::openContainer(const QString &path, ProjectContainerReport *report)
{
    auto *loaded = new VisionProject(this);
    if (!ProjectStorage::loadContainer(path, *loaded, report)) {
        loaded->deleteLater();
        return false;
    }
    SyncGuard guard(this);
    clearUndoStacks();
    m_viewports.clear();
    m_project->deleteLater();
    m_project = loaded;
    connectProjectSignals();
    m_filePath = path;
    syncActiveFlowToDocument();
    activateUndoStack(m_project->activeFlowId());
    m_project->markClean();
    m_document->markClean();
    emit filePathChanged(m_filePath);
    emit projectReset();
    emit flowListChanged();
    emit activeFlowChanged(m_project->activeFlowId());
    emit titleChanged(m_project->title());
    emitModified();
    return true;
}

bool ProjectService::saveLegacySelt(const QString &path, QString *error)
{
    syncDocumentToActiveFlow();
    {
        SyncGuard guard(this);
        if (m_document->title() != m_project->title())
            m_document->setTitle(m_project->title());
    }

    // Prefer Document adapter (keeps canvas settings) and inject project variables.
    if (!ProjectStorage::saveDocumentWithVariables(path, *m_document, m_project->variables(), error))
        return false;

    m_filePath = path;
    m_project->markClean();
    m_document->markClean();
    emit filePathChanged(m_filePath);
    emitModified();
    return true;
}

bool ProjectService::saveContainer(const QString &path, QString *error)
{
    syncDocumentToActiveFlow();
    if (!ProjectStorage::saveContainer(path, *m_project, error))
        return false;
    m_filePath = path;
    m_project->markClean();
    m_document->markClean();
    emit filePathChanged(m_filePath);
    emitModified();
    return true;
}

VisionFlow *ProjectService::createFlow(const QString &name)
{
    syncDocumentToActiveFlow();
    VisionFlow *flow = m_project->createFlow(name);
    if (!flow)
        return nullptr;
    ensureUndoStack(flow->flowId());
    setActiveFlow(flow->flowId());
    return flow;
}

bool ProjectService::renameFlow(const QString &flowId, const QString &name)
{
    if (!m_project->renameFlow(flowId, name))
        return false;
    emit titleChanged(m_project->title());
    return true;
}

bool ProjectService::removeFlow(const QString &flowId)
{
    if (m_project->flowCount() <= 1)
        return false;

    syncDocumentToActiveFlow();
    const QString previousActive = m_project->activeFlowId();
    if (!m_project->removeFlow(flowId))
        return false;

    if (QUndoStack *stack = m_undoStacks.take(flowId)) {
        m_undoGroup->removeStack(stack);
        stack->deleteLater();
    }
    m_viewports.remove(flowId);

    const QString newActive = m_project->activeFlowId();
    if (previousActive != newActive) {
        syncActiveFlowToDocument();
        activateUndoStack(newActive);
        emit activeFlowChanged(newActive);
        emit statusMessage(QStringLiteral("已切换到流程「%1」")
                               .arg(m_project->activeFlow() ? m_project->activeFlow()->name() : QString()));
    }
    emit titleChanged(m_project->title());
    return true;
}

bool ProjectService::setActiveFlow(const QString &flowId)
{
    if (!m_project->flow(flowId))
        return false;
    if (m_project->activeFlowId() == flowId)
        return true;

    syncDocumentToActiveFlow();
    if (!m_project->setActiveFlow(flowId))
        return false;

    syncActiveFlowToDocument();
    activateUndoStack(flowId);
    emit activeFlowChanged(flowId);
    emit titleChanged(m_project->title());
    emit statusMessage(QStringLiteral("已切换流程；各流程撤销历史相互独立"));
    return true;
}

void ProjectService::syncDocumentToActiveFlow()
{
    if (m_syncDepth > 0)
        return;
    VisionFlow *flow = m_project->activeFlow();
    if (!flow)
        return;

    QVector<NodeModel> nodes = m_document->nodes();
    QVector<ConnectionModel> connections = m_document->connections();
    if (VisionNodeRegistry::upgradeGraph(nodes, connections)) {
        SyncGuard guard(this);
        m_document->replaceAll(m_document->title(), m_document->settings(), nodes, connections);
    }

    SyncGuard guard(this);
    QString error;
    if (!flow->replaceGraph(m_document->nodes(), m_document->connections(), &error)) {
        emit statusMessage(QStringLiteral("同步到流程失败: %1").arg(error));
        return;
    }
    // Keep project title as source of truth; mirror into document title when empty mismatch.
    if (m_document->title() != m_project->title()) {
        // Avoid bouncing: update document under guard without re-sync.
        m_document->setTitle(m_project->title());
        m_document->markClean();
    }
}

void ProjectService::syncActiveFlowToDocument()
{
    VisionFlow *flow = m_project->activeFlow();
    if (!flow)
        return;

    SyncGuard guard(this);
    const DocumentSettings settings = m_document->settings();
    m_document->replaceAll(m_project->title(), settings, flow->nodes(), flow->connections());
}

FlowViewportState ProjectService::viewportState(const QString &flowId) const
{
    return m_viewports.value(flowId);
}

void ProjectService::setViewportState(const QString &flowId, const FlowViewportState &state)
{
    m_viewports.insert(flowId, state);
}

void ProjectService::connectProjectSignals()
{
    connect(m_project, &VisionProject::titleChanged, this, [this](const QString &title) {
        emit titleChanged(title);
        if (m_syncDepth == 0) {
            SyncGuard guard(this);
            if (m_document->title() != title)
                m_document->setTitle(title);
        }
        emitModified();
    });
    connect(m_project, &VisionProject::modifiedChanged, this, [this](bool) { emitModified(); });
    connect(m_project, &VisionProject::flowListChanged, this, &ProjectService::flowListChanged);
    connect(m_project, &VisionProject::activeFlowChanged, this, [this](const QString &id) {
        Q_UNUSED(id);
        emit titleChanged(m_project->title());
    });
}

void ProjectService::connectDocumentSignals()
{
    auto scheduleSync = [this]() {
        if (m_syncDepth > 0)
            return;
        syncDocumentToActiveFlow();
        emitModified();
    };

    connect(m_document, &Document::nodeAdded, this, scheduleSync);
    connect(m_document, &Document::nodeRemoved, this, scheduleSync);
    connect(m_document, &Document::nodeUpdated, this, scheduleSync);
    connect(m_document, &Document::connectionAdded, this, scheduleSync);
    connect(m_document, &Document::connectionRemoved, this, scheduleSync);
    connect(m_document, &Document::connectionUpdated, this, scheduleSync);
    connect(m_document, &Document::modifiedChanged, this, [this](bool) { emitModified(); });
}

QUndoStack *ProjectService::ensureUndoStack(const QString &flowId)
{
    if (flowId.isEmpty())
        return nullptr;
    if (QUndoStack *existing = m_undoStacks.value(flowId, nullptr))
        return existing;

    auto *stack = new QUndoStack(this);
    m_undoStacks.insert(flowId, stack);
    m_undoGroup->addStack(stack);
    return stack;
}

void ProjectService::activateUndoStack(const QString &flowId)
{
    QUndoStack *stack = ensureUndoStack(flowId);
    if (!stack)
        return;
    m_undoGroup->setActiveStack(stack);
    emit undoStackChanged(stack);
}

void ProjectService::clearUndoStacks()
{
    for (auto it = m_undoStacks.begin(); it != m_undoStacks.end(); ++it) {
        m_undoGroup->removeStack(it.value());
        it.value()->deleteLater();
    }
    m_undoStacks.clear();
}

void ProjectService::emitModified()
{
    emit modifiedChanged(isModified());
}

} // namespace Selt
