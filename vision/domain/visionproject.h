#ifndef VISIONPROJECT_H
#define VISIONPROJECT_H

#include "vision/domain/projectvariables.h"
#include "vision/domain/visionflow.h"
#include "vision/storage/projectresourcestore.h"

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

class VisionProject : public QObject
{
    Q_OBJECT
public:
    explicit VisionProject(QObject *parent = nullptr);

    QString projectId() const { return m_projectId; }
    void setProjectId(const QString &projectId);
    QString title() const { return m_title; }
    void setTitle(const QString &title);

    VisionFlow *activeFlow() const;
    QString activeFlowId() const { return m_activeFlowId; }
    bool setActiveFlow(const QString &flowId);

    VisionFlow *flow(const QString &flowId) const;
    QVector<VisionFlow *> flows() const;
    QStringList flowIds() const { return m_flowOrder; }
    QStringList flowNames() const;
    int flowCount() const { return m_flowOrder.size(); }

    VisionFlow *createFlow(const QString &name = QStringLiteral("流程"));
    VisionFlow *createFlowWithId(const QString &flowId, const QString &name);
    bool removeFlow(const QString &flowId);
    bool renameFlow(const QString &flowId, const QString &name);

    Selt::ProjectVariableStore &variables() { return m_variables; }
    const Selt::ProjectVariableStore &variables() const { return m_variables; }
    void setVariables(const Selt::ProjectVariableStore &vars);

    Selt::ProjectResourceStore &resources() { return m_resources; }
    const Selt::ProjectResourceStore &resources() const { return m_resources; }
    void setResources(const Selt::ProjectResourceStore &resources);

    /// Adapt legacy single-document graph into the default main flow.
    void importLegacyDocument(const class Document &document);
    /// Export active flow into a legacy Document for existing canvas/serializer.
    void exportActiveFlowToDocument(class Document &document) const;

    void clearAllFlows();
    void markClean();

    bool isModified() const { return m_modified; }
    void setModified(bool modified);

signals:
    void titleChanged(const QString &title);
    void modifiedChanged(bool modified);
    void flowAdded(const QString &flowId);
    void flowRemoved(const QString &flowId);
    void flowRenamed(const QString &flowId, const QString &name);
    void activeFlowChanged(const QString &flowId);
    void flowListChanged();
    void variablesChanged();

private:
    void ensureMainFlow();
    void connectFlowSignals(VisionFlow *flow);
    QString adjacentFlowId(const QString &removedId) const;

    QString m_projectId;
    QString m_title{QStringLiteral("未命名项目")};
    QString m_activeFlowId;
    QHash<QString, VisionFlow *> m_flows;
    QStringList m_flowOrder;
    Selt::ProjectVariableStore m_variables;
    Selt::ProjectResourceStore m_resources;
    bool m_modified{false};
};

#endif // VISIONPROJECT_H
