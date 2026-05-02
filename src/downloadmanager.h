#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QElapsedTimer>
#include <QUrl>
#include <QVector>

class DownloadManager : public QObject
{
    Q_OBJECT
public:
    explicit DownloadManager(QObject *parent = nullptr);
    ~DownloadManager();

    void startDownload(const QUrl &url, const QString &savePath);
    void cancelDownload();
    bool isDownloading() const;

    void fetchJson(const QUrl &url);

    static QString defaultDownloadDir();

signals:
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal, int speedBytesPerSec);
    void downloadFinished(const QString &filePath);
    void downloadError(const QString &errorMessage);
    void statusMessage(const QString &message);
    void jsonFetched(const QByteArray &data, const QString &context);
    void jsonFetchError(const QString &error);

private slots:
    void onReadyRead();
    void onFinished();
    void onErrorOccurred(QNetworkReply::NetworkError error);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    void startDownloadInternal(const QUrl &url, const QString &savePath, qint64 resumeFrom);
    void cleanupDownload();
    void finishDownload();

    QNetworkAccessManager *m_nam;
    QNetworkReply *m_reply;
    QFile *m_outputFile;
    QString m_savePath;
    QString m_partPath;
    qint64 m_existingBytes;
    QElapsedTimer m_startTime;
    QVector<QPair<qint64, qint64>> m_speedSamples;  // (elapsedMs, bytesReceived)
    bool m_cancelled;
    bool m_fetchMode;
    bool m_resumeMode;
    QByteArray m_jsonBuffer;
};
