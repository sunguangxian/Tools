#include "MainWindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("portmonitor-qt"));
    QApplication::setOrganizationName(QStringLiteral("Tools"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    MainWindow window;
    window.show();
    return app.exec();
}
