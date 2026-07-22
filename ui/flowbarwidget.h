#ifndef FLOWBARWIDGET_H
#define FLOWBARWIDGET_H

#include <QWidget>

class QComboBox;
class QTabBar;
class QToolButton;
class QStackedWidget;

/// Compact multi-flow selector with tab style + combo fallback.
/// Tab and Combo share one active-id state source via activeFlowChosen.
class FlowBarWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FlowBarWidget(QWidget *parent = nullptr);

    void setFlows(const QStringList &ids, const QStringList &names, const QString &activeId);
    QString activeFlowId() const { return m_activeId; }

signals:
    void activeFlowChosen(const QString &flowId);
    void createFlowRequested();
    void renameFlowRequested();
    void removeFlowRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void syncActiveUi(const QString &activeId);
    void updatePresentationMode();
    void emitActiveFromIndex(int index);

    QTabBar *m_tabBar{nullptr};
    QComboBox *m_combo{nullptr};
    QStackedWidget *m_selectorStack{nullptr};
    QToolButton *m_addButton{nullptr};
    QToolButton *m_renameButton{nullptr};
    QToolButton *m_removeButton{nullptr};
    QStringList m_ids;
    QString m_activeId;
    bool m_updating{false};
};

#endif // FLOWBARWIDGET_H
