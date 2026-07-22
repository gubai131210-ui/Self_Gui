#include "vision/runtime/samplevalidation.h"

#include "vision/nodes/builtinexecutors.h"
#include "vision/registry/visionnodeids.h"
#include "vision/registry/visionnoderegistry.h"

#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

using namespace Selt;

class TestSampleValidation : public QObject
{
    Q_OBJECT
private slots:
    void labelsAndSummary();
    void batchRunsLocalImages();
};

void TestSampleValidation::labelsAndSummary()
{
    QCOMPARE(sampleLabelToString(SampleLabel::Ok), QStringLiteral("ok"));
    QCOMPARE(sampleLabelFromString(QStringLiteral("PASS")), SampleLabel::Ok);
    QCOMPARE(sampleLabelFromString(QStringLiteral("fail")), SampleLabel::Ng);
    QCOMPARE(sampleLabelFromString(QStringLiteral("other")), SampleLabel::Unknown);

    SampleValidationSummary summary;
    SampleRunResult ok;
    ok.sample.expected = SampleLabel::Ok;
    ok.matched = true;
    summary.results.append(ok);
    ++summary.matchedCount;
    ++summary.expectedOk;
    QCOMPARE(summary.processedCount(), 1);
    QCOMPARE(summary.matchRate(), 1.0);
}

void TestSampleValidation::batchRunsLocalImages()
{
    registerBuiltInOpenCvExecutors();
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("sample.png"));
    QImage image(32, 32, QImage::Format_Grayscale8);
    image.fill(255);
    QVERIFY(image.save(imagePath));

    Document document;
    NodeModel loader = VisionNodeRegistry::create(VisionNodeIds::imageLoader(), QPointF());
    loader.id = QStringLiteral("loader");
    document.addNode(loader);

    VisionSample sample;
    sample.id = QStringLiteral("sample-1");
    sample.imagePath = imagePath;
    sample.expected = SampleLabel::Ok;
    const SampleValidationSummary summary =
        SampleValidationRunner::run(document, {sample}, {}, QStringLiteral("batch-1"));

    QCOMPARE(summary.results.size(), 1);
    QVERIFY(summary.results.first().executed);
    QVERIFY(summary.results.first().context.sourceDescription.contains(QStringLiteral("sample.png")));
    QCOMPARE(summary.results.first().sample.batchId, QString());
    QCOMPARE(summary.results.first().context.batchId, QStringLiteral("batch-1"));
}

QTEST_MAIN(TestSampleValidation)
#include "test_sample_validation.moc"
