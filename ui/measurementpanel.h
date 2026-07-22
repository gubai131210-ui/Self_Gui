#ifndef MEASUREMENTPANEL_H
#define MEASUREMENTPANEL_H

#include "vision/model/measurementresult.h"
#include "vision/model/visioncontext.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;

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

private slots:
    void copySelectedOutputCell();
    void copySelectedOutputRow();
    void showSelectedOutputDetail();

private:
    void rebuildOutputCombo(const VisionContext &context, const QString &preferredNodeId);
    void rebuildResultTable(const VisionContext &context);
    void rebuildOutputDataTable(const ModuleRunResult *moduleResult);
    void showMeasurementRows(const MeasurementResult &measurement);
    void updateDiagnosisPanel(const ModuleRunResult *moduleResult);
    void onOutputComboChanged(int index);
    void applyTableFilter();
    void appendOutputRow(const QString &category,
                         const QString &portId,
                         const QString &displayName,
                         const QString &typeName,
                         const QString &valueText,
                         const QString &status = QStringLiteral("ok"),
                         const QString &detailText = QString());

    QComboBox *m_outputCombo{nullptr};
    QLineEdit *m_filterEdit{nullptr};
    QTableWidget *m_resultTable{nullptr};
    QTableWidget *m_outputDataTable{nullptr};
    QLabel *m_statusLabel{nullptr};
    QLabel *m_moduleStatusLabel{nullptr};
    QLabel *m_diagnosisLabel{nullptr};
    QLabel *m_timeLabel{nullptr};
    QLabel *m_moduleTimeLabel{nullptr};
    QLabel *m_frameLabel{nullptr};
    QPlainTextEdit *m_logEdit{nullptr};
    VisionContext m_context;
};

#endif // MEASUREMENTPANEL_H
