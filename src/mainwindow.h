#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QElapsedTimer>
#include <QMap>
#include "downloadmanager.h"
#include "modelparser.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onFetchFilesClicked();
    void onDownloadClicked();
    void onOpenFolderClicked();
    void onFileListItemClicked(QListWidgetItem *item);
    void onFileDoubleClicked(QListWidgetItem *item);
    void onJsonFetched(const QByteArray &data, const QString &context);
    void onJsonFetchError(const QString &error);

private:
    void setupUi();
    void setUiEnabled(bool enabled);
    bool checkFileOverwrite(const QString &filePath);
    void tryFetchFromModelscope();
    void tryFetchFromHuggingface();
    void filterGgufFiles(QStringList &names, QList<qint64> &sizes);
    void prepareDownloadDir();
    void startDownloads(const QList<int> &indices);
    void fillDownloadSlots();
    void onSlotProgress(int fileIdx, qint64 received, qint64 total, int speed);
    void onSlotFinished(int fileIdx, const QString &filePath);
    void onSlotError(int fileIdx, const QString &error);
    void updateOverallProgress();
    void onBatchComplete();

    QLineEdit *m_urlEdit;
    QPushButton *m_fetchBtn;
    QPushButton *m_downloadBtn;
    QPushButton *m_openFolderBtn;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QListWidget *m_fileList;

    ModelParser::ModelInfo m_modelInfo;

    QStringList m_fileNames;
    QList<qint64> m_fileSizes;
    bool m_fileListMode;
    ModelParser::Source m_downloadSource;
    bool m_ggufMode;

    // Concurrent download pool
    QList<DownloadManager*> m_pool;
    QMap<DownloadManager*, int> m_activeMap;   // mgr -> fileIndex
    QList<int> m_pendingIndices;               // waiting file indices

    QMap<int, qint64> m_slotReceived;
    QMap<int, qint64> m_slotTotal;
    QStringList m_itemBaseTexts;

    int m_maxConcurrent;
    int m_totalFiles;
    int m_completedFiles;
    int m_failedFiles;
    QString m_downloadDir;
    qint64 m_batchTotalSize;
    QElapsedTimer m_displayThrottle;
};
