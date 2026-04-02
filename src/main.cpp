#include <QApplication>
#include "MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("Fat Particle Detector");
    app.setOrganizationName("Jetson Nano Lab");

    app.setWindowIcon(QIcon("resources/app_icon.ico"));

    MainWindow window;
    window.show();

    return app.exec();
}
