#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QSpinBox>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QElapsedTimer>
#include <QMap>
#include <QSet>
#include <QStyle>
#include "downloadmanager.h"
#include "modelparser.h"
#include "modelservice.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onFetchFilesClicked();
    void onDownloadAllClicked();
    void onDownloadSelectedClicked();
    void onDownloadItemClicked(int fileIdx);
    void onFileDoubleClicked(QListWidgetItem *item);
    void onOpenFolderClicked();
    void onJsonFetched(const QByteArray &data, const QString &context);
    void onJsonFetchError(const QString &error);

private:
    void setupUi();
    void applyStyleSheet();
    void setUiEnabled(bool enabled);
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
    void updateItemWidgets();
    void populateFileList(ModelParser::Source source, const QStringList &names, const QList<qint64> &sizes, bool allowGgufFilter = true);
    void chooseModelIdSource();

    QLineEdit *m_urlEdit;
    QPushButton *m_fetchBtn;
    QPushButton *m_downloadAllBtn;
    QPushButton *m_downloadSelBtn;
    QPushButton *m_openFolderBtn;
    QSpinBox *m_threadSpin;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QListWidget *m_fileList;

    ModelParser::ModelInfo m_modelInfo;

    QStringList m_fileNames;
    QList<qint64> m_fileSizes;
    ModelParser::Source m_downloadSource;
    bool m_ggufMode;
    bool m_discoveringModelId;
    ModelService::FileList m_modelscopeFiles;
    ModelService::FileList m_huggingfaceFiles;

    QList<DownloadManager*> m_pool;
    QMap<DownloadManager*, int> m_activeMap;
    QList<int> m_pendingIndices;

    QMap<int, qint64> m_slotReceived;
    QMap<int, qint64> m_slotTotal;
    QStringList m_itemBaseTexts;
    QList<int> m_currentTaskIndices;
    QSet<int> m_successIndices;
    QSet<int> m_failedIndices;

    int m_maxConcurrent;
    int m_totalFiles;
    int m_completedFiles;
    int m_failedFiles;
    QString m_downloadDir;
    qint64 m_batchTotalSize;
    QElapsedTimer m_displayThrottle;
};
