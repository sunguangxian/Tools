#include "MainWindow.h"

#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("PortMonitor"));
    QApplication::setOrganizationName(QStringLiteral("Tools"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSize(9);
    app.setFont(font);
    app.setStyle(QStringLiteral("Fusion"));

    QString pcap;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if ((args.at(i) == QLatin1String("--open-pcap") || args.at(i) == QLatin1String("-p")) && i + 1 < args.size())
            pcap = args.at(++i);
    }

    MainWindow window;
    window.show();
    if (!pcap.isEmpty())
        window.openPcap(pcap);
    return app.exec();
}
