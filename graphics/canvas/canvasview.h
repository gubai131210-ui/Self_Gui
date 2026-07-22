#ifndef CANVASVIEW_H
#define CANVASVIEW_H

#include <QGraphicsView>

class CanvasScene;

class CanvasView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit CanvasView(CanvasScene *scene, QWidget *parent = nullptr);

    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitAll();
    qreal zoomFactor() const;
    void setZoomFactor(qreal factor);
    void setMinimapVisible(bool visible);
    bool isMinimapVisible() const { return m_minimapVisible; }

    QPointF viewCenter() const;
    void setViewCenter(const QPointF &center);
    int horizontalScroll() const;
    int verticalScroll() const;
    void setScrollOffsets(int h, int v);

signals:
    void zoomChanged(qreal factor);
    void mouseScenePosChanged(const QPointF &pos);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
    void setZoom(qreal factor);
    CanvasScene *canvasScene() const;

    bool m_panning{false};
    bool m_spaceDown{false};
    QPoint m_lastPanPos;
    qreal m_zoom{1.0};
    bool m_minimapVisible{true};
};

#endif // CANVASVIEW_H
