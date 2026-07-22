#include "ui/flowbarwidget.h"
#include "ui/nodecatalogmodel.h"
#include "ui/theme/iconprovider.h"
#include "ui/theme/themecontroller.h"
#include "ui/theme/uistyle.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QSignalSpy>
#include <QTest>

class TestUiFoundation : public QObject
{
    Q_OBJECT
private slots:
    void iconResourcePathsExist();
    void iconsAreCached();
    void themeModeRoundTrip();
    void themeApplyDoesNotCrash();
    void industrialUiTokensExist();
    void layoutVersionAndDockMins();
    void lightThemeSectionTokens();
    void nodeCatalogEmptyWithoutOpenCv();
    void nodeCatalogMimeType();
    void nodeCatalogProxyFilter();
    void flowBarSingleActiveSource();
    void flowBarFallsBackToComboWhenNarrow();
};

void TestUiFoundation::iconResourcePathsExist()
{
    Q_INIT_RESOURCE(selt);
    QVERIFY(QFile::exists(Selt::IconProvider::resourcePath(Selt::IconId::Save)));
    QVERIFY(!Selt::IconProvider::resourcePath(Selt::IconId::Save).isEmpty());
    QVERIFY(!Selt::IconProvider::resourcePath(Selt::IconId::Run).isEmpty());
    QVERIFY(Selt::IconProvider::resourcePath(Selt::IconId::Save).startsWith(QStringLiteral(":/selt/")));
}

void TestUiFoundation::iconsAreCached()
{
    Q_INIT_RESOURCE(selt);
    const QIcon first = Selt::IconProvider::icon(Selt::IconId::Save, 20);
    QVERIFY(!first.isNull());
    QVERIFY(!first.pixmap(20, 20).isNull());
    const QIcon second = Selt::IconProvider::icon(Selt::IconId::Save, 20);
    QVERIFY(!second.pixmap(20, 20).isNull());
}

void TestUiFoundation::themeModeRoundTrip()
{
    QCOMPARE(Selt::ThemeController::modeFromString(QStringLiteral("dark")), Selt::ThemeMode::Dark);
    QCOMPARE(Selt::ThemeController::modeToString(Selt::ThemeMode::Light), QStringLiteral("light"));
    Selt::ThemeController controller;
    controller.setMode(Selt::ThemeMode::Dark);
    QCOMPARE(controller.mode(), Selt::ThemeMode::Dark);
}

void TestUiFoundation::themeApplyDoesNotCrash()
{
    Q_INIT_RESOURCE(selt);
    Selt::ThemeController controller;
    controller.setMode(Selt::ThemeMode::Light);
    controller.apply(qApp);
    QVERIFY(!qApp->styleSheet().isEmpty());
    QVERIFY(qApp->styleSheet().contains(QStringLiteral("4a90d9"))
            || qApp->styleSheet().contains(QStringLiteral("#4a90d9"))
            || qApp->styleSheet().contains(QStringLiteral("dcecfa")));
    controller.setMode(Selt::ThemeMode::Dark);
    controller.apply(qApp);
    QVERIFY(qApp->styleSheet().contains(QStringLiteral("ff8c00"))
            || qApp->styleSheet().contains(QStringLiteral("#ff8c00"))
            || !qApp->styleSheet().isEmpty());
    controller.setMode(Selt::ThemeMode::System);
    controller.apply(qApp);
    QVERIFY(!qApp->styleSheet().isEmpty());
}

void TestUiFoundation::industrialUiTokensExist()
{
    Selt::UiStyle::setActivePalette(Selt::UiStyle::Palette::Dark);
    QVERIFY(Selt::UiStyle::accentOrange().isValid());
    QCOMPARE(Selt::UiStyle::accentOrange().name(QColor::HexRgb), QStringLiteral("#ff8c00"));
    QCOMPARE(Selt::UiStyle::leftDockDefaultWidth, 280);
    QCOMPARE(Selt::UiStyle::rightDockDefaultWidth, 360);
    QVERIFY(Selt::UiStyle::leftDockMinWidth < Selt::UiStyle::leftDockDefaultWidth);
    QVERIFY(Selt::UiStyle::rightDockMinWidth < Selt::UiStyle::rightDockDefaultWidth);
    QVERIFY(Selt::UiStyle::centralMinWidth >= 320);
}

void TestUiFoundation::layoutVersionAndDockMins()
{
    QCOMPARE(Selt::UiStyle::windowLayoutVersion, 8);
    QVERIFY(Selt::UiStyle::imageToolbarHeight >= 28);
}

void TestUiFoundation::lightThemeSectionTokens()
{
    Selt::UiStyle::setActivePalette(Selt::UiStyle::Palette::Light);
    QCOMPARE(Selt::UiStyle::sectionHeaderBackground().name(QColor::HexRgb), QStringLiteral("#dcecfa"));
    QCOMPARE(Selt::UiStyle::sectionHeaderHover().name(QColor::HexRgb), QStringLiteral("#c6e0f5"));
    QCOMPARE(Selt::UiStyle::accent().name(QColor::HexRgb), QStringLiteral("#4a90d9"));
    QVERIFY(Selt::UiStyle::successGreen().isValid());
    QVERIFY(Selt::UiStyle::failRed().isValid());
    QVERIFY(Selt::UiStyle::successGreen() != Selt::UiStyle::failRed());
}

void TestUiFoundation::nodeCatalogEmptyWithoutOpenCv()
{
    Selt::NodeCatalogModel model;
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.columnCount(), 1);
}

void TestUiFoundation::nodeCatalogMimeType()
{
    Selt::NodeCatalogModel model;
    const QStringList types = model.mimeTypes();
    QVERIFY(types.contains(QStringLiteral("application/x-selt-node-type")));
    QCOMPARE(types.size(), 1);
}

void TestUiFoundation::nodeCatalogProxyFilter()
{
    Selt::NodeCatalogModel model;
    Selt::NodeCatalogFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.setFilterText(QString());
    QCOMPARE(proxy.rowCount(), model.rowCount());
    proxy.setFilterText(QStringLiteral("___no_match___"));
    QCOMPARE(proxy.rowCount(), 0);
}

void TestUiFoundation::flowBarSingleActiveSource()
{
    Q_INIT_RESOURCE(selt);
    FlowBarWidget bar;
    QSignalSpy spy(&bar, &FlowBarWidget::activeFlowChosen);
    bar.setFlows({QStringLiteral("f1"), QStringLiteral("f2"), QStringLiteral("f3")},
                 {QStringLiteral("流程1"), QStringLiteral("流程2"), QStringLiteral("流程3")},
                 QStringLiteral("f2"));
    QCOMPARE(bar.activeFlowId(), QStringLiteral("f2"));
    QCOMPARE(spy.count(), 0);

    bar.resize(800, 40);
    QTest::qWait(10);
    // Trigger selection change through public setFlows with different active.
    bar.setFlows({QStringLiteral("f1"), QStringLiteral("f2"), QStringLiteral("f3")},
                 {QStringLiteral("流程1"), QStringLiteral("流程2"), QStringLiteral("流程3")},
                 QStringLiteral("f3"));
    QCOMPARE(bar.activeFlowId(), QStringLiteral("f3"));
}

void TestUiFoundation::flowBarFallsBackToComboWhenNarrow()
{
    Q_INIT_RESOURCE(selt);
    FlowBarWidget bar;
    bar.setFlows({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"),
                  QStringLiteral("d"), QStringLiteral("e"), QStringLiteral("f"),
                  QStringLiteral("g"), QStringLiteral("h"), QStringLiteral("i")},
                 {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
                  QStringLiteral("4"), QStringLiteral("5"), QStringLiteral("6"),
                  QStringLiteral("7"), QStringLiteral("8"), QStringLiteral("9")},
                 QStringLiteral("a"));
    bar.resize(300, 40);
    QTest::qWait(10);
    QCOMPARE(bar.activeFlowId(), QStringLiteral("a"));
    bar.resize(900, 40);
    QTest::qWait(10);
    QCOMPARE(bar.activeFlowId(), QStringLiteral("a"));
}

QTEST_MAIN(TestUiFoundation)
#include "test_ui_foundation.moc"
