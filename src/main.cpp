#include <QApplication>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFont>
#include "mainwindow.h"
#include "clirunner.h"

int main(int argc, char *argv[])
{
    // Parse args before creating app to decide GUI vs CLI
    CliRunner::Options cliOpts;

    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "-h" || arg == "--help") {
            QCoreApplication cliApp(argc, argv);
            cliApp.setApplicationName("ModelLink");
            cliApp.setApplicationVersion("1.0.0");
            CliRunner runner(cliOpts);
            runner.printHelp();
            return 0;
        }
        if (arg == "--gguf") {
            cliOpts.ggufOnly = true;
        } else if (arg == "-a" || arg == "--all") {
            cliOpts.downloadAll = true;
        } else if (arg == "-l" || arg == "--list") {
            cliOpts.listOnly = true;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) cliOpts.outputDir = QString::fromLocal8Bit(argv[++i]);
        } else if (arg == "-t" || arg == "--threads") {
            if (i + 1 < argc) cliOpts.threads = QString::fromLocal8Bit(argv[++i]).toInt();
        } else if (!arg.startsWith('-')) {
            cliOpts.model = arg;
        }
    }

    if (!cliOpts.model.isEmpty()) {
        // CLI mode
        QCoreApplication cliApp(argc, argv);
        cliApp.setApplicationName("ModelLink");
        cliApp.setApplicationVersion("1.0.0");
        CliRunner runner(cliOpts);
        int ret = runner.run();
        return ret;
    }

    // GUI mode
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
