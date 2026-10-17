#include "MainWindow.h"
#include "codec.h"

#include <QApplication>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("sscom-qt"));
    QApplication::setOrganizationName(QStringLiteral("Tools"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    if (app.arguments().contains(QStringLiteral("--self-test"))) {
        QString error;
        const int code = codec::selfTest(&error);
        QTextStream out(code == 0 ? stdout : stderr);
        out << (code == 0 ? QStringLiteral("self-test ok\n") : error + QLatin1Char('\n'));
        return code;
    }

    MainWindow window;
    window.show();
    return app.exec();
}
