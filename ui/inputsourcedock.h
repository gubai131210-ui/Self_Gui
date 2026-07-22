#ifndef INPUTSOURCEDOCK_H
#define INPUTSOURCEDOCK_H

#include <QWidget>

namespace Selt {
class InputSourceService;
}

class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;

class InputSourceDock : public QWidget
{
    Q_OBJECT
public:
    explicit InputSourceDock(Selt::InputSourceService *service, QWidget *parent = nullptr);

private slots:
    void onKindChanged(int index);
    void browsePath();
    void openSource();
    void closeSource();
    void refreshStatus();

private:
    Selt::InputSourceService *m_service{nullptr};
    QComboBox *m_kindCombo{nullptr};
    QSpinBox *m_cameraIndex{nullptr};
    QLineEdit *m_pathEdit{nullptr};
    QLabel *m_statusLabel{nullptr};
};

#endif
