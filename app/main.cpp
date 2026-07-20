#include "ui/mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Selt"));
    QApplication::setApplicationName(QStringLiteral("Selt_Gui"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    MainWindow w;
    w.show();
    return QApplication::exec();
}
