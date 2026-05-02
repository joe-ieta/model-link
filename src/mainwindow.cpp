#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <QApplication>

static QString formatSize(qint64 size)
{
    if (size < 1024) {
        return QString("%1 B").arg(size);
    } else if (size < 1024 * 1024) {
        return QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    } else if (size < 1024LL * 1024 * 1024) {
        return QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
    } else {
        return QString("%1 GB").arg(size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_fileListMode(false)
    , m_downloadSource(ModelParser::Invalid)
    , m_ggufMode(false)
    , m_maxConcurrent(3)
    , m_totalFiles(0)
    , m_completedFiles(0)
    , m_failedFiles(0)
    , m_batchTotalSize(0)
{
    setupUi();
}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    QGroupBox *urlGroup = new QGroupBox("模型标识 / URL");
    QHBoxLayout *urlLayout = new QHBoxLayout(urlGroup);

    m_urlEdit = new QLineEdit();
    m_urlEdit->setPlaceholderText(
        "输入模型标识 (如 Qwen/Qwen3.6-27B) 或模型链接，或直接文件下载链接..."
    );
    m_urlEdit->setClearButtonEnabled(true);

    m_fetchBtn = new QPushButton("获取文件");
    m_fetchBtn->setFixedWidth(100);

    urlLayout->addWidget(m_urlEdit);
    urlLayout->addWidget(m_fetchBtn);

    QGroupBox *fileGroup = new QGroupBox("下载文件列表");
    QVBoxLayout *fileLayout = new QVBoxLayout(fileGroup);

    m_fileList = new QListWidget();
    m_fileList->setAlternatingRowColors(true);
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_fileList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    fileLayout->addWidget(m_fileList);

    QHBoxLayout *btnLayout = new QHBoxLayout();

    m_downloadBtn = new QPushButton("下载全部文件");
    m_downloadBtn->setFixedWidth(140);
    m_downloadBtn->setEnabled(false);

    m_openFolderBtn = new QPushButton("打开下载文件夹");
    m_openFolderBtn->setFixedWidth(140);

    btnLayout->addWidget(m_downloadBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_openFolderBtn);

    QGroupBox *progressGroup = new QGroupBox("下载进度");
    QVBoxLayout *progressLayout = new QVBoxLayout(progressGroup);

    m_progressBar = new QProgressBar();
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat("%p%");
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
    m_progressBar->setValue(0);

    m_statusLabel = new QLabel("就绪");
    m_statusLabel->setWordWrap(true);

    progressLayout->addWidget(m_progressBar);
    progressLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(urlGroup);
    mainLayout->addWidget(fileGroup, 1);
    mainLayout->addLayout(btnLayout);
    mainLayout->addWidget(progressGroup);

    connect(m_fetchBtn, &QPushButton::clicked, this, &MainWindow::onFetchFilesClicked);
    connect(m_downloadBtn, &QPushButton::clicked, this, &MainWindow::onDownloadClicked);
    connect(m_openFolderBtn, &QPushButton::clicked, this, &MainWindow::onOpenFolderClicked);
    connect(m_fileList, &QListWidget::itemClicked, this, &MainWindow::onFileListItemClicked);
    connect(m_fileList, &QListWidget::itemDoubleClicked, this, &MainWindow::onFileDoubleClicked);
    connect(m_urlEdit, &QLineEdit::returnPressed, this, &MainWindow::onFetchFilesClicked);
}

void MainWindow::setUiEnabled(bool enabled)
{
    m_urlEdit->setEnabled(enabled);
    m_fetchBtn->setEnabled(enabled);
    m_downloadBtn->setEnabled(enabled && m_fileList->count() > 0);
    m_fileList->setEnabled(enabled);
}

void MainWindow::filterGgufFiles(QStringList &names, QList<qint64> &sizes)
{
    QStringList ggufNames;
    QList<qint64> ggufSizes;

    for (int i = 0; i < names.size(); ++i) {
        if (names[i].endsWith(".gguf", Qt::CaseInsensitive)) {
            ggufNames.append(names[i]);
            ggufSizes.append(sizes[i]);
        }
    }

    if (!ggufNames.isEmpty()) {
        names = ggufNames;
        sizes = ggufSizes;
        m_ggufMode = true;
    } else {
        m_ggufMode = false;
    }
}

void MainWindow::prepareDownloadDir()
{
    QString modelFolder = QString("model_link_%1_%2")
        .arg(m_modelInfo.namespaceName, m_modelInfo.modelName)
        .replace('/', '_').replace('\\', '_');

    QString baseDir = DownloadManager::defaultDownloadDir();
    m_downloadDir = QDir(baseDir).filePath(modelFolder);

    QDir dir(m_downloadDir);
    if (dir.exists()) {
        dir.removeRecursively();
    }
    QDir().mkpath(m_downloadDir);
}

void MainWindow::fillDownloadSlots()
{
    while (m_activeMap.size() < m_maxConcurrent && !m_pendingIndices.isEmpty()) {
        int fileIdx = m_pendingIndices.takeFirst();

        DownloadManager *mgr = new DownloadManager(this);
        m_pool.append(mgr);
        m_activeMap.insert(mgr, fileIdx);
        m_slotReceived[fileIdx] = 0;
        m_slotTotal[fileIdx] = 0;

        const QString &file = m_fileNames[fileIdx];
        QString savePath = QDir(m_downloadDir).filePath(file);

        QUrl downloadUrl;
        if (m_downloadSource == ModelParser::ModelScope) {
            downloadUrl = ModelParser::modelscopeDownloadUrl(
                m_modelInfo.namespaceName, m_modelInfo.modelName, file);
        } else if (m_downloadSource == ModelParser::HuggingFace) {
            downloadUrl = ModelParser::huggingfaceDownloadUrl(
                m_modelInfo.namespaceName, m_modelInfo.modelName, file);
        } else {
            downloadUrl = QUrl(m_urlEdit->text().trimmed());
        }

        // Connect with captured fileIdx
        connect(mgr, &DownloadManager::downloadProgress, this,
            [this, fileIdx](qint64 received, qint64 total, int speed) {
                onSlotProgress(fileIdx, received, total, speed);
            });
        connect(mgr, &DownloadManager::downloadFinished, this,
            [this, fileIdx](const QString &fp) { onSlotFinished(fileIdx, fp); });
        connect(mgr, &DownloadManager::downloadError, this,
            [this, fileIdx](const QString &err) { onSlotError(fileIdx, err); });

        mgr->startDownload(downloadUrl, savePath);

        // Auto-scroll to this item
        if (QListWidgetItem *item = m_fileList->item(fileIdx)) {
            m_fileList->scrollToItem(item, QAbstractItemView::EnsureVisible);
        }
    }
}

void MainWindow::onSlotProgress(int fileIdx, qint64 received, qint64 total, int speed)
{
    m_slotReceived[fileIdx] = received;
    m_slotTotal[fileIdx] = total;

    bool refreshDisplay = !m_displayThrottle.isValid() || m_displayThrottle.elapsed() >= 500;

    // Per-file item text update (throttled)
    if (refreshDisplay && fileIdx >= 0 && fileIdx < m_itemBaseTexts.size()) {
        int pct = (total > 0) ? static_cast<int>(received * 100 / total) : 0;

        // Fixed-width speed string (10 chars)
        QString speedStr;
        if (speed >= 1024 * 1024)
            speedStr = QString("%1 MB/s").arg(speed / (1024.0 * 1024.0), 6, 'f', 1);
        else if (speed >= 1024)
            speedStr = QString("%1 KB/s").arg(speed / 1024.0, 6, 'f', 1);
        else
            speedStr = QString("%1  B/s").arg(speed, 6);

        QListWidgetItem *item = m_fileList->item(fileIdx);
        if (item) {
            item->setText(QString("%1  —  %2% | %3")
                .arg(m_itemBaseTexts[fileIdx]).arg(pct, 3).arg(speedStr));
        }
    }

    // Overall progress bar + status label (throttled)
    if (refreshDisplay) {
        m_displayThrottle.restart();
        updateOverallProgress();
    }
}

void MainWindow::onSlotFinished(int fileIdx, const QString &filePath)
{
    Q_UNUSED(filePath)

    // Find and remove the manager from active map
    DownloadManager *done = nullptr;
    for (auto it = m_activeMap.begin(); it != m_activeMap.end(); ++it) {
        if (it.value() == fileIdx) {
            done = it.key();
            break;
        }
    }
    if (done) {
        m_activeMap.remove(done);
        done->deleteLater();
        m_pool.removeOne(done);
    }

    m_slotReceived.remove(fileIdx);
    m_slotTotal.remove(fileIdx);
    m_completedFiles++;

    // Mark item as complete
    if (fileIdx >= 0 && fileIdx < m_itemBaseTexts.size()) {
        QListWidgetItem *item = m_fileList->item(fileIdx);
        if (item) {
            item->setText(QString::fromUtf8("✓  ") + m_itemBaseTexts[fileIdx]);
            item->setForeground(QColor(0, 140, 0));
        }
    }

    // Check if batch is done
    if (m_activeMap.isEmpty() && m_pendingIndices.isEmpty()) {
        onBatchComplete();
        return;
    }

    updateOverallProgress();
    fillDownloadSlots();
}

void MainWindow::onSlotError(int fileIdx, const QString &error)
{
    // Find and remove the manager
    DownloadManager *failed = nullptr;
    for (auto it = m_activeMap.begin(); it != m_activeMap.end(); ++it) {
        if (it.value() == fileIdx) {
            failed = it.key();
            break;
        }
    }
    if (failed) {
        m_activeMap.remove(failed);
        failed->deleteLater();
        m_pool.removeOne(failed);
    }

    m_slotReceived.remove(fileIdx);
    m_slotTotal.remove(fileIdx);
    m_failedFiles++;

    // Mark item as failed
    if (fileIdx >= 0 && fileIdx < m_itemBaseTexts.size()) {
        QListWidgetItem *item = m_fileList->item(fileIdx);
        if (item) {
            item->setText(QString::fromUtf8("✗  ") + m_itemBaseTexts[fileIdx]);
            item->setForeground(QColor(200, 0, 0));
        }
    }

    if (m_activeMap.isEmpty() && m_pendingIndices.isEmpty()) {
        onBatchComplete();
        return;
    }

    updateOverallProgress();
    fillDownloadSlots();
}

void MainWindow::updateOverallProgress()
{
    qint64 totalDone = 0;
    qint64 totalSize = m_batchTotalSize;

    for (auto it = m_activeMap.begin(); it != m_activeMap.end(); ++it) {
        int idx = it.value();
        totalDone += m_slotReceived.value(idx, 0);
    }
    for (int i = 0; i < m_totalFiles; ++i) {
        if (!m_slotReceived.contains(i) && !m_pendingIndices.contains(i)) {
            if (m_fileSizes[i] > 0) {
                totalDone += m_fileSizes[i];
            }
        }
    }

    int overallPercent = (totalSize > 0) ? static_cast<int>(totalDone * 100 / totalSize) : 0;
    m_progressBar->setValue(overallPercent);

    int done = m_completedFiles + m_failedFiles;
    m_progressBar->setFormat(
        QString("%1/%2 个文件 | %3 / %4 | %5%")
            .arg(done).arg(m_totalFiles)
            .arg(formatSize(totalDone)).arg(formatSize(totalSize))
            .arg(overallPercent));

    m_statusLabel->setText(
        QString("并行下载中 (%1 线程) | 已完成 %2/%3 个文件")
            .arg(m_activeMap.size())
            .arg(done)
            .arg(m_totalFiles));
}

void MainWindow::onBatchComplete()
{
    // Clean up any remaining managers
    for (auto *mgr : m_pool) {
        mgr->cancelDownload();
        mgr->deleteLater();
    }
    m_pool.clear();
    m_activeMap.clear();
    m_slotReceived.clear();
    m_slotTotal.clear();

    setUiEnabled(true);
    m_progressBar->setValue(100);
    m_progressBar->setFormat("完成");

    QString summary;
    if (m_failedFiles > 0) {
        summary = QString("下载完成: %1 成功, %2 失败 → %3")
            .arg(m_completedFiles).arg(m_failedFiles).arg(m_downloadDir);
    } else {
        summary = QString("全部下载完成! 共 %1 个文件 → %2")
            .arg(m_totalFiles).arg(m_downloadDir);
    }
    m_statusLabel->setText(summary);

    m_downloadBtn->setText("下载全部文件");
    m_downloadBtn->setEnabled(true);

    onOpenFolderClicked();

    QMessageBox::information(this, "完成",
        QString("成功 %1 个, 失败 %2 个\n保存位置:\n%3")
            .arg(m_completedFiles).arg(m_failedFiles).arg(m_downloadDir));
}

void MainWindow::tryFetchFromModelscope()
{
    m_downloadSource = ModelParser::ModelScope;
    m_statusLabel->setText("正在从魔塔社区查询模型...");

    auto *mgr = new DownloadManager(this);
    m_pool.append(mgr);

    connect(mgr, &DownloadManager::jsonFetched, this, &MainWindow::onJsonFetched);
    connect(mgr, &DownloadManager::jsonFetchError, this, &MainWindow::onJsonFetchError);

    QUrl apiUrl = ModelParser::modelscopeApiUrl(
        m_modelInfo.namespaceName, m_modelInfo.modelName);
    mgr->fetchJson(apiUrl);
}

void MainWindow::tryFetchFromHuggingface()
{
    m_downloadSource = ModelParser::HuggingFace;
    m_statusLabel->setText("魔塔社区未找到，正在从 HuggingFace 查询...");

    auto *mgr = new DownloadManager(this);
    m_pool.append(mgr);

    connect(mgr, &DownloadManager::jsonFetched, this, &MainWindow::onJsonFetched);
    connect(mgr, &DownloadManager::jsonFetchError, this, &MainWindow::onJsonFetchError);

    QUrl apiUrl = ModelParser::huggingfaceApiUrl(
        m_modelInfo.namespaceName, m_modelInfo.modelName);
    mgr->fetchJson(apiUrl);
}

void MainWindow::onFetchFilesClicked()
{
    QString input = m_urlEdit->text().trimmed();
    if (input.isEmpty()) return;

    m_modelInfo = ModelParser::parseInput(input);

    if (!m_modelInfo.isValid()) {
        QMessageBox::warning(this, "错误",
            "无法识别的输入格式。\n"
            "支持的格式:\n"
            "  · 模型标识: Qwen/Qwen3.6-27B\n"
            "  · 魔塔链接: modelscope.cn/models/{组织}/{模型}\n"
            "  · HuggingFace链接: huggingface.co/{组织}/{模型}\n"
            "  · 直接文件下载链接");
        return;
    }

    m_fileList->clear();
    m_fileNames.clear();
    m_fileSizes.clear();
    m_fileListMode = true;
    m_downloadSource = ModelParser::Invalid;
    m_ggufMode = false;

    // Clean up any leftover managers from previous operations
    for (auto *mgr : m_pool) {
        mgr->cancelDownload();
        mgr->deleteLater();
    }
    m_pool.clear();

    if (m_modelInfo.isModelId() || m_modelInfo.source == ModelParser::ModelScope) {
        setUiEnabled(false);
        tryFetchFromModelscope();
        return;
    }

    if (m_modelInfo.source == ModelParser::HuggingFace) {
        setUiEnabled(false);
        tryFetchFromHuggingface();
        return;
    }

    if (m_modelInfo.source == ModelParser::DirectUrl) {
        QString fileName;
        if (!m_modelInfo.filePath.isEmpty()) {
            fileName = QFileInfo(m_modelInfo.filePath).fileName();
        }
        if (fileName.isEmpty()) {
            fileName = QUrl(input).fileName();
        }
        if (fileName.isEmpty()) {
            fileName = "download";
        }

        m_fileNames.append(fileName);
        m_fileSizes.append(-1);
        m_fileList->addItem(fileName);
        m_downloadBtn->setEnabled(true);
        m_fileListMode = false;
    }
}

void MainWindow::onJsonFetched(const QByteArray &data, const QString &context)
{
    // Clean up fetch managers
    for (auto *mgr : m_pool) {
        mgr->deleteLater();
    }
    m_pool.clear();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        QMessageBox::warning(this, "错误", "无法解析服务器响应");
        setUiEnabled(true);
        return;
    }

    QJsonObject root = doc.object();
    bool isModelscopeResponse = context.contains("modelscope.cn");

    if (isModelscopeResponse) {
        int code = root["Code"].toInt(-1);
        if (code != 200) {
            QString msg = root["Message"].toString("未知错误");
            if (m_modelInfo.isModelId()) {
                m_statusLabel->setText(
                    QString("魔塔未找到 (%1)，尝试 HuggingFace...").arg(msg));
                tryFetchFromHuggingface();
                return;
            }
            QMessageBox::warning(this, "ModelScope API 错误",
                QString("Code: %1\nMessage: %2").arg(code).arg(msg));
            setUiEnabled(true);
            return;
        }
    } else {
        if (root.contains("error")) {
            QString msg = root["error"].toString();
            if (m_modelInfo.isModelId()) {
                QMessageBox::warning(this, "查询失败",
                    QString("魔塔社区和 HuggingFace 均未找到该模型:\n%1\n\n"
                            "HuggingFace 错误: %2")
                        .arg(m_modelInfo.namespaceName + "/" + m_modelInfo.modelName)
                        .arg(msg));
                setUiEnabled(true);
                return;
            }
            QMessageBox::warning(this, "HuggingFace API 错误", msg);
            setUiEnabled(true);
            return;
        }
    }

    QStringList rawNames;
    QList<qint64> rawSizes;

    if (isModelscopeResponse) {
        m_downloadSource = ModelParser::ModelScope;
        QJsonObject dataObj = root["Data"].toObject();
        QJsonArray files = dataObj["Files"].toArray();
        for (const auto &fileVal : files) {
            QJsonObject fileObj = fileVal.toObject();
            QString name = fileObj["Name"].toString();
            qint64 size = static_cast<qint64>(fileObj["Size"].toDouble());
            if (!name.isEmpty()) {
                rawNames.append(name);
                rawSizes.append(size);
            }
        }
    } else {
        m_downloadSource = ModelParser::HuggingFace;
        QJsonArray siblings = root["siblings"].toArray();
        for (const auto &sibVal : siblings) {
            QJsonObject sibObj = sibVal.toObject();
            QString name = sibObj["rfilename"].toString();
            qint64 size = static_cast<qint64>(sibObj["size"].toDouble());
            if (!name.isEmpty()) {
                rawNames.append(name);
                rawSizes.append(size);
            }
        }
    }

    if (rawNames.isEmpty()) {
        m_statusLabel->setText("该模型没有找到文件");
        setUiEnabled(true);
        return;
    }

    m_fileNames = rawNames;
    m_fileSizes = rawSizes;
    filterGgufFiles(m_fileNames, m_fileSizes);

    m_fileList->clear();
    m_itemBaseTexts.clear();
    m_batchTotalSize = 0;

    for (int i = 0; i < m_fileNames.size(); ++i) {
        QString text = QString("[%1] %2").arg(i + 1).arg(m_fileNames[i]);
        if (m_fileSizes[i] > 0) {
            m_batchTotalSize += m_fileSizes[i];
            double sizeMB = m_fileSizes[i] / (1024.0 * 1024.0);
            if (sizeMB >= 1000.0) {
                text += QString("  [%1 GB]").arg(sizeMB / 1024.0, 0, 'f', 2);
            } else if (sizeMB >= 1.0) {
                text += QString("  [%1 MB]").arg(sizeMB, 0, 'f', 1);
            } else {
                text += QString("  [%1 KB]").arg(m_fileSizes[i] / 1024.0, 0, 'f', 1);
            }
        }
        m_itemBaseTexts.append(text);
        m_fileList->addItem(text);
    }

    QString sourceLabel = isModelscopeResponse
        ? (m_ggufMode ? "魔塔社区 (GGUF)" : "魔塔社区")
        : (m_ggufMode ? "HuggingFace (GGUF)" : "HuggingFace");

    m_statusLabel->setText(
        QString("✓ %1: %2 个文件，合计 %3")
            .arg(sourceLabel)
            .arg(m_fileNames.size())
            .arg(formatSize(m_batchTotalSize)));

    m_downloadBtn->setEnabled(m_fileNames.size() > 0);
    setUiEnabled(true);
}

void MainWindow::onJsonFetchError(const QString &error)
{
    for (auto *mgr : m_pool) {
        mgr->deleteLater();
    }
    m_pool.clear();

    if (m_modelInfo.isModelId() && m_downloadSource == ModelParser::ModelScope) {
        m_statusLabel->setText(
            QString("魔塔社区查询失败 (%1)，尝试 HuggingFace...").arg(error));
        tryFetchFromHuggingface();
        return;
    }

    setUiEnabled(true);
    QMessageBox::warning(this, "获取文件列表失败", error);
}

void MainWindow::onFileListItemClicked(QListWidgetItem *item)
{
    Q_UNUSED(item)
    m_downloadBtn->setEnabled(m_activeMap.isEmpty() && m_fileList->count() > 0);
}

void MainWindow::onDownloadClicked()
{
    if (!m_activeMap.isEmpty()) {
        // Cancel all active downloads
        m_pendingIndices.clear();
        for (auto it = m_activeMap.begin(); it != m_activeMap.end(); ++it) {
            it.key()->cancelDownload();
        }
        for (auto *mgr : m_pool) {
            mgr->deleteLater();
        }
        m_pool.clear();
        m_activeMap.clear();
        m_slotReceived.clear();
        m_slotTotal.clear();

        // Reset file list item texts
        for (int i = 0; i < m_itemBaseTexts.size() && i < m_fileList->count(); ++i) {
            QListWidgetItem *item = m_fileList->item(i);
            if (item) {
                item->setText(m_itemBaseTexts[i]);
                item->setForeground(QColor());
            }
        }

        setUiEnabled(true);
        m_progressBar->setValue(0);
        m_progressBar->setFormat("0%");
        m_statusLabel->setText("已取消下载");
        m_downloadBtn->setText("下载全部文件");
        m_downloadBtn->setEnabled(m_fileNames.size() > 0);
        return;
    }

    if (m_fileNames.isEmpty()) {
        QMessageBox::warning(this, "错误", "文件列表为空");
        return;
    }

    QList<int> all;
    for (int i = 0; i < m_fileNames.size(); ++i) all.append(i);
    startDownloads(all);
}

void MainWindow::onFileDoubleClicked(QListWidgetItem *item)
{
    if (!m_activeMap.isEmpty()) return;  // already downloading

    int row = m_fileList->row(item);
    if (row < 0 || row >= m_fileNames.size()) return;

    startDownloads({row});
}

void MainWindow::startDownloads(const QList<int> &indices)
{
    prepareDownloadDir();

    m_totalFiles = indices.size();
    m_completedFiles = 0;
    m_failedFiles = 0;

    m_pendingIndices = indices;

    m_slotReceived.clear();
    m_slotTotal.clear();
    m_displayThrottle.invalidate();

    // Reset item texts for these indices
    for (int idx : indices) {
        if (idx < m_itemBaseTexts.size()) {
            QListWidgetItem *item = m_fileList->item(idx);
            if (item) {
                item->setText(m_itemBaseTexts[idx]);
                item->setForeground(QColor());
            }
        }
    }

    m_progressBar->setValue(0);
    m_progressBar->setFormat("0%");

    QString msg = (m_totalFiles == 1)
        ? QString("开始下载: %1").arg(m_fileNames[indices[0]])
        : QString("开始并行下载 %1 个文件 (%2 线程)...")
              .arg(m_totalFiles).arg(m_maxConcurrent);
    m_statusLabel->setText(msg);

    setUiEnabled(false);
    m_downloadBtn->setEnabled(true);
    m_downloadBtn->setText("取消下载");

    fillDownloadSlots();
}

bool MainWindow::checkFileOverwrite(const QString &filePath)
{
    if (!QFile::exists(filePath)) {
        return true;
    }

    QFileInfo fi(filePath);
    QString msg = QString("文件已存在:\n%1\n大小: %2\n\n是否覆盖？\n(将影响批量下载中的所有同名文件)")
        .arg(filePath)
        .arg(formatSize(fi.size()));

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "文件已存在", msg,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        QFile::remove(filePath);
        QFile::remove(filePath + ".part");
        return true;
    }
    return false;
}

void MainWindow::onOpenFolderClicked()
{
    QString dir = m_downloadDir.isEmpty()
        ? DownloadManager::defaultDownloadDir()
        : m_downloadDir;

    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
