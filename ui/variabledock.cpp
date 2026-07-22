#include "ui/variabledock.h"

#include "vision/data/datatype.h"
#include "vision/domain/variablescope.h"
#include "vision/domain/visionproject.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace Selt;

VariableDock::VariableDock(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    m_hintLabel = new QLabel(
        QStringLiteral("作用域链：全局 → 工程 → 流程 → 分组 → 算子（近处覆盖远处）"), this);
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    m_scopeCombo = new QComboBox(this);
    m_scopeCombo->addItem(QStringLiteral("全局"), QStringLiteral("global"));
    m_scopeCombo->addItem(QStringLiteral("工程"), QStringLiteral("project"));
    m_scopeCombo->addItem(QStringLiteral("当前流程"), QStringLiteral("flow"));
    layout->addWidget(m_scopeCombo);

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("名称"), QStringLiteral("类型"), QStringLiteral("值"), QStringLiteral("ID")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table, 1);

    auto *form = new QHBoxLayout;
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(QStringLiteral("名称"));
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(QStringLiteral("实数"), static_cast<int>(DataTypeId::Real));
    m_typeCombo->addItem(QStringLiteral("整数"), static_cast<int>(DataTypeId::Int));
    m_typeCombo->addItem(QStringLiteral("布尔"), static_cast<int>(DataTypeId::Bool));
    m_typeCombo->addItem(QStringLiteral("字符串"), static_cast<int>(DataTypeId::String));
    m_valueEdit = new QLineEdit(this);
    m_valueEdit->setPlaceholderText(QStringLiteral("初始值"));
    m_addButton = new QPushButton(QStringLiteral("添加"), this);
    m_removeButton = new QPushButton(QStringLiteral("删除"), this);
    form->addWidget(m_nameEdit, 1);
    form->addWidget(m_typeCombo);
    form->addWidget(m_valueEdit, 1);
    form->addWidget(m_addButton);
    form->addWidget(m_removeButton);
    layout->addLayout(form);

    connect(m_scopeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &VariableDock::onScopeChanged);
    connect(m_addButton, &QPushButton::clicked, this, &VariableDock::onAdd);
    connect(m_removeButton, &QPushButton::clicked, this, &VariableDock::onRemove);
}

void VariableDock::setProject(VisionProject *project)
{
    m_project = project;
    reload();
}

ProjectVariableStore *VariableDock::activeStore() const
{
    const QString scope = m_scopeCombo ? m_scopeCombo->currentData().toString() : QStringLiteral("project");
    if (scope == QLatin1String("global"))
        return &GlobalVariableStore::instance().store();
    if (!m_project)
        return nullptr;
    if (scope == QLatin1String("flow")) {
        VisionFlow *flow = m_project->activeFlow();
        return flow ? &flow->flowVariables() : nullptr;
    }
    return &m_project->variables();
}

QString VariableDock::scopeLabel() const
{
    return m_scopeCombo ? m_scopeCombo->currentText() : QStringLiteral("工程");
}

void VariableDock::reload()
{
    m_table->setRowCount(0);
    ProjectVariableStore *store = activeStore();
    if (!store)
        return;
    const QVector<ProjectVariableRecord> records = store->records();
    m_table->setRowCount(records.size());
    for (int i = 0; i < records.size(); ++i) {
        const ProjectVariableRecord &rec = records.at(i);
        m_table->setItem(i, 0, new QTableWidgetItem(rec.name));
        m_table->setItem(i, 1, new QTableWidgetItem(dataTypeIdDisplayName(rec.dataType)));
        QString valueText;
        if (rec.dataType == DataTypeId::Bool)
            valueText = rec.value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        else if (rec.dataType == DataTypeId::Int)
            valueText = QString::number(rec.value.toInt());
        else if (rec.dataType == DataTypeId::Real)
            valueText = QString::number(rec.value.toReal());
        else
            valueText = rec.value.toString();
        m_table->setItem(i, 2, new QTableWidgetItem(valueText));
        m_table->setItem(i, 3, new QTableWidgetItem(rec.id));
    }
}

void VariableDock::onScopeChanged()
{
    reload();
}

void VariableDock::onAdd()
{
    ProjectVariableStore *store = activeStore();
    if (!store)
        return;
    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        emit diagnosticsReady({QStringLiteral("请填写变量名称")});
        return;
    }
    const auto type = static_cast<DataTypeId>(m_typeCombo->currentData().toInt());
    DataValue value;
    const QString text = m_valueEdit->text();
    switch (type) {
    case DataTypeId::Bool:
        value = DataValue(text == QLatin1String("1") || text.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0);
        break;
    case DataTypeId::Int:
        value = DataValue(text.toInt());
        break;
    case DataTypeId::String:
        value = DataValue(text);
        break;
    case DataTypeId::Real:
    default:
        value = DataValue(text.toDouble());
        break;
    }
    const QString id = store->add(name, type, value);
    if (id.isEmpty()) {
        emit diagnosticsReady({QStringLiteral("添加变量失败")});
        return;
    }
    m_nameEdit->clear();
    m_valueEdit->clear();
    reload();
    emit diagnosticsReady({QStringLiteral("已在%1作用域添加变量: %2").arg(scopeLabel(), name)});
    if (m_project)
        m_project->setModified(true);
}

void VariableDock::onRemove()
{
    ProjectVariableStore *store = activeStore();
    if (!store)
        return;
    const int row = m_table->currentRow();
    if (row < 0)
        return;
    QTableWidgetItem *idItem = m_table->item(row, 3);
    if (!idItem)
        return;
    store->remove(idItem->text());
    reload();
    emit diagnosticsReady({QStringLiteral("已删除变量")});
    if (m_project)
        m_project->setModified(true);
}
