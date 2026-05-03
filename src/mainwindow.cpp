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
#include <QStyleFactory>
#include <QWidgetAction>
#include <QFrame>
#include <algorithm>

static QString formatSize(qint64 size)
{
    if (size < 1024)
        return QString("%1 B").arg(size);
    else if (size < 1024 * 1024)
        return QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    else if (size < 1024LL * 1024 * 1024)
        return QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
    else
        return QString("%1 GB").arg(size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_downloadSource(ModelParser::Invalid)
    , m_ggufMode(false)
    , m_discoveringModelId(false)
    , m_maxConcurrent(3)
    , m_totalFiles(0)
    , m_completedFiles(0)
    , m_failedFiles(0)
    , m_batchTotalSize(0)
{
    setupUi();
}

void MainWindow::applyStyleSheet()
{
    QString base;
    bool isDark = palette().color(QPalette::Window).lightness() < 128;

    if (isDark) {
        base = R"(
            QMainWindow { background: #1e1e2e; }
            QGroupBox {
                font-weight: bold; border: 1px solid #3a3a5c; border-radius: 6px;
                margin-top: 10px; padding: 12px 8px 8px 8px;
                background: #252540;
            }
            QGroupBox::title {
                subcontrol-origin: margin; left: 12px; padding: 0 6px;
                color: #cdd6f4;
            }
            QLineEdit {
                padding: 6px 10px; border: 1px solid #45475a; border-radius: 5px;
                background: #313244; color: #cdd6f4; selection-background-color: #89b4fa;
            }
            QLineEdit:focus { border: 1px solid #89b4fa; }
            QPushButton {
                padding: 6px 16px; border: 1px solid #45475a; border-radius: 5px;
                background: #45475a; color: #cdd6f4; font-weight: bold;
            }
            QPushButton:hover { background: #585b70; }
            QPushButton:pressed { background: #313244; }
            QPushButton:disabled { color: #6c7086; background: #313244; }
            QSpinBox {
                padding: 4px 6px 4px 8px; border: 1px solid #45475a; border-radius: 5px;
                background: #313244; color: #cdd6f4; min-width: 56px;
            }
            QSpinBox::up-button {
                subcontrol-origin: border; subcontrol-position: top right;
                width: 16px; border: none; border-left: 1px solid #45475a;
                background: #3a3a55;
            }
            QSpinBox::down-button {
                subcontrol-origin: border; subcontrol-position: bottom right;
                width: 16px; border: none; border-left: 1px solid #45475a;
                background: #3a3a55;
            }
            QSpinBox::up-arrow, QSpinBox::down-arrow { width: 6px; height: 6px; }
            QSpinBox::up-arrow { image: none; border-left: 3px solid transparent; border-right: 3px solid transparent; border-bottom: 4px solid #a6adc8; }
            QSpinBox::down-arrow { image: none; border-left: 3px solid transparent; border-right: 3px solid transparent; border-top: 4px solid #a6adc8; }
            QProgressBar {
                border: 1px solid #45475a; border-radius: 5px; text-align: center;
                background: #313244; color: #cdd6f4; height: 20px;
            }
            QProgressBar::chunk { background: #89b4fa; border-radius: 4px; }
            QListWidget {
                border: 1px solid #45475a; border-radius: 5px;
                background: #1e1e2e; color: #cdd6f4; outline: none;
            }
            QListWidget::item { padding: 5px 8px; border-radius: 3px; }
            QListWidget::item:selected { background: #45475a; color: #cdd6f4; }
            QListWidget::item:hover:!selected { background: #313244; }
            QLabel { color: #a6adc8; }
        )";
    } else {
        base = R"(
            QGroupBox {
                font-weight: bold; border: 1px solid #d0d0d0; border-radius: 6px;
                margin-top: 10px; padding: 12px 8px 8px 8px;
            }
            QGroupBox::title {
                subcontrol-origin: margin; left: 12px; padding: 0 6px;
            }
            QLineEdit {
                padding: 6px 10px; border: 1px solid #c0c0c0; border-radius: 5px;
                background: palette(base); selection-background-color: #0078d4;
            }
            QLineEdit:focus { border: 1px solid #0078d4; }
            QPushButton {
                padding: 6px 16px; border: 1px solid #c0c0c0; border-radius: 5px;
                background: #f0f0f0; font-weight: bold;
            }
            QPushButton:hover { background: #e0e0e0; }
            QPushButton:pressed { background: #d0d0d0; }
            QPushButton:disabled { color: #a0a0a0; }
            QSpinBox {
                padding: 4px 6px 4px 8px; border: 1px solid #c0c0c0; border-radius: 5px;
                background: palette(base); min-width: 56px;
            }
            QSpinBox::up-button {
                subcontrol-origin: border; subcontrol-position: top right;
                width: 16px; border: none; border-left: 1px solid #d0d0d0;
                background: #f4f4f4;
            }
            QSpinBox::down-button {
                subcontrol-origin: border; subcontrol-position: bottom right;
                width: 16px; border: none; border-left: 1px solid #d0d0d0;
                background: #f4f4f4;
            }
            QSpinBox::up-arrow, QSpinBox::down-arrow { width: 6px; height: 6px; }
            QSpinBox::up-arrow { image: none; border-left: 3px solid transparent; border-right: 3px solid transparent; border-bottom: 4px solid #606060; }
            QSpinBox::down-arrow { image: none; border-left: 3px solid transparent; border-right: 3px solid transparent; border-top: 4px solid #606060; }
            QProgressBar {
                border: 1px solid #c0c0c0; border-radius: 5px; text-align: center;
                background: palette(base); height: 20px;
            }
            QProgressBar::chunk { background: #0078d4; border-radius: 4px; }
            QListWidget {
                border: 1px solid #c0c0c0; border-radius: 5px;
                background: palette(base); outline: none;
            }
            QListWidget::item { padding: 5px 8px; border-radius: 3px; }
            QListWidget::item:selected { background: #0078d4; color: white; }
            QListWidget::item:hover:!selected { background: #e8f0fe; }
        )";
    }

    setStyleSheet(base);
}

void MainWindow::setupUi()
{
    applyStyleSheet();

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(16, 12, 16, 12);

    // ---- URL row ----
    QHBoxLayout *urlLayout = new QHBoxLayout();
    urlLayout->setSpacing(8);

    m_urlEdit = new QLineEdit();
    m_urlEdit->setPlaceholderText("输入模型标识 (如 Qwen/Qwen3.6-27B) 或模型链接...");
    m_urlEdit->setClearButtonEnabled(true);

    m_fetchBtn = new QPushButton("获取文件");
    m_fetchBtn->setFixedWidth(96);

    urlLayout->addWidget(m_urlEdit, 1);
    urlLayout->addWidget(m_fetchBtn);

    // ---- File list ----
    m_fileList = new QListWidget();
    m_fileList->setAlternatingRowColors(false);
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_fileList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // ---- Button row ----
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    m_downloadSelBtn = new QPushButton("下载选中");
    m_downloadSelBtn->setFixedWidth(100);
    m_downloadSelBtn->setEnabled(false);

    m_downloadAllBtn = new QPushButton("下载全部");
    m_downloadAllBtn->setFixedWidth(100);
    m_downloadAllBtn->setEnabled(false);

    m_openFolderBtn = new QPushButton("打开文件夹");
    m_openFolderBtn->setFixedWidth(110);

    QLabel *threadLabel = new QLabel("线程");
    m_threadSpin = new QSpinBox();
    m_threadSpin->setRange(1, 8);
    m_threadSpin->setValue(m_maxConcurrent);
    m_threadSpin->setFixedWidth(58);

    btnLayout->addWidget(m_downloadSelBtn);
    btnLayout->addWidget(m_downloadAllBtn);
    btnLayout->addWidget(threadLabel);
    btnLayout->addWidget(m_threadSpin);
    btnLayout->addStretch();
    btnLayout->addWidget(m_openFolderBtn);

    // ---- Progress ----
    QGroupBox *progressGroup = new QGroupBox("下载进度");
    QVBoxLayout *progressLayout = new QVBoxLayout(progressGroup);
    progressLayout->setSpacing(6);

    m_progressBar = new QProgressBar();
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat("就绪");
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
    m_progressBar->setValue(0);

    m_statusLabel = new QLabel("就绪");
    m_statusLabel->setWordWrap(true);

    progressLayout->addWidget(m_progressBar);
    progressLayout->addWidget(m_statusLabel);

    // ---- Assemble ----
    mainLayout->addLayout(urlLayout);
    mainLayout->addWidget(m_fileList, 1);
    mainLayout->addLayout(btnLayout);
    mainLayout->addWidget(progressGroup);

    // ---- Connections ----
    connect(m_fetchBtn, &QPushButton::clicked, this, &MainWindow::onFetchFilesClicked);
    connect(m_downloadAllBtn, &QPushButton::clicked, this, &MainWindow::onDownloadAllClicked);
    connect(m_downloadSelBtn, &QPushButton::clicked, this, &MainWindow::onDownloadSelectedClicked);
    connect(m_openFolderBtn, &QPushButton::clicked, this, &MainWindow::onOpenFolderClicked);
    connect(m_threadSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        m_maxConcurrent = value;
    });
    connect(m_urlEdit, &QLineEdit::returnPressed, this, &MainWindow::onFetchFilesClicked);
    connect(m_fileList, &QListWidget::itemSelectionChanged, this, [this]() {
        m_downloadSelBtn->setEnabled(
            m_activeMap.isEmpty() && !m_fileList->selectedItems().isEmpty());
    });
    connect(m_fileList, &QListWidget::itemDoubleClicked, this, &MainWindow::onFileDoubleClicked);
}

void MainWindow::updateItemWidgets()
{
    for (int i = 0; i < m_fileList->count() && i < m_fileNames.size(); ++i) {
        QListWidgetItem *item = m_fileList->item(i);
        if (!item) continue;

        QWidget *w = new QWidget();
        QHBoxLayout *lay = new QHBoxLayout(w);
        lay->setContentsMargins(4, 2, 4, 2);
        lay->setSpacing(6);

        QLabel *label = new QLabel(m_itemBaseTexts[i]);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        lay->addWidget(label, 1);

        QPushButton *dlBtn = new QPushButton("↓");
        dlBtn->setFixedSize(28, 28);
        dlBtn->setToolTip(QString("下载 %1").arg(m_fileNames[i]));
        bool isDark = palette().color(QPalette::Window).lightness() < 128;
        dlBtn->setStyleSheet(isDark
            ? "QPushButton { border: 1px solid #585b70; border-radius: 14px; "
              "background: #313244; color: #cdd6f4; font-weight: bold; font-size: 14px; padding: 0; }"
              "QPushButton:hover { background: #89b4fa; color: #1e1e2e; border-color: #89b4fa; }"
            : "QPushButton { border: 1px solid #aaa; border-radius: 14px; "
              "background: #f0f0f0; font-weight: bold; font-size: 14px; padding: 0; }"
              "QPushButton:hover { background: #0078d4; color: white; border-color: #0078d4; }");

        int idx = i;
        connect(dlBtn, &QPushButton::clicked, this, [this, idx]() {
            onDownloadItemClicked(idx);
        });

        lay->addWidget(dlBtn);
        item->setSizeHint(w->sizeHint());
        m_fileList->setItemWidget(item, w);
    }
}

void MainWindow::setUiEnabled(bool enabled)
{
    m_urlEdit->setEnabled(enabled);
    m_fetchBtn->setEnabled(enabled);
    m_downloadAllBtn->setEnabled(enabled && m_fileList->count() > 0);
    m_downloadSelBtn->setEnabled(
        enabled && !m_fileList->selectedItems().isEmpty() && m_activeMap.isEmpty());
    if (enabled) {
        m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    } else {
        m_fileList->setSelectionMode(QAbstractItemView::NoSelection);
    }
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

void MainWindow::populateFileList(ModelParser::Source source, const QStringList &names, const QList<qint64> &sizes, bool allowGgufFilter)
{
    m_downloadSource = source;
    m_fileNames = names;
    m_fileSizes = sizes;
    if (allowGgufFilter) {
        filterGgufFiles(m_fileNames, m_fileSizes);
    } else {
        m_ggufMode = !m_fileNames.isEmpty() && std::all_of(m_fileNames.begin(), m_fileNames.end(), [](const QString &n) {
            return n.endsWith(".gguf", Qt::CaseInsensitive);
        });
    }

    m_fileList->clear();
    m_itemBaseTexts.clear();
    m_batchTotalSize = 0;

    for (int i = 0; i < m_fileNames.size(); ++i) {
        QString text = QString("[%1] %2").arg(i + 1).arg(m_fileNames[i]);
        if (m_fileSizes[i] > 0) {
            m_batchTotalSize += m_fileSizes[i];
            double mb = m_fileSizes[i] / (1024.0 * 1024.0);
            if (mb >= 1000.0) text += QString("  [%1 GB]").arg(mb / 1024.0, 0, 'f', 2);
            else if (mb >= 1.0) text += QString("  [%1 MB]").arg(mb, 0, 'f', 1);
            else text += QString("  [%1 KB]").arg(m_fileSizes[i] / 1024.0, 0, 'f', 1);
        }
        m_itemBaseTexts.append(text);
        m_fileList->addItem("");
    }

    updateItemWidgets();

    QString src = source == ModelParser::ModelScope
        ? (m_ggufMode ? "魔塔社区 (GGUF)" : "魔塔社区")
        : (m_ggufMode ? "HuggingFace (GGUF)" : "HuggingFace");
    m_statusLabel->setText(QString("✓ %1: %2 个文件，合计 %3")
        .arg(src).arg(m_fileNames.size()).arg(formatSize(m_batchTotalSize)));

    m_downloadAllBtn->setEnabled(!m_fileNames.isEmpty());
    setUiEnabled(true);
}

void MainWindow::chooseModelIdSource()
{
    m_discoveringModelId = false;
    ModelService::Selection selection = ModelService::choosePreferred(m_modelscopeFiles, m_huggingfaceFiles);
    if (selection.ok) {
        populateFileList(selection.source, selection.names, selection.sizes, false);
        m_ggufMode = selection.gguf;
        return;
    }

    setUiEnabled(true);
    QMessageBox::warning(this, "查询失败", "魔塔社区和 HuggingFace 均未找到可下载文件。");
}

void MainWindow::prepareDownloadDir()
{
    QString modelFolder = QString("model_link_%1_%2")
        .arg(m_modelInfo.namespaceName, m_modelInfo.modelName)
        .replace('/', '_').replace('\\', '_');
    if (modelFolder == "model_link__") {
        modelFolder = "model_link_direct";
    }

    m_downloadDir = QDir(DownloadManager::defaultDownloadDir()).filePath(modelFolder);
    QDir().mkpath(m_downloadDir);
}

void MainWindow::startDownloads(const QList<int> &indices)
{
    prepareDownloadDir();

    m_totalFiles = indices.size();
    m_completedFiles = 0;
    m_failedFiles = 0;
    m_pendingIndices = indices;
    m_currentTaskIndices = indices;

    // Recalculate total size for the selected subset
    m_batchTotalSize = 0;
    for (int idx : indices) {
        if (idx < m_fileSizes.size() && m_fileSizes[idx] > 0)
            m_batchTotalSize += m_fileSizes[idx];
    }

    m_slotReceived.clear();
    m_slotTotal.clear();
    m_successIndices.clear();
    m_failedIndices.clear();
    m_displayThrottle.invalidate();

    for (int idx = 0; idx < m_fileList->count(); ++idx) {
        QListWidgetItem *item = m_fileList->item(idx);
        if (!item) continue;
        QWidget *w = m_fileList->itemWidget(item);
        if (w) {
            QLabel *lbl = w->findChild<QLabel*>();
            if (lbl) {
                lbl->setText(m_itemBaseTexts[idx]);
                lbl->setStyleSheet("");
            }
        }
    }

    m_progressBar->setValue(0);
    m_progressBar->setFormat("0%");

    QString msg = (m_totalFiles == 1)
        ? QString("开始下载: %1").arg(m_fileNames[indices[0]])
        : QString("并行下载 %1 个文件 (%2 线程)...").arg(m_totalFiles).arg(m_maxConcurrent);
    m_statusLabel->setText(msg);

    setUiEnabled(false);
    m_downloadAllBtn->setEnabled(true);
    m_downloadAllBtn->setText("取消下载");
    m_downloadSelBtn->setEnabled(false);

    m_fileList->clearSelection();

    fillDownloadSlots();
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
        if (m_downloadSource == ModelParser::ModelScope)
            downloadUrl = ModelParser::modelscopeDownloadUrl(
                m_modelInfo.namespaceName, m_modelInfo.modelName, file);
        else if (m_downloadSource == ModelParser::HuggingFace)
            downloadUrl = ModelParser::huggingfaceDownloadUrl(
                m_modelInfo.namespaceName, m_modelInfo.modelName, file);
        else
            downloadUrl = QUrl(m_urlEdit->text().trimmed());

        connect(mgr, &DownloadManager::downloadProgress, this,
            [this, fileIdx](qint64 r, qint64 t, int s) { onSlotProgress(fileIdx, r, t, s); });
        connect(mgr, &DownloadManager::downloadFinished, this,
            [this, fileIdx](const QString &fp) { onSlotFinished(fileIdx, fp); });
        connect(mgr, &DownloadManager::downloadError, this,
            [this, fileIdx](const QString &err) { onSlotError(fileIdx, err); });

        mgr->startDownload(downloadUrl, savePath);
        if (QListWidgetItem *item = m_fileList->item(fileIdx))
            m_fileList->scrollToItem(item, QAbstractItemView::EnsureVisible);
    }
}

void MainWindow::onSlotProgress(int fileIdx, qint64 received, qint64 total, int speed)
{
    m_slotReceived[fileIdx] = received;
    m_slotTotal[fileIdx] = total;

    bool refresh = !m_displayThrottle.isValid() || m_displayThrottle.elapsed() >= 500;

    if (refresh && fileIdx >= 0 && fileIdx < m_itemBaseTexts.size()) {
        int pct = (total > 0) ? static_cast<int>(received * 100 / total) : 0;
        QString speedStr;
        if (speed >= 1024 * 1024)
            speedStr = QString("%1 MB/s").arg(speed / (1024.0 * 1024.0), 6, 'f', 1);
        else if (speed >= 1024)
            speedStr = QString("%1 KB/s").arg(speed / 1024.0, 6, 'f', 1);
        else
            speedStr = QString("%1  B/s").arg(speed, 6);

        QListWidgetItem *item = m_fileList->item(fileIdx);
        if (item) {
            QWidget *w = m_fileList->itemWidget(item);
            if (w) {
                QLabel *lbl = w->findChild<QLabel*>();
                if (lbl) {
                    lbl->setText(QString("%1  —  %2% | %3")
                        .arg(m_itemBaseTexts[fileIdx]).arg(pct, 3).arg(speedStr));
                    lbl->setStyleSheet(
                        "QLabel { color: #0078d4; font-weight: bold; }");
                }
            }
        }
    }

    if (refresh) {
        m_displayThrottle.restart();
        updateOverallProgress();
    }
}

void MainWindow::onSlotFinished(int fileIdx, const QString &filePath)
{
    Q_UNUSED(filePath)

    DownloadManager *done = nullptr;
    for (auto it = m_activeMap.begin(); it != m_activeMap.end(); ++it) {
        if (it.value() == fileIdx) { done = it.key(); break; }
    }
    if (done) {
        m_activeMap.remove(done);
        done->deleteLater();
        m_pool.removeOne(done);
    }

    m_slotReceived.remove(fileIdx);
    m_slotTotal.remove(fileIdx);
    m_successIndices.insert(fileIdx);
    m_completedFiles++;

    if (fileIdx >= 0 && fileIdx < m_itemBaseTexts.size()) {
        QListWidgetItem *item = m_fileList->item(fileIdx);
        if (item) {
            QWidget *w = m_fileList->itemWidget(item);
            if (w) {
                QLabel *lbl = w->findChild<QLabel*>();
                if (lbl) {
                    lbl->setText(QString::fromUtf8("✓  ") + m_itemBaseTexts[fileIdx]);
                    lbl->setStyleSheet(
                        "QLabel { color: #107c10; font-weight: bold; }");
                }
            }
        }
    }

    if (m_activeMap.isEmpty() && m_pendingIndices.isEmpty()) {
        onBatchComplete();
        return;
    }
    updateOverallProgress();
    fillDownloadSlots();
}

void MainWindow::onSlotError(int fileIdx, const QString &error)
{
    Q_UNUSED(error)

    DownloadManager *failed = nullptr;
    for (auto it = m_activeMap.begin(); it != m_activeMap.end(); ++it) {
        if (it.value() == fileIdx) { failed = it.key(); break; }
    }
    if (failed) {
        m_activeMap.remove(failed);
        failed->deleteLater();
        m_pool.removeOne(failed);
    }

    m_slotReceived.remove(fileIdx);
    m_slotTotal.remove(fileIdx);
    m_failedIndices.insert(fileIdx);
    m_failedFiles++;

    if (fileIdx >= 0 && fileIdx < m_itemBaseTexts.size()) {
        QListWidgetItem *item = m_fileList->item(fileIdx);
        if (item) {
            QWidget *w = m_fileList->itemWidget(item);
            if (w) {
                QLabel *lbl = w->findChild<QLabel*>();
                if (lbl) {
                    lbl->setText(QString::fromUtf8("✗  ") + m_itemBaseTexts[fileIdx]);
                    lbl->setStyleSheet(
                        "QLabel { color: #c50f1f; font-weight: bold; }");
                }
            }
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
    qint64 downloadedDone = 0;
    qint64 processedForPercent = 0;
    qint64 totalSize = m_batchTotalSize;

    for (auto it = m_activeMap.begin(); it != m_activeMap.end(); ++it) {
        qint64 received = m_slotReceived.value(it.value(), 0);
        downloadedDone += received;
        processedForPercent += received;
    }

    for (int i : m_currentTaskIndices) {
        if (i >= 0 && i < m_fileSizes.size() && m_fileSizes[i] > 0) {
            if (m_successIndices.contains(i)) {
                downloadedDone += m_fileSizes[i];
                processedForPercent += m_fileSizes[i];
            } else if (m_failedIndices.contains(i)) {
                processedForPercent += m_fileSizes[i];
            }
        }
    }

    int overallPercent = (totalSize > 0) ? static_cast<int>(processedForPercent * 100 / totalSize) : 0;
    m_progressBar->setValue(overallPercent);

    int done = m_completedFiles + m_failedFiles;
    m_progressBar->setFormat(
        QString("%1/%2 个文件 | %3 / %4 | %5%")
            .arg(done).arg(m_totalFiles)
            .arg(formatSize(downloadedDone)).arg(formatSize(totalSize))
            .arg(overallPercent));

    m_statusLabel->setText(
        QString("并行下载中 (%1 线程) | 已完成 %2/%3 个文件")
            .arg(m_activeMap.size()).arg(done).arg(m_totalFiles));
}

void MainWindow::onBatchComplete()
{
    for (auto *mgr : m_pool) { mgr->cancelDownload(); mgr->deleteLater(); }
    m_pool.clear();
    m_activeMap.clear();
    m_slotReceived.clear();
    m_slotTotal.clear();
    m_successIndices.clear();
    m_failedIndices.clear();

    setUiEnabled(true);
    m_progressBar->setValue(100);
    m_progressBar->setFormat("完成");

    QString summary = (m_failedFiles > 0)
        ? QString("下载完成: %1 成功, %2 失败 → %3")
              .arg(m_completedFiles).arg(m_failedFiles).arg(m_downloadDir)
        : QString("全部下载完成! 共 %1 个文件 → %2")
              .arg(m_totalFiles).arg(m_downloadDir);
    m_statusLabel->setText(summary);

    m_downloadAllBtn->setText("下载全部");
    m_downloadAllBtn->setEnabled(true);
    m_downloadSelBtn->setEnabled(!m_fileList->selectedItems().isEmpty());

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
    mgr->fetchJson(ModelParser::modelscopeApiUrl(
        m_modelInfo.namespaceName, m_modelInfo.modelName));
}

void MainWindow::tryFetchFromHuggingface()
{
    m_downloadSource = ModelParser::HuggingFace;
    m_statusLabel->setText("魔塔社区未找到，正在从 HuggingFace 查询...");

    auto *mgr = new DownloadManager(this);
    m_pool.append(mgr);
    connect(mgr, &DownloadManager::jsonFetched, this, &MainWindow::onJsonFetched);
    connect(mgr, &DownloadManager::jsonFetchError, this, &MainWindow::onJsonFetchError);
    mgr->fetchJson(ModelParser::huggingfaceApiUrl(
        m_modelInfo.namespaceName, m_modelInfo.modelName));
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
    m_itemBaseTexts.clear();
    m_downloadSource = ModelParser::Invalid;
    m_ggufMode = false;
    m_discoveringModelId = false;
    m_modelscopeFiles = ModelService::FileList();
    m_huggingfaceFiles = ModelService::FileList();

    for (auto *mgr : m_pool) { mgr->cancelDownload(); mgr->deleteLater(); }
    m_pool.clear();

    if (m_modelInfo.isModelId()) {
        m_discoveringModelId = true;
        setUiEnabled(false);
        tryFetchFromModelscope();
        return;
    }
    if (m_modelInfo.source == ModelParser::ModelScope) {
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
        if (!m_modelInfo.filePath.isEmpty())
            fileName = QFileInfo(m_modelInfo.filePath).fileName();
        if (fileName.isEmpty()) fileName = QUrl(input).fileName();
        if (fileName.isEmpty()) fileName = "download";

        m_fileNames.append(fileName);
        m_fileSizes.append(-1);
        m_itemBaseTexts.append(fileName);
        m_fileList->addItem(fileName);
        updateItemWidgets();
        m_downloadAllBtn->setEnabled(true);
    }
}

void MainWindow::onJsonFetched(const QByteArray &data, const QString &context)
{
    for (auto *mgr : m_pool) mgr->deleteLater();
    m_pool.clear();

    bool isMS = context.contains("modelscope.cn");
    ModelService::FileList parsed = isMS
        ? ModelService::parseModelScopeFiles(data)
        : ModelService::parseHuggingFaceFiles(data);

    if (!parsed.ok && !m_discoveringModelId) {
        QMessageBox::warning(this, isMS ? "ModelScope API 错误" : "HuggingFace API 错误", parsed.error);
        setUiEnabled(true);
        return;
    }

    if (m_discoveringModelId) {
        if (isMS) {
            m_modelscopeFiles = parsed;
            tryFetchFromHuggingface();
        } else {
            m_huggingfaceFiles = parsed;
            chooseModelIdSource();
        }
        return;
    }

    populateFileList(isMS ? ModelParser::ModelScope : ModelParser::HuggingFace, parsed.names, parsed.sizes);
}

void MainWindow::onJsonFetchError(const QString &error)
{
    for (auto *mgr : m_pool) mgr->deleteLater();
    m_pool.clear();

    if (m_discoveringModelId && m_downloadSource == ModelParser::ModelScope) {
        m_modelscopeFiles = ModelService::FileList();
        m_statusLabel->setText(
            QString("魔塔社区查询失败 (%1)，尝试 HuggingFace...").arg(error));
        tryFetchFromHuggingface();
        return;
    }
    if (m_discoveringModelId && m_downloadSource == ModelParser::HuggingFace) {
        m_huggingfaceFiles = ModelService::FileList();
        chooseModelIdSource();
        return;
    }
    setUiEnabled(true);
    QMessageBox::warning(this, "获取文件列表失败", error);
}

void MainWindow::onDownloadAllClicked()
{
    if (!m_activeMap.isEmpty()) {
        m_pendingIndices.clear();
        for (auto it = m_activeMap.begin(); it != m_activeMap.end(); ++it)
            it.key()->cancelDownload();
        for (auto *mgr : m_pool) mgr->deleteLater();
        m_pool.clear();
        m_activeMap.clear();
        m_slotReceived.clear();
        m_slotTotal.clear();
        m_successIndices.clear();
        m_failedIndices.clear();

        for (int i = 0; i < m_fileList->count(); ++i) {
            QListWidgetItem *item = m_fileList->item(i);
            if (item) {
                QWidget *w = m_fileList->itemWidget(item);
                if (w) {
                    QLabel *lbl = w->findChild<QLabel*>();
                    if (lbl) { lbl->setText(m_itemBaseTexts[i]); lbl->setStyleSheet(""); }
                }
            }
        }

        setUiEnabled(true);
        m_progressBar->setValue(0);
        m_progressBar->setFormat("就绪");
        m_statusLabel->setText("已取消下载");
        m_downloadAllBtn->setText("下载全部");
        return;
    }

    if (m_fileNames.isEmpty()) return;
    QList<int> all;
    for (int i = 0; i < m_fileNames.size(); ++i) all.append(i);
    startDownloads(all);
}

void MainWindow::onDownloadSelectedClicked()
{
    if (!m_activeMap.isEmpty()) return;

    QList<int> sel;
    for (auto *item : m_fileList->selectedItems()) {
        int row = m_fileList->row(item);
        if (row >= 0 && row < m_fileNames.size()) sel.append(row);
    }
    if (sel.isEmpty()) return;
    startDownloads(sel);
}

void MainWindow::onDownloadItemClicked(int fileIdx)
{
    if (!m_activeMap.isEmpty()) return;
    if (fileIdx < 0 || fileIdx >= m_fileNames.size()) return;
    startDownloads({fileIdx});
}

void MainWindow::onFileDoubleClicked(QListWidgetItem *item)
{
    if (!m_activeMap.isEmpty()) return;
    int row = m_fileList->row(item);
    if (row >= 0 && row < m_fileNames.size())
        startDownloads({row});
}

void MainWindow::onOpenFolderClicked()
{
    QString dir = m_downloadDir.isEmpty()
        ? DownloadManager::defaultDownloadDir() : m_downloadDir;
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
