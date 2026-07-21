#ifndef NODEPALETTE_H
#define NODEPALETTE_H

#include <QWidget>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMouseEvent;

class NodePalette : public QWidget
{
    Q_OBJECT

public:
    explicit NodePalette(QWidget *parent = nullptr);

signals:
    void nodeTypeActivated(const QString &type);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void onFilterChanged(const QString &text);
    void onItemActivated(QListWidgetItem *item);

private:
    void rebuildList(const QString &filter = QString());
    void startDragFromItem(QListWidgetItem *item);

    QLineEdit *m_filterEdit{nullptr};
    QListWidget *m_list{nullptr};
};

#endif // NODEPALETTE_H
