#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ModelLink");
    app.setApplicationVersion("1.0.0");

    MainWindow window;
    window.setWindowTitle("ModelLink - 模型下载工具");
    window.resize(700, 520);
    window.show();

    return app.exec();
}
