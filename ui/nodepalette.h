#ifndef NODEPALETTE_H
#define NODEPALETTE_H

#include <QListWidget>

class QMouseEvent;

class NodePalette : public QListWidget
{
    Q_OBJECT

public:
    explicit NodePalette(QWidget *parent = nullptr);

signals:
    void nodeTypeActivated(const QString &type);

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
};

#endif // NODEPALETTE_H
