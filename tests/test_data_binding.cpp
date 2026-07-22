#include "vision/data/datatype.h"
#include "vision/domain/visionflow.h"
#include "vision/model/moduleparamdef.h"
#include "vision/model/roi.h"

#include <QJsonObject>
#include <QtTest>

class DataBindingTest : public QObject
{
    Q_OBJECT
private slots:
    void dataTypeRoundTrip();
    void dataValueScalarsAndJson();
    void dataValueIntToReal();
    void dataValueRejectRealToInt();
    void parameterBindingRoundTrip();
    void parameterBindingLegacyKindInt();
    void parameterBindingLegacyConstant();
    void parameterBindingInvalid();
};

void DataBindingTest::dataTypeRoundTrip()
{
    using Selt::DataTypeId;
    const QVector<DataTypeId> ids = {
        DataTypeId::Bool, DataTypeId::Int, DataTypeId::Real, DataTypeId::String,
        DataTypeId::Image, DataTypeId::Roi, DataTypeId::Measurement, DataTypeId::Overlay,
        DataTypeId::Point2D, DataTypeId::Line, DataTypeId::Circle};
    for (DataTypeId id : ids) {
        bool ok = false;
        QCOMPARE(Selt::dataTypeIdFromString(Selt::dataTypeIdToString(id), &ok), id);
        QVERIFY(ok);
        QVERIFY(!Selt::dataTypeIdDisplayName(id).isEmpty());
    }
}

void DataBindingTest::dataValueScalarsAndJson()
{
    Selt::DataValue b(true);
    Selt::DataValue i(42);
    Selt::DataValue r(3.5);
    Selt::DataValue s(QStringLiteral("hello"));
    RoiRect roi;
    roi.enabled = true;
    roi.rect = QRectF(1, 2, 3, 4);
    Selt::DataValue roiValue(roi);

    auto roundTrip = [](const Selt::DataValue &v) {
        QString err;
        return Selt::DataValue::fromJson(v.toJson(), &err);
    };

    QCOMPARE(roundTrip(b).toBool(), true);
    QCOMPARE(roundTrip(i).toInt(), 42);
    QCOMPARE(roundTrip(r).toReal(), 3.5);
    QCOMPARE(roundTrip(s).toString(), QStringLiteral("hello"));
    QCOMPARE(roundTrip(roiValue).toRoi().rect, roi.rect);
    QVERIFY(b.isValidFor(Selt::DataTypeId::Bool));
    QVERIFY(!i.isNull());
}

void DataBindingTest::dataValueIntToReal()
{
    Selt::DataValue i(7);
    Selt::DataValue out;
    QString err;
    QVERIFY(i.convertTo(Selt::DataTypeId::Real, &out, &err));
    QCOMPARE(out.type(), Selt::DataTypeId::Real);
    QCOMPARE(out.toReal(), 7.0);
    QVERIFY(Selt::dataTypesCompatible(Selt::DataTypeId::Int, Selt::DataTypeId::Real));
}

void DataBindingTest::dataValueRejectRealToInt()
{
    Selt::DataValue r(2.5);
    Selt::DataValue out;
    QString err;
    QVERIFY(!r.convertTo(Selt::DataTypeId::Int, &out, &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(!Selt::dataTypesCompatible(Selt::DataTypeId::Real, Selt::DataTypeId::Int));
}

void DataBindingTest::parameterBindingRoundTrip()
{
    auto up = Selt::ParameterBinding::upstream(QStringLiteral("n1"), QStringLiteral("width"),
                                               Selt::DataTypeId::Real);
    QString err;
    auto restored = Selt::ParameterBinding::fromJson(up.toJson(), &err);
    QVERIFY(err.isEmpty());
    QCOMPARE(restored.kind, Selt::ParameterSourceKind::UpstreamBinding);
    QCOMPARE(restored.upstreamNodeId, QStringLiteral("n1"));
    QCOMPARE(restored.upstreamPortId, QStringLiteral("width"));
    QCOMPARE(restored.targetType, Selt::DataTypeId::Real);
    QVERIFY(restored.isValid());
}

void DataBindingTest::parameterBindingLegacyKindInt()
{
    QJsonObject obj{
        {QStringLiteral("kind"), 1},
        {QStringLiteral("upstreamNodeId"), QStringLiteral("a")},
        {QStringLiteral("upstreamPortId"), QStringLiteral("out")}};
    QString err;
    auto b = Selt::ParameterBinding::fromJson(obj, &err);
    QVERIFY(err.isEmpty());
    QCOMPARE(b.kind, Selt::ParameterSourceKind::UpstreamBinding);
}

void DataBindingTest::parameterBindingLegacyConstant()
{
    auto b = Selt::ParameterBinding::legacyConstantBinding(128, Selt::DataTypeId::Int);
    QCOMPARE(b.kind, Selt::ParameterSourceKind::Constant);
    QVERIFY(b.isValid());

    ModuleParamDef param;
    param.key = QStringLiteral("threshold");
    param.label = QStringLiteral("阈值");
    param.type = ModuleParamType::Int;
    param.minimum = 0;
    param.maximum = 255;

    VisionFlow flow(QStringLiteral("f1"), QStringLiteral("flow"));
    QString err;
    QVERIFY(b.validateAgainst(flow, param, &err));
}

void DataBindingTest::parameterBindingInvalid()
{
    QString err;
    auto bad = Selt::ParameterBinding::fromJson(
        QJsonObject{{QStringLiteral("kind"), QStringLiteral("nope")}}, &err);
    QVERIFY(!err.isEmpty());
    Q_UNUSED(bad);

    err.clear();
    auto unknownType = Selt::ParameterBinding::fromJson(
        QJsonObject{{QStringLiteral("kind"), QStringLiteral("constant")},
                    {QStringLiteral("targetType"), QStringLiteral("not-a-type")}},
        &err);
    QVERIFY(!err.isEmpty());
    Q_UNUSED(unknownType);
}

QTEST_MAIN(DataBindingTest)
#include "test_data_binding.moc"
