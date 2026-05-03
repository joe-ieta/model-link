#pragma once

#include <QObject>
#include <QStringList>
#include "downloadmanager.h"
#include "modelservice.h"
#include "modelparser.h"

class CliRunner : public QObject
{
    Q_OBJECT
public:
    struct Options {
        QString model;
        bool ggufOnly = false;
        bool listOnly = false;
        bool downloadAll = false;
        QString outputDir;
        int threads = 3;
    };

    explicit CliRunner(const Options &opts, QObject *parent = nullptr);
    int run();
    void printHelp();

private slots:
    void onModelscopeFetched(const QByteArray &data, const QString &context);
    void onHuggingfaceFetched(const QByteArray &data, const QString &context);
    void onFetchError(const QString &error);
    void onDownloadFinished(const QString &filePath);
    void onDownloadError(const QString &error);

private:
    void fetchFileLists();
    void onBothFetched();
    void printFileList();
    void startDownloads();
    void startNextDownload();
    void printStatus(const QString &msg);
    QString outputPath(const QString &file) const;

    Options m_opts;
    ModelParser::ModelInfo m_modelInfo;
    ModelService::FileList m_msFiles;
    ModelService::FileList m_hfFiles;
    bool m_msDone = false;
    bool m_hfDone = false;
    QStringList m_selectedFiles;
    QList<qint64> m_selectedSizes;
    ModelParser::Source m_downloadSource = ModelParser::Invalid;
    int m_nextIndex = 0;
    int m_completed = 0;
    int m_failed = 0;
    int m_total = 0;
    int m_running = 0;
    DownloadManager *m_currentMgr = nullptr;
    QString m_outputDir;
};
