#ifndef MEASUREMENTPANEL_H
#define MEASUREMENTPANEL_H

#include "vision/model/measurementresult.h"
#include "vision/model/visioncontext.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPlainTextEdit;

class MeasurementPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MeasurementPanel(QWidget *parent = nullptr);

    void showContext(const VisionContext &context, const QString &preferredNodeId = QString());
    void clearResults();
    QString selectedOutputNodeId() const;
    void setSelectedOutputNodeId(const QString &nodeId);

signals:
    void outputNodeChanged(const QString &nodeId);

private:
    void rebuildOutputCombo(const VisionContext &context, const QString &preferredNodeId);
    void showMeasurement(const MeasurementResult &measurement);
    void onOutputComboChanged(int index);

    QComboBox *m_outputCombo{nullptr};
    QLabel *m_statusLabel{nullptr};
    QLabel *m_moduleStatusLabel{nullptr};
    QLabel *m_widthLabel{nullptr};
    QLabel *m_heightLabel{nullptr};
    QLabel *m_areaLabel{nullptr};
    QLabel *m_centerLabel{nullptr};
    QLabel *m_timeLabel{nullptr};
    QLabel *m_moduleTimeLabel{nullptr};
    QPlainTextEdit *m_logEdit{nullptr};
    VisionContext m_context;
};

#endif // MEASUREMENTPANEL_H
