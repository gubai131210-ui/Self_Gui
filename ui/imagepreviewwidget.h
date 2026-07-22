#ifndef IMAGEPREVIEWWIDGET_H
#define IMAGEPREVIEWWIDGET_H

#include "vision/model/extendedroi.h"
#include "vision/model/overlayitems.h"
#include "vision/model/roi.h"

#include <QImage>
#include <QPoint>
#include <QSet>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;

class ImageCanvasWidget : public QWidget
{
    Q_OBJECT
public:
    enum class RoiHandle {
        None,
        Move,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
        Radius,
        PolygonVertex
    };

    enum class RoiTool {
        Rectangle,
        RotatedRect,
        Circle,
        Ellipse,
        Polygon
    };

    explicit ImageCanvasWidget(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    void setOverlays(const OverlayList &overlays);
    void setRoi(const RoiRect &roi);
    void setExtendedRoi(const Selt::ExtendedRoi &roi);
    void setRoiEditEnabled(bool enabled);
    void setRoiTool(RoiTool tool);
    RoiTool roiTool() const { return m_roiTool; }
    void setOverlayTypeFilter(const QSet<int> &visibleTypes); // empty = all
    void setScale(qreal scale);
    void setOffset(const QPointF &offset);
    qreal scale() const { return m_scale; }
    QPointF offset() const { return m_offset; }
    RoiRect roi() const { return m_roi.toLegacyRect(); }
    Selt::ExtendedRoi extendedRoi() const { return m_roi; }
    QImage image() const { return m_image; }

    void fitToView();
    void resetView();
    void zoomBy(qreal factor);
    void clearRoi();
    void setCheckerboardBackground(bool enabled);
    bool checkerboardBackground() const { return m_checkerboard; }
    /// When true, resize/refit keeps the image fitting the widget.
    void setAutoFitEnabled(bool enabled);
    bool autoFitEnabled() const { return m_autoFit; }

signals:
    void roiChanged(const RoiRect &roi);
    void extendedRoiChanged(const Selt::ExtendedRoi &roi);
    void viewChanged();
    void cursorInfoChanged(const QPointF &imagePos, const QColor &pixel, bool valid);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRectF imageDisplayRect() const;
    QPointF widgetToImage(const QPoint &pos) const;
    QPointF imageToWidget(const QPointF &pos) const;
    RoiHandle hitTestHandle(const QPoint &pos) const;
    void clampRoiToImage();
    void emitRoiIfChanged();
    void drawRoiShape(QPainter &painter) const;
    void drawHandles(QPainter &painter) const;
    void applyBoundingBoxToShape(const QRectF &box);
    Selt::RoiShapeType shapeForTool(RoiTool tool) const;

    QImage m_image;
    OverlayList m_overlays;
    QSet<int> m_overlayTypeFilter;
    Selt::ExtendedRoi m_roi;
    RoiTool m_roiTool{RoiTool::Rectangle};
    qreal m_scale{1.0};
    QPointF m_offset;
    bool m_panning{false};
    bool m_drawingRoi{false};
    bool m_editingRoi{false};
    bool m_polygonDrawing{false};
    bool m_roiEditEnabled{true};
    RoiHandle m_activeHandle{RoiHandle::None};
    int m_activePolygonIndex{-1};
    QPoint m_lastMousePos;
    QPointF m_roiStart;
    QRectF m_roiAtEditStart;
    bool m_checkerboard{false};
    bool m_autoFit{true};
};

class ImagePreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImagePreviewWidget(QWidget *parent = nullptr);

    void setImage(const QImage &image, const QString &title = QString());
    void setOverlays(const OverlayList &overlays);
    void setRoi(const RoiRect &roi);
    void setExtendedRoi(const Selt::ExtendedRoi &roi);
    void clearImage();
    void setRoiEditEnabled(bool enabled);

    RoiRect currentRoi() const;
    Selt::ExtendedRoi currentExtendedRoi() const;
    QImage currentImage() const;

signals:
    void roiChanged(const RoiRect &roi);
    void extendedRoiChanged(const Selt::ExtendedRoi &roi);
    void teachTemplateRequested();

private:
    void updateTitle();
    void updateHud(const QPointF &pos, const QColor &pixel, bool valid);
    void resetHud();
    void applyOverlayFilter();
    void onRoiToolChanged(int index);
    void configureCompactButton(QPushButton *button, const QString &text, const QString &tip);

    QLabel *m_titleLabel{nullptr};
    QLabel *m_hudLabel{nullptr};
    ImageCanvasWidget *m_canvas{nullptr};
    QPushButton *m_fitBtn{nullptr};
    QPushButton *m_resetBtn{nullptr};
    QPushButton *m_zoomInBtn{nullptr};
    QPushButton *m_zoomOutBtn{nullptr};
    QPushButton *m_clearRoiBtn{nullptr};
    QPushButton *m_teachTemplateBtn{nullptr};
    QPushButton *m_bgToggleBtn{nullptr};
    QComboBox *m_roiToolCombo{nullptr};
    QComboBox *m_overlayFilter{nullptr};
    OverlayList m_allOverlays;
    QString m_title;
};

#endif // IMAGEPREVIEWWIDGET_H
