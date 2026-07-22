#include "ui/communicationdock.h"

#include "communication/communicationmanager.h"
#include "communication/clients.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace Selt::Communication;

CommunicationDock::CommunicationDock(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    m_capEdit = new QPlainTextEdit(this);
    m_capEdit->setReadOnly(true);
    m_capEdit->setMaximumHeight(90);
    layout->addWidget(new QLabel(QStringLiteral("通讯能力 / 部署提示"), this));
    layout->addWidget(m_capEdit);

    auto *form = new QFormLayout;
    m_profileCombo = new QComboBox(this);
    m_idEdit = new QLineEdit(this);
    m_nameEdit = new QLineEdit(this);
    m_kindCombo = new QComboBox(this);
    m_kindCombo->addItem(QStringLiteral("TCP"), QStringLiteral("tcp"));
    m_kindCombo->addItem(QStringLiteral("COM"), QStringLiteral("serial"));
    m_kindCombo->addItem(QStringLiteral("Modbus TCP"), QStringLiteral("modbus_tcp"));
    m_kindCombo->addItem(QStringLiteral("Modbus RTU"), QStringLiteral("modbus_rtu"));
    m_hostEdit = new QLineEdit(QStringLiteral("127.0.0.1"), this);
    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(502);
    m_comEdit = new QLineEdit(QStringLiteral("COM1"), this);
    m_baudSpin = new QSpinBox(this);
    m_baudSpin->setRange(1200, 921600);
    m_baudSpin->setValue(9600);
    m_unitSpin = new QSpinBox(this);
    m_unitSpin->setRange(0, 255);
    m_unitSpin->setValue(1);
    m_timeoutSpin = new QSpinBox(this);
    m_timeoutSpin->setRange(50, 60000);
    m_timeoutSpin->setValue(1000);
    m_stateLabel = new QLabel(QStringLiteral("状态: closed"), this);
    form->addRow(QStringLiteral("配置"), m_profileCombo);
    form->addRow(QStringLiteral("ID"), m_idEdit);
    form->addRow(QStringLiteral("名称"), m_nameEdit);
    form->addRow(QStringLiteral("类型"), m_kindCombo);
    form->addRow(QStringLiteral("主机"), m_hostEdit);
    form->addRow(QStringLiteral("端口"), m_portSpin);
    form->addRow(QStringLiteral("串口"), m_comEdit);
    form->addRow(QStringLiteral("波特率"), m_baudSpin);
    form->addRow(QStringLiteral("站号"), m_unitSpin);
    form->addRow(QStringLiteral("超时ms"), m_timeoutSpin);
    layout->addLayout(form);
    layout->addWidget(m_stateLabel);

    auto *row = new QHBoxLayout;
    m_saveButton = new QPushButton(QStringLiteral("保存配置"), this);
    m_testButton = new QPushButton(QStringLiteral("测试连接(Mock)"), this);
    auto *refresh = new QPushButton(QStringLiteral("刷新能力"), this);
    row->addWidget(m_saveButton);
    row->addWidget(m_testButton);
    row->addWidget(refresh);
    layout->addLayout(row);
    layout->addStretch(1);

    connect(m_saveButton, &QPushButton::clicked, this, &CommunicationDock::onSaveProfile);
    connect(m_testButton, &QPushButton::clicked, this, &CommunicationDock::onTestConnection);
    connect(refresh, &QPushButton::clicked, this, &CommunicationDock::onRefreshCapabilities);
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CommunicationDock::onProfileSelected);

    onRefreshCapabilities();
    reloadProfileList();
}

void CommunicationDock::onRefreshCapabilities()
{
    QStringList lines = capabilitySummary();
    lines.append(QString());
    lines += runtimeDeploymentHints();
    m_capEdit->setPlainText(lines.join(QLatin1Char('\n')));
    emit diagnosticsReady(lines);
}

void CommunicationDock::reloadProfileList()
{
    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();
    m_profileCombo->addItem(QStringLiteral("(新建)"), QString());
    for (const CommunicationProfile &p : CommunicationManager::instance().profiles())
        m_profileCombo->addItem(p.displayName.isEmpty() ? p.id : p.displayName, p.id);
    m_profileCombo->blockSignals(false);
}

void CommunicationDock::onProfileSelected(int index)
{
    Q_UNUSED(index);
    loadSelectedToForm();
}

void CommunicationDock::loadSelectedToForm()
{
    const QString id = m_profileCombo->currentData().toString();
    if (id.isEmpty())
        return;
    const CommunicationProfile p = CommunicationManager::instance().profile(id);
    m_idEdit->setText(p.id);
    m_nameEdit->setText(p.displayName);
    const int kindIdx = m_kindCombo->findData(transportKindToString(p.kind));
    if (kindIdx >= 0)
        m_kindCombo->setCurrentIndex(kindIdx);
    m_hostEdit->setText(p.tcp.host);
    m_portSpin->setValue(p.tcp.port);
    m_comEdit->setText(p.serial.portName);
    m_baudSpin->setValue(p.serial.baudRate);
    m_unitSpin->setValue(p.modbus.unitId);
    m_timeoutSpin->setValue(p.tcp.timeoutMs);
    m_stateLabel->setText(QStringLiteral("状态: %1").arg(CommunicationManager::instance().stateText(id)));
}

void CommunicationDock::onSaveProfile()
{
    CommunicationProfile p;
    p.id = m_idEdit->text().trimmed();
    if (p.id.isEmpty())
        p.id = QStringLiteral("conn_%1").arg(m_profileCombo->count());
    p.displayName = m_nameEdit->text().trimmed().isEmpty() ? p.id : m_nameEdit->text().trimmed();
    p.kind = transportKindFromString(m_kindCombo->currentData().toString());
    p.tcp.host = m_hostEdit->text().trimmed();
    p.tcp.port = m_portSpin->value();
    p.tcp.timeoutMs = m_timeoutSpin->value();
    p.serial.portName = m_comEdit->text().trimmed();
    p.serial.baudRate = m_baudSpin->value();
    p.serial.timeoutMs = m_timeoutSpin->value();
    p.modbus.unitId = m_unitSpin->value();
    p.modbus.timeoutMs = m_timeoutSpin->value();
    p.modbus.tcp = p.tcp;
    p.modbus.serial = p.serial;
    p.modbus.transport = (p.kind == TransportKind::ModbusRtu) ? TransportKind::ModbusRtu
                                                              : TransportKind::ModbusTcp;
    CommunicationManager::instance().upsertProfile(p);
    reloadProfileList();
    const int idx = m_profileCombo->findData(p.id);
    if (idx >= 0)
        m_profileCombo->setCurrentIndex(idx);
    emit diagnosticsReady({QStringLiteral("已保存通讯配置: %1").arg(p.id)});
}

void CommunicationDock::onTestConnection()
{
    onSaveProfile();
    const QString id = m_idEdit->text().trimmed();
    auto diag = CommunicationManager::instance().open(id, true);
    m_stateLabel->setText(QStringLiteral("状态: %1").arg(CommunicationManager::instance().stateText(id)));
    QStringList lines;
    if (diag.ok) {
        lines << QStringLiteral("测试连接成功(Mock): %1").arg(id);
        ReceiveFramePolicy policy;
        policy.expectMinBytes = 1;
        auto req = CommunicationManager::instance().request(id, QByteArray("ping"), policy, true);
        lines << QStringLiteral("回显: %1 (%2 ms)")
                     .arg(QString::fromUtf8(req.response.toHex(' ')))
                     .arg(req.elapsedMs);
    } else {
        lines << QStringLiteral("测试失败: %1 [%2]").arg(diag.message, diag.code);
        lines += diag.hints;
    }
    emit diagnosticsReady(lines);
}
