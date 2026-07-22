#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QPlainTextEdit;

namespace Selt {
namespace Communication {
class CommunicationManager;
}
}

/// Dock for editing communication profiles and testing connectivity (client only).
class CommunicationDock : public QWidget
{
    Q_OBJECT
public:
    explicit CommunicationDock(QWidget *parent = nullptr);

signals:
    void diagnosticsReady(const QStringList &lines);

private slots:
    void onSaveProfile();
    void onTestConnection();
    void onRefreshCapabilities();
    void onProfileSelected(int index);

private:
    void reloadProfileList();
    void loadSelectedToForm();

    QComboBox *m_profileCombo{nullptr};
    QComboBox *m_kindCombo{nullptr};
    QLineEdit *m_idEdit{nullptr};
    QLineEdit *m_nameEdit{nullptr};
    QLineEdit *m_hostEdit{nullptr};
    QSpinBox *m_portSpin{nullptr};
    QLineEdit *m_comEdit{nullptr};
    QSpinBox *m_baudSpin{nullptr};
    QSpinBox *m_unitSpin{nullptr};
    QSpinBox *m_timeoutSpin{nullptr};
    QLabel *m_stateLabel{nullptr};
    QPlainTextEdit *m_capEdit{nullptr};
    QPushButton *m_saveButton{nullptr};
    QPushButton *m_testButton{nullptr};
};
