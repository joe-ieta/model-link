#include "downloadmanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_reply(nullptr)
    , m_outputFile(nullptr)
    , m_existingBytes(0)
    , m_cancelled(false)
    , m_fetchMode(false)
    , m_resumeMode(false)
{
}

DownloadManager::~DownloadManager()
{
    cancelDownload();
}

QString DownloadManager::defaultDownloadDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
}

void DownloadManager::startDownload(const QUrl &url, const QString &savePath)
{
    if (m_reply) {
        emit downloadError("下载正在进行中");
        return;
    }

    m_cancelled = false;
    m_fetchMode = false;
    m_savePath = savePath;
    m_partPath = savePath + ".part";
    m_existingBytes = 0;
    m_speedSamples.clear();

    // Check for existing partial download (resume support)
    QFileInfo partInfo(m_partPath);
    if (partInfo.exists() && partInfo.size() > 0) {
        m_existingBytes = partInfo.size();
        m_resumeMode = true;
        startDownloadInternal(url, savePath, m_existingBytes);
    } else {
        m_resumeMode = false;
        startDownloadInternal(url, savePath, 0);
    }
}

void DownloadManager::startDownloadInternal(const QUrl &url, const QString &savePath, qint64 resumeFrom)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "ModelLink/1.0");

    if (resumeFrom > 0) {
        QByteArray rangeHeader = "bytes=" + QByteArray::number(resumeFrom) + "-";
        request.setRawHeader("Range", rangeHeader);
        emit statusMessage(QString("从断点续传 (已有 %1 bytes)...").arg(resumeFrom));
    }

    // Open file for writing (append if resuming)
    if (resumeFrom > 0) {
        m_outputFile = new QFile(m_partPath);
        if (!m_outputFile->open(QIODevice::Append)) {
            emit downloadError("无法打开文件: " + m_partPath);
            cleanupDownload();
            return;
        }
    } else {
        m_outputFile = new QFile(m_partPath);
        if (!m_outputFile->open(QIODevice::WriteOnly)) {
            emit downloadError("无法创建文件: " + m_partPath);
            cleanupDownload();
            return;
        }
    }

    m_reply = m_nam->get(request);
    m_startTime.start();
    m_speedSamples.clear();

    connect(m_reply, &QNetworkReply::readyRead, this, &DownloadManager::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &DownloadManager::onFinished);
    connect(m_reply, &QNetworkReply::errorOccurred, this, &DownloadManager::onErrorOccurred);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &DownloadManager::onDownloadProgress);

    emit statusMessage("正在连接服务器...");
}

void DownloadManager::cancelDownload()
{
    m_cancelled = true;
    if (m_reply) {
        m_reply->abort();
    }
    cleanupDownload();
}

bool DownloadManager::isDownloading() const
{
    return m_reply != nullptr;
}

void DownloadManager::fetchJson(const QUrl &url)
{
    if (m_reply) {
        emit jsonFetchError("已有操作在进行中");
        return;
    }

    m_cancelled = false;
    m_fetchMode = true;
    m_jsonBuffer.clear();

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "ModelLink/1.0");
    request.setRawHeader("Accept", "application/json");

    m_reply = m_nam->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &DownloadManager::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &DownloadManager::onFinished);
    connect(m_reply, &QNetworkReply::errorOccurred, this, &DownloadManager::onErrorOccurred);

    emit statusMessage("获取文件列表...");
}

void DownloadManager::onReadyRead()
{
    if (!m_reply) return;

    if (m_fetchMode) {
        m_jsonBuffer.append(m_reply->readAll());
    } else if (m_outputFile) {
        m_outputFile->write(m_reply->readAll());
    }
}

void DownloadManager::onFinished()
{
    if (!m_reply) return;

    if (m_fetchMode) {
        QByteArray data = m_jsonBuffer + m_reply->readAll();
        m_jsonBuffer.clear();
        m_fetchMode = false;

        if (!m_cancelled) {
            QString urlPath = m_reply->url().toString();
            emit jsonFetched(data, urlPath);
        }
        m_reply->deleteLater();
        m_reply = nullptr;
        return;
    }

    if (m_outputFile) {
        // Read any remaining data
        QByteArray remainder = m_reply->readAll();
        if (!remainder.isEmpty()) {
            m_outputFile->write(remainder);
        }
    }

    if (!m_cancelled && m_outputFile) {
        finishDownload();
    } else {
        cleanupDownload();
    }

    m_reply->deleteLater();
    m_reply = nullptr;
}

void DownloadManager::finishDownload()
{
    m_outputFile->close();
    delete m_outputFile;
    m_outputFile = nullptr;

    // Remove target file if exists, rename .part to target
    if (QFile::exists(m_savePath)) {
        QFile::remove(m_savePath);
    }

    if (QFile::rename(m_partPath, m_savePath)) {
        emit statusMessage("下载完成");
        emit downloadFinished(m_savePath);
    } else {
        emit downloadError("无法重命名文件: " + m_partPath + " -> " + m_savePath);
    }
}

void DownloadManager::onErrorOccurred(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)
    if (!m_reply || m_cancelled) return;

    if (m_fetchMode) {
        emit jsonFetchError(m_reply->errorString());
    } else {
        emit downloadError(m_reply->errorString());
        cleanupDownload();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

void DownloadManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (m_cancelled || m_fetchMode) return;

    qint64 totalReceived = m_existingBytes + bytesReceived;
    qint64 totalSize = m_existingBytes + bytesTotal;

    // Sliding window speed (5s average)
    qint64 nowMs = m_startTime.elapsed();
    m_speedSamples.append({nowMs, bytesReceived});

    // Remove samples older than 5 seconds
    while (!m_speedSamples.isEmpty() && m_speedSamples.first().first < nowMs - 5000) {
        m_speedSamples.removeFirst();
    }

    int speed = 0;
    if (m_speedSamples.size() >= 2) {
        qint64 dt = m_speedSamples.last().first - m_speedSamples.first().first;
        qint64 db = m_speedSamples.last().second - m_speedSamples.first().second;
        if (dt > 0) {
            speed = static_cast<int>(db * 1000 / dt);
        }
    } else if (m_speedSamples.size() == 1) {
        // Single sample: use since start
        qint64 elapsed = m_speedSamples.first().first;
        if (elapsed > 0) {
            speed = static_cast<int>(m_speedSamples.first().second * 1000 / elapsed);
        }
    }

    emit downloadProgress(totalReceived, totalSize, speed);
}

void DownloadManager::cleanupDownload()
{
    if (m_outputFile) {
        m_outputFile->close();
        delete m_outputFile;
        m_outputFile = nullptr;
    }
}
