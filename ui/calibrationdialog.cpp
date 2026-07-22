#include "ui/calibrationdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QUuid>
#include <QVBoxLayout>

CalibrationDialog::CalibrationDialog(CalibrationStore *store, QWidget *parent)
    : QDialog(parent)
    , m_store(store)
{
    setWindowTitle(QStringLiteral("标定管理"));
    resize(460, 420);
    if (m_store)
        m_edited = *m_store;

    auto *layout = new QVBoxLayout(this);
    m_list = new QComboBox(this);
    m_activeLabel = new QLabel(this);
    layout->addWidget(new QLabel(QStringLiteral("标定列表"), this));
    layout->addWidget(m_list);
    layout->addWidget(m_activeLabel);

    auto *form = new QFormLayout;
    m_idEdit = new QLineEdit(this);
    m_unitCombo = new QComboBox(this);
    m_unitCombo->addItems({QStringLiteral("px"), QStringLiteral("mm"), QStringLiteral("um")});
    m_unitPerPixel = new QDoubleSpinBox(this);
    m_unitPerPixel->setDecimals(6);
    m_unitPerPixel->setRange(1e-9, 1e6);
    m_unitPerPixel->setValue(1.0);
    m_originX = new QDoubleSpinBox(this);
    m_originY = new QDoubleSpinBox(this);
    m_rotationDeg = new QDoubleSpinBox(this);
    m_originX->setRange(-1e6, 1e6);
    m_originY->setRange(-1e6, 1e6);
    m_rotationDeg->setRange(-360.0, 360.0);
    m_rotationDeg->setDecimals(3);
    form->addRow(QStringLiteral("ID"), m_idEdit);
    form->addRow(QStringLiteral("单位"), m_unitCombo);
    form->addRow(QStringLiteral("单位/像素"), m_unitPerPixel);
    form->addRow(QStringLiteral("原点 X (px)"), m_originX);
    form->addRow(QStringLiteral("原点 Y (px)"), m_originY);
    form->addRow(QStringLiteral("旋转 (°)"), m_rotationDeg);
    layout->addLayout(form);

    auto *scaleRow = new QHBoxLayout;
    m_pixelSize = new QDoubleSpinBox(this);
    m_physicalSize = new QDoubleSpinBox(this);
    m_pixelSize->setRange(1e-9, 1e9);
    m_physicalSize->setRange(1e-9, 1e9);
    m_pixelSize->setValue(100.0);
    m_physicalSize->setValue(10.0);
    auto *computeBtn = new QPushButton(QStringLiteral("由像素/实物尺寸计算"), this);
    scaleRow->addWidget(new QLabel(QStringLiteral("像素尺寸"), this));
    scaleRow->addWidget(m_pixelSize);
    scaleRow->addWidget(new QLabel(QStringLiteral("实物尺寸"), this));
    scaleRow->addWidget(m_physicalSize);
    scaleRow->addWidget(computeBtn);
    layout->addLayout(scaleRow);

    auto *btnRow = new QHBoxLayout;
    auto *newBtn = new QPushButton(QStringLiteral("新建"), this);
    auto *saveBtn = new QPushButton(QStringLiteral("保存当前"), this);
    auto *delBtn = new QPushButton(QStringLiteral("删除"), this);
    auto *activeBtn = new QPushButton(QStringLiteral("设为当前标定"), this);
    btnRow->addWidget(newBtn);
    btnRow->addWidget(saveBtn);
    btnRow->addWidget(delBtn);
    btnRow->addWidget(activeBtn);
    layout->addLayout(btnRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(m_list, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CalibrationDialog::onSelectionChanged);
    connect(computeBtn, &QPushButton::clicked, this, &CalibrationDialog::onComputeScale);
    connect(newBtn, &QPushButton::clicked, this, &CalibrationDialog::onNewCalibration);
    connect(saveBtn, &QPushButton::clicked, this, &CalibrationDialog::onSaveCurrent);
    connect(delBtn, &QPushButton::clicked, this, &CalibrationDialog::onDeleteCurrent);
    connect(activeBtn, &QPushButton::clicked, this, &CalibrationDialog::onSetActive);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (m_store)
            *m_store = m_edited;
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    reloadList();
}

void CalibrationDialog::reloadList()
{
    m_list->blockSignals(true);
    m_list->clear();
    for (const CalibrationModel &item : m_edited.items()) {
        const QString label = QStringLiteral("%1 (%2)")
                                  .arg(item.calibrationId, item.unit);
        m_list->addItem(label, item.calibrationId);
    }
    const int activeIndex = m_list->findData(m_edited.activeId());
    if (activeIndex >= 0)
        m_list->setCurrentIndex(activeIndex);
    m_list->blockSignals(false);
    m_activeLabel->setText(QStringLiteral("当前标定: %1")
                               .arg(m_edited.activeId().isEmpty()
                                        ? QStringLiteral("(无)")
                                        : m_edited.activeId()));
    if (m_list->count() > 0)
        onSelectionChanged(m_list->currentIndex());
    else
        loadForm({});
}

void CalibrationDialog::loadForm(const CalibrationModel &model)
{
    m_idEdit->setText(model.calibrationId);
    const int unitIndex = m_unitCombo->findText(model.unit);
    m_unitCombo->setCurrentIndex(unitIndex >= 0 ? unitIndex : 0);
    m_unitPerPixel->setValue(model.unitPerPixel > 0.0 ? model.unitPerPixel : 1.0);
    m_originX->setValue(model.originPx.x());
    m_originY->setValue(model.originPx.y());
    m_rotationDeg->setValue(model.rotationRad * 180.0 / 3.14159265358979323846);
}

CalibrationModel CalibrationDialog::readForm() const
{
    CalibrationModel model;
    model.calibrationId = m_idEdit->text().trimmed();
    model.unit = m_unitCombo->currentText();
    model.unitPerPixel = m_unitPerPixel->value();
    model.originPx = QPointF(m_originX->value(), m_originY->value());
    model.rotationRad = m_rotationDeg->value() * 3.14159265358979323846 / 180.0;
    model.valid = !model.calibrationId.isEmpty() && model.unitPerPixel > 0.0;
    return model;
}

void CalibrationDialog::onSelectionChanged(int index)
{
    if (index < 0)
        return;
    const QString id = m_list->itemData(index).toString();
    for (const CalibrationModel &item : m_edited.items()) {
        if (item.calibrationId == id) {
            loadForm(item);
            return;
        }
    }
}

void CalibrationDialog::onComputeScale()
{
    if (m_pixelSize->value() <= 0.0) {
        QMessageBox::warning(this, QStringLiteral("标定"), QStringLiteral("像素尺寸必须大于 0"));
        return;
    }
    m_unitPerPixel->setValue(m_physicalSize->value() / m_pixelSize->value());
}

void CalibrationDialog::onSaveCurrent()
{
    CalibrationModel model = readForm();
    if (!model.valid) {
        QMessageBox::warning(this, QStringLiteral("标定"), QStringLiteral("请填写有效的标定 ID 和比例"));
        return;
    }
    m_edited.upsert(model);
    reloadList();
}

void CalibrationDialog::onNewCalibration()
{
    CalibrationModel model;
    model.calibrationId = QStringLiteral("cal-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    model.unit = QStringLiteral("mm");
    model.unitPerPixel = 0.1;
    model.valid = true;
    m_edited.upsert(model);
    m_edited.setActiveId(model.calibrationId);
    reloadList();
}

void CalibrationDialog::onDeleteCurrent()
{
    const QString id = m_idEdit->text().trimmed();
    if (id.isEmpty())
        return;
    m_edited.remove(id);
    reloadList();
}

void CalibrationDialog::onSetActive()
{
    const QString id = m_idEdit->text().trimmed();
    if (!m_edited.setActiveId(id)) {
        QMessageBox::warning(this, QStringLiteral("标定"), QStringLiteral("请先保存该标定"));
        return;
    }
    m_activeLabel->setText(QStringLiteral("当前标定: %1").arg(id));
}
