#ifndef CALIBRATIONDIALOG_H
#define CALIBRATIONDIALOG_H

#include "vision/model/calibrationstore.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QLabel;

class CalibrationDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CalibrationDialog(CalibrationStore *store, QWidget *parent = nullptr);

    CalibrationStore resultStore() const { return m_edited; }

private slots:
    void onSelectionChanged(int index);
    void onComputeScale();
    void onSaveCurrent();
    void onNewCalibration();
    void onDeleteCurrent();
    void onSetActive();

private:
    void reloadList();
    void loadForm(const CalibrationModel &model);
    CalibrationModel readForm() const;

    CalibrationStore *m_store{nullptr};
    CalibrationStore m_edited;
    QComboBox *m_list{nullptr};
    QLineEdit *m_idEdit{nullptr};
    QComboBox *m_unitCombo{nullptr};
    QDoubleSpinBox *m_unitPerPixel{nullptr};
    QDoubleSpinBox *m_originX{nullptr};
    QDoubleSpinBox *m_originY{nullptr};
    QDoubleSpinBox *m_rotationDeg{nullptr};
    QDoubleSpinBox *m_pixelSize{nullptr};
    QDoubleSpinBox *m_physicalSize{nullptr};
    QLabel *m_activeLabel{nullptr};
};

#endif // CALIBRATIONDIALOG_H
