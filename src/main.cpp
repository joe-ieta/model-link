#include <QApplication>
#include <QFont>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("ModelLink");
    app.setApplicationVersion("1.0.0");

    QFont font = app.font();
    font.setPointSize(10);
    app.setFont(font);

    MainWindow window;
    window.setWindowTitle("ModelLink - 模型下载工具");
    window.resize(780, 560);
    window.setMinimumSize(640, 400);
    window.show();

    return app.exec();
}
