#ifndef IMAGEPREVIEWWIDGET_H
#define IMAGEPREVIEWWIDGET_H

#include "vision/model/overlayitems.h"
#include "vision/model/roi.h"

#include <QImage>
#include <QPoint>
#include <QWidget>

class QLabel;
class QPushButton;

class ImageCanvasWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ImageCanvasWidget(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    void setOverlays(const OverlayList &overlays);
    void setRoi(const RoiRect &roi);
    void setRoiEditEnabled(bool enabled);
    void setScale(qreal scale);
    void setOffset(const QPointF &offset);
    qreal scale() const { return m_scale; }
    QPointF offset() const { return m_offset; }
    RoiRect roi() const { return m_roi; }
    QImage image() const { return m_image; }

    void fitToView();
    void resetView();
    void zoomBy(qreal factor);

signals:
    void roiChanged(const RoiRect &roi);
    void viewChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRectF imageDisplayRect() const;
    QPointF widgetToImage(const QPoint &pos) const;
    QPointF imageToWidget(const QPointF &pos) const;

    QImage m_image;
    OverlayList m_overlays;
    RoiRect m_roi;
    qreal m_scale{1.0};
    QPointF m_offset;
    bool m_panning{false};
    bool m_drawingRoi{false};
    bool m_roiEditEnabled{true};
    QPoint m_lastMousePos;
    QPointF m_roiStart;
};

class ImagePreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImagePreviewWidget(QWidget *parent = nullptr);

    void setImage(const QImage &image, const QString &title = QString());
    void setOverlays(const OverlayList &overlays);
    void setRoi(const RoiRect &roi);
    void clearImage();
    void setRoiEditEnabled(bool enabled);

    RoiRect currentRoi() const;
    QImage currentImage() const;

signals:
    void roiChanged(const RoiRect &roi);

private:
    void updateTitle();

    QLabel *m_titleLabel{nullptr};
    ImageCanvasWidget *m_canvas{nullptr};
    QPushButton *m_fitBtn{nullptr};
    QPushButton *m_resetBtn{nullptr};
    QPushButton *m_clearRoiBtn{nullptr};
    QString m_title;
};

#endif // IMAGEPREVIEWWIDGET_H
