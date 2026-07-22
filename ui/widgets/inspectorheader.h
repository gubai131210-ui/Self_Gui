#ifndef INSPECTORHEADER_H
#define INSPECTORHEADER_H

#include "graphics/items/nodegraphicsitem.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QToolButton;

/// 检查器固定头：图标、模块名、分类、状态点、单节点运行与菜单。
class InspectorHeader : public QWidget
{
    Q_OBJECT

public:
    explicit InspectorHeader(QWidget *parent = nullptr);

    void setNodeInfo(const QString &displayName,
                     const QString &category,
                     const QString &iconKey = QString());
    void setStatus(NodeRunVisualStatus status, const QString &text);
    void setEmpty();

signals:
    void runNodeRequested();
    void copyParamsRequested();

private:
    void updateStatusDot(NodeRunVisualStatus status);

    QLabel *m_iconLabel{nullptr};
    QLabel *m_nameLabel{nullptr};
    QLabel *m_categoryLabel{nullptr};
    QLabel *m_statusDot{nullptr};
    QLabel *m_statusText{nullptr};
    QPushButton *m_runButton{nullptr};
    QToolButton *m_menuButton{nullptr};
};

#endif // INSPECTORHEADER_H
