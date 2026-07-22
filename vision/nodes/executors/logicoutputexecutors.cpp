#include "vision/nodes/imageexecutorhelpers.h"
#include "vision/registry/visionnodeids.h"
#include "vision/runtime/nodeexecutorregistry.h"
#include "vision/runtime/resultsink.h"
#include "vision/model/subflowsupport.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <cmath>
#include <memory>

namespace Selt {
namespace {

class BoolAndExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::boolAnd(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        const bool a = request.inputs.value(QStringLiteral("a")).toBool(
            resolveBoolParam(request, QStringLiteral("a"), true));
        const bool b = request.inputs.value(QStringLiteral("b")).toBool(
            resolveBoolParam(request, QStringLiteral("b"), true));
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("ok"), DataValue(a && b));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class BoolOrExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::boolOr(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        const bool a = request.inputs.value(QStringLiteral("a")).toBool(false);
        const bool b = request.inputs.value(QStringLiteral("b")).toBool(false);
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("ok"), DataValue(a || b));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class BoolNotExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::boolNot(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        const bool a = request.inputs.value(QStringLiteral("a")).toBool(false);
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("ok"), DataValue(!a));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class NumericCompareExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::numericCompare(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        const double value = resolveRealInput(request, QStringLiteral("value"), 0.0);
        const double ref = resolveRealInput(request, QStringLiteral("reference"), 0.0);
        const QString op = resolveStringParam(request, QStringLiteral("op"), QStringLiteral("eq"));
        const double eps = resolveRealInput(request, QStringLiteral("epsilon"), 1e-6);
        bool ok = false;
        if (op == QLatin1String("gt"))
            ok = value > ref;
        else if (op == QLatin1String("lt"))
            ok = value < ref;
        else if (op == QLatin1String("ge"))
            ok = value >= ref;
        else if (op == QLatin1String("le"))
            ok = value <= ref;
        else
            ok = std::abs(value - ref) <= eps;
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("ok"), DataValue(ok));
        r.outputs.insert(QStringLiteral("state"),
                         DataValue(ok ? QStringLiteral("ok") : QStringLiteral("ng")));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class SwitchExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::switchNode(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        const bool cond = request.inputs.value(QStringLiteral("condition")).toBool(false);
        const DataValue selected = cond ? request.inputs.value(QStringLiteral("a"))
                                        : request.inputs.value(QStringLiteral("b"));
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("out"), selected);
        if (selected.type() == DataTypeId::Image)
            r.outputs.insert(QStringLiteral("image"), selected);
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class CounterExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::counter(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        QElapsedTimer t;
        t.start();
        const QString key = resolveStringParam(request, QStringLiteral("key"), QStringLiteral("counter"));
        const bool reset = resolveBoolParam(request, QStringLiteral("reset"), false);
        int count = context.variable(key).toInt(0);
        if (reset)
            count = 0;
        const bool ok = request.inputs.value(QStringLiteral("ok")).toBool(true);
        ++count;
        int okCount = context.variable(key + QStringLiteral(".ok")).toInt(0);
        int ngCount = context.variable(key + QStringLiteral(".ng")).toInt(0);
        if (reset) {
            okCount = 0;
            ngCount = 0;
        }
        if (ok)
            ++okCount;
        else
            ++ngCount;
        context.setVariable(key, DataValue(count));
        context.setVariable(key + QStringLiteral(".ok"), DataValue(okCount));
        context.setVariable(key + QStringLiteral(".ng"), DataValue(ngCount));
        if (auto *snap = context.variableScopeSnapshot()) {
            snap->runtimeWrite(key, DataValue(count));
            snap->runtimeWrite(key + QStringLiteral(".ok"), DataValue(okCount));
            snap->runtimeWrite(key + QStringLiteral(".ng"), DataValue(ngCount));
        }
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("count"), DataValue(count));
        r.outputs.insert(QStringLiteral("okCount"), DataValue(okCount));
        r.outputs.insert(QStringLiteral("ngCount"), DataValue(ngCount));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class AggregateExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::aggregate(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &context) override
    {
        QElapsedTimer t;
        t.start();
        const QString key = resolveStringParam(request, QStringLiteral("key"), QStringLiteral("aggregate"));
        const double value = resolveRealInput(request, QStringLiteral("value"), 0.0);
        const int n = context.variable(key + QStringLiteral(".n")).toInt(0) + 1;
        const double sum = context.variable(key + QStringLiteral(".sum")).toReal(0.0) + value;
        context.setVariable(key + QStringLiteral(".n"), DataValue(n));
        context.setVariable(key + QStringLiteral(".sum"), DataValue(sum));
        if (auto *snap = context.variableScopeSnapshot()) {
            snap->runtimeWrite(key + QStringLiteral(".n"), DataValue(n));
            snap->runtimeWrite(key + QStringLiteral(".sum"), DataValue(sum));
            snap->runtimeWrite(key + QStringLiteral(".mean"), DataValue(sum / n));
        }
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("count"), DataValue(n));
        r.outputs.insert(QStringLiteral("sum"), DataValue(sum));
        r.outputs.insert(QStringLiteral("mean"), DataValue(sum / n));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class ResultWriterExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::resultWriter(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        ResultRecord record;
        record.nodeId = request.nodeId;
        record.batchId = resolveStringParam(request, QStringLiteral("batchId"));
        record.frameId = resolveStringParam(request, QStringLiteral("frameId"));
        if (request.inputs.contains(QStringLiteral("measurement")))
            record.measurement = request.inputs.value(QStringLiteral("measurement")).toMeasurement();
        record.decisionState = request.inputs.contains(QStringLiteral("state"))
            ? request.inputs.value(QStringLiteral("state")).toString()
            : record.measurement.decisionState;
        record.timestamp = QDateTime::currentDateTimeUtc();
        const QStringList errors = ResultSinkRegistry::instance().publishAll(record);
        ExecutionResult r;
        r.outputs.insert(QStringLiteral("ok"), DataValue(errors.isEmpty()));
        if (!errors.isEmpty())
            r.outputs.insert(QStringLiteral("error"), DataValue(errors.join(QLatin1Char(';'))));
        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

class SubflowExecutor : public INodeExecutor
{
public:
    QString typeId() const override { return VisionNodeIds::subflow(); }
    ExecutionResult execute(const ExecutionRequest &request, ExecutionContext &) override
    {
        QElapsedTimer t;
        t.start();
        ExecutionResult r;
        QString error;
        const SubflowInterface iface = subflowInterfaceFromParameters(request.parameters);
        if (!iface.flowId.isEmpty() && !iface.validateScope(&error)) {
            r.status = ModuleStatus::Failed;
            r.errorMessage = error;
            r.diagnosticCode = QStringLiteral("subflow_interface_invalid");
            r.elapsedMs = t.elapsed();
            return r;
        }

        // Terminal bridge MVP: map outer inputs onto outer outputs by id / image aliases.
        // Nested inner-flow execution is deferred to ProjectService orchestration.
        auto copyInput = [&](const QString &id) {
            if (request.inputs.contains(id))
                r.outputs.insert(id, request.inputs.value(id));
        };
        for (const TypedPortDef &out : iface.outputs)
            copyInput(out.id);
        copyInput(QStringLiteral("in"));
        copyInput(QStringLiteral("out"));
        copyInput(QStringLiteral("image"));

        if (r.outputs.contains(QStringLiteral("out")) && !r.outputs.contains(QStringLiteral("image")))
            r.outputs.insert(QStringLiteral("image"), r.outputs.value(QStringLiteral("out")));
        if (r.outputs.contains(QStringLiteral("image")) && !r.outputs.contains(QStringLiteral("out")))
            r.outputs.insert(QStringLiteral("out"), r.outputs.value(QStringLiteral("image")));
        if (!r.outputs.contains(QStringLiteral("out")) && request.inputs.contains(QStringLiteral("in")))
            r.outputs.insert(QStringLiteral("out"), request.inputs.value(QStringLiteral("in")));
        if (!r.outputs.contains(QStringLiteral("image")) && request.inputs.contains(QStringLiteral("image")))
            r.outputs.insert(QStringLiteral("image"), request.inputs.value(QStringLiteral("image")));

        r.status = ModuleStatus::Success;
        r.elapsedMs = t.elapsed();
        return r;
    }
};

} // namespace

void registerLogicOutputExecutors(NodeExecutorRegistry &reg)
{
    reg.registerFactory(VisionNodeIds::boolAnd(), [] { return std::make_shared<BoolAndExecutor>(); });
    reg.registerFactory(VisionNodeIds::boolOr(), [] { return std::make_shared<BoolOrExecutor>(); });
    reg.registerFactory(VisionNodeIds::boolNot(), [] { return std::make_shared<BoolNotExecutor>(); });
    reg.registerFactory(VisionNodeIds::numericCompare(), [] { return std::make_shared<NumericCompareExecutor>(); });
    reg.registerFactory(VisionNodeIds::switchNode(), [] { return std::make_shared<SwitchExecutor>(); });
    reg.registerFactory(VisionNodeIds::counter(), [] { return std::make_shared<CounterExecutor>(); });
    reg.registerFactory(VisionNodeIds::aggregate(), [] { return std::make_shared<AggregateExecutor>(); });
    reg.registerFactory(VisionNodeIds::resultWriter(), [] { return std::make_shared<ResultWriterExecutor>(); });
    reg.registerFactory(VisionNodeIds::subflow(), [] { return std::make_shared<SubflowExecutor>(); });
}

} // namespace Selt
