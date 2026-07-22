#include "ui/mainwindow.h"

#include "foundation/seltversion.h"
#if SELT_HAS_OPENCV
#include "plugin_host/pluginhost.h"
#include "vision/nodes/builtinexecutors.h"
#endif

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QIcon>

// Force-link Qt resources compiled from resources/selt.qrc
static void initSeltResources()
{
    Q_INIT_RESOURCE(selt);
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    initSeltResources();
    QApplication::setOrganizationName(QStringLiteral("Selt"));
    QApplication::setApplicationName(Selt::VersionInfo::productName());
    QApplication::setApplicationVersion(Selt::VersionInfo::productVersionString());
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/selt/icons/app_icon.svg")));

#if SELT_HAS_OPENCV
    Selt::registerBuiltInOpenCvExecutors();
    const QString pluginDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins/nodes"));
    Selt::PluginHost::instance().loadFromDirectory(pluginDir);
#endif

    MainWindow w;
    w.show();
    return QApplication::exec();
}
