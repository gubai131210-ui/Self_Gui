#include "ui/inputsourcedock.h"

#include "devices/camera/icameradevice.h"
#include "ui/theme/uistyle.h"
#include "vision/runtime/inputsourceservice.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

InputSourceDock::InputSourceDock(Selt::InputSourceService *service, QWidget *parent)
    : QWidget(parent)
    , m_service(service)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Selt::UiStyle::panelMargin,
                               Selt::UiStyle::panelMargin,
                               Selt::UiStyle::panelMargin,
                               Selt::UiStyle::panelMargin);
    layout->setSpacing(Selt::UiStyle::compactSpacing);

    auto *title = new QLabel(QStringLiteral("输入源"), this);
    title->setObjectName(QStringLiteral("PanelTitle"));
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);

    m_kindCombo = new QComboBox(this);
    m_kindCombo->addItem(QStringLiteral("单张图片"), int(Selt::InputSourceService::SourceKind::ImageFile));
    m_kindCombo->addItem(QStringLiteral("目录回放"), int(Selt::InputSourceService::SourceKind::DirectoryReplay));
    m_kindCombo->addItem(QStringLiteral("相机"), int(Selt::InputSourceService::SourceKind::Camera));

    m_cameraIndex = new QSpinBox(this);
    m_cameraIndex->setRange(0, 16);
    m_cameraIndex->setPrefix(QStringLiteral("索引 "));
    m_cameraIndex->setVisible(false);

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(QStringLiteral("图片或目录路径"));

    auto *browseBtn = new QPushButton(QStringLiteral("浏览…"), this);
    auto *openBtn = new QPushButton(QStringLiteral("打开"), this);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    auto *triggerBtn = new QPushButton(QStringLiteral("软触发"), this);
    auto *refreshBtn = new QPushButton(QStringLiteral("刷新相机"), this);

    m_statusLabel = new QLabel(QStringLiteral("未打开输入源"), this);
    m_statusLabel->setWordWrap(true);

    auto *pathRow = new QHBoxLayout;
    pathRow->addWidget(m_pathEdit, 1);
    pathRow->addWidget(browseBtn);

    auto *actionRow = new QHBoxLayout;
    actionRow->addWidget(openBtn);
    actionRow->addWidget(closeBtn);
    actionRow->addWidget(triggerBtn);
    actionRow->addWidget(refreshBtn);

    layout->addWidget(title);
    layout->addWidget(m_kindCombo);
    layout->addWidget(m_cameraIndex);
    layout->addLayout(pathRow);
    layout->addLayout(actionRow);
    layout->addWidget(m_statusLabel);
    layout->addStretch(1);

    connect(m_kindCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InputSourceDock::onKindChanged);
    connect(browseBtn, &QPushButton::clicked, this, &InputSourceDock::browsePath);
    connect(openBtn, &QPushButton::clicked, this, &InputSourceDock::openSource);
    connect(closeBtn, &QPushButton::clicked, this, &InputSourceDock::closeSource);
    connect(triggerBtn, &QPushButton::clicked, this, [this]() {
        if (!m_service)
            return;
        const Selt::Error err = m_service->softTrigger();
        if (!err.ok())
            m_statusLabel->setText(QStringLiteral("软触发失败: %1").arg(err.message));
        else
            refreshStatus();
    });
    connect(refreshBtn, &QPushButton::clicked, this, [this]() {
        const QVector<Selt::CameraDeviceInfo> cams = Selt::probeOpenCvCameras(4);
        if (cams.isEmpty()) {
            m_statusLabel->setText(QStringLiteral("未探测到可用相机（需 OpenCV videoio 且设备已连接）"));
        } else {
            QStringList names;
            for (const Selt::CameraDeviceInfo &c : cams)
                names.append(c.displayName);
            m_statusLabel->setText(QStringLiteral("可用相机: %1").arg(names.join(QStringLiteral(", "))));
        }
    });

    if (m_service) {
        connect(m_service, &Selt::InputSourceService::sourceChanged,
                this, &InputSourceDock::refreshStatus);
        connect(m_service, &Selt::InputSourceService::diagnosticsChanged,
                this, [this](const QVector<Selt::RuntimeDiagnostic> &diags) {
                    if (!diags.isEmpty())
                        m_statusLabel->setText(diags.last().message);
                });
    }
    onKindChanged(m_kindCombo->currentIndex());
    refreshStatus();
}

void InputSourceDock::onKindChanged(int index)
{
    const int kind = m_kindCombo->itemData(index).toInt();
    const bool isCamera = kind == int(Selt::InputSourceService::SourceKind::Camera);
    m_cameraIndex->setVisible(isCamera);
    m_pathEdit->setEnabled(!isCamera);
    if (isCamera)
        m_pathEdit->setPlaceholderText(QStringLiteral("相机模式无需路径"));
    else if (kind == int(Selt::InputSourceService::SourceKind::DirectoryReplay))
        m_pathEdit->setPlaceholderText(QStringLiteral("选择图片目录"));
    else
        m_pathEdit->setPlaceholderText(QStringLiteral("选择图片文件"));
}

void InputSourceDock::browsePath()
{
    const int kind = m_kindCombo->currentData().toInt();
    if (kind == int(Selt::InputSourceService::SourceKind::DirectoryReplay)) {
        const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择回放目录"));
        if (!dir.isEmpty())
            m_pathEdit->setText(dir);
        return;
    }
    if (kind == int(Selt::InputSourceService::SourceKind::ImageFile)) {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择图片"),
            QString(),
            QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;所有文件 (*.*)"));
        if (!path.isEmpty())
            m_pathEdit->setText(path);
    }
}

void InputSourceDock::openSource()
{
    if (!m_service)
        return;
    const int kind = m_kindCombo->currentData().toInt();
    Selt::Error err = Selt::Error::success();
    if (kind == int(Selt::InputSourceService::SourceKind::Camera)) {
        err = m_service->openCamera(m_cameraIndex->value());
    } else if (kind == int(Selt::InputSourceService::SourceKind::DirectoryReplay)) {
        err = m_service->openDirectoryReplay(m_pathEdit->text().trimmed());
    } else {
        err = m_service->openImageFile(m_pathEdit->text().trimmed());
    }
    if (!err.ok())
        m_statusLabel->setText(QStringLiteral("打开失败: %1").arg(err.message));
    else
        refreshStatus();
}

void InputSourceDock::closeSource()
{
    if (m_service)
        m_service->close();
    refreshStatus();
}

void InputSourceDock::refreshStatus()
{
    if (!m_service) {
        m_statusLabel->setText(QStringLiteral("无输入源服务"));
        return;
    }
    if (!m_service->isOpen()) {
        m_statusLabel->setText(QStringLiteral("未打开输入源（运行时将使用图片输入节点路径）"));
        return;
    }
    const Selt::CameraDeviceInfo info = m_service->deviceInfo();
    const QString caps = info.capabilities.isEmpty()
        ? QStringLiteral("-")
        : info.capabilities.join(QStringLiteral(","));
    m_statusLabel->setText(
        QStringLiteral("已打开: %1\n设备: %2 | 帧计数: %3 | 能力: %4")
            .arg(m_service->sourceDescription(),
                 info.id,
                 QString::number(m_service->frameCount()),
                 caps));
}
