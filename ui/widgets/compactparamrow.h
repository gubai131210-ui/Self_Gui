#ifndef COMPACTPARAMROW_H
#define COMPACTPARAMROW_H

#include <QWidget>

#ifndef SELT_HAS_OPENCV
#define SELT_HAS_OPENCV 0
#endif

#if SELT_HAS_OPENCV
#include "vision/data/datatype.h"
#endif

class QLabel;
class QToolButton;
class QSlider;
class QGridLayout;
class QEvent;
class BindingEditor;

/// 2 列紧凑参数行：标签 | 控件(通常为 BindingEditor) | 绑定图标；可选数值滑块。
class CompactParamRow : public QWidget
{
    Q_OBJECT

public:
    explicit CompactParamRow(const QString &label,
                             QWidget *editor,
                             bool hasSlider = false,
                             QWidget *parent = nullptr);

    QWidget *editor() const { return m_editor; }
    void setBindingEditor(BindingEditor *editor);
    BindingEditor *bindingEditor() const { return m_bindingEditor; }

#if SELT_HAS_OPENCV
    void setBindingIndicator(Selt::ParameterSourceKind kind);
#endif

    void setSliderRange(double min, double max, int decimals = 0);
    void setSliderVisible(bool visible);
    bool isSliderVisible() const;

    QString paramKey() const { return property("paramKey").toString(); }
    QString paramLabel() const { return property("paramLabel").toString(); }

signals:
    void bindingIconClicked();
    void sliderEdited();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onLinkClicked();
    void onSliderMoved(int value);
    void syncSliderFromEditor();

private:
    void wireEditorSlider();
    QWidget *sliderTargetEditor() const;

    QLabel *m_label{nullptr};
    QWidget *m_editor{nullptr};
    QToolButton *m_linkButton{nullptr};
    QSlider *m_slider{nullptr};
    BindingEditor *m_bindingEditor{nullptr};
    QGridLayout *m_grid{nullptr};
    bool m_bindingExpanded{false};
    int m_decimals{0};
    double m_sliderMin{0.0};
    double m_sliderMax{100.0};
};

#endif // COMPACTPARAMROW_H
