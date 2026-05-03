#include "clirunner.h"
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>
#include <iostream>

static QString formatSize(qint64 s)
{
    if (s < 1024) return QString("%1 B").arg(s);
    if (s < 1048576) return QString("%1 KB").arg(s / 1024.0, 0, 'f', 1);
    if (s < 1073741824LL) return QString("%1 MB").arg(s / (1048576.0), 0, 'f', 1);
    return QString("%1 GB").arg(s / (1073741824.0), 0, 'f', 2);
}

CliRunner::CliRunner(const Options &opts, QObject *parent)
    : QObject(parent), m_opts(opts)
{
    if (m_opts.threads < 1) m_opts.threads = 1;
    if (m_opts.threads > 8) m_opts.threads = 8;
}

void CliRunner::printHelp()
{
    QTextStream out(stdout);
    out << "ModelLink - 模型下载工具  v1.0.0\n\n"
        << "用法: ModelLink.exe [模型标识|URL] [选项]\n\n"
        << "参数:\n"
        << "  <model>              模型标识 (如 Qwen/Qwen3.6-27B) 或模型链接\n\n"
        << "选项:\n"
        << "  --gguf               仅下载 GGUF 格式文件\n"
        << "  -a, --all            下载所有文件 (不限于 GGUF)\n"
        << "  -o, --output <dir>   指定输出目录\n"
        << "  -t, --threads <n>    并发下载线程数 (1-8, 默认 3)\n"
        << "  -l, --list           仅列出文件，不下载\n"
        << "  -h, --help           显示此帮助\n\n"
        << "示例:\n"
        << "  ModelLink.exe Qwen/Qwen3.6-27B              # 下载 GGUF 文件\n"
        << "  ModelLink.exe Qwen/Qwen3.6-27B -a           # 下载所有文件\n"
        << "  ModelLink.exe Qwen/Qwen3.6-27B -t 5         # 5 线程下载\n"
        << "  ModelLink.exe Qwen/Qwen3.6-27B -o D:/models # 指定输出目录\n"
        << "  ModelLink.exe Qwen/Qwen3.6-27B -l           # 列出文件\n"
        << "  ModelLink.exe (无参数)                       # 启动图形界面\n";
    out.flush();
}

int CliRunner::run()
{
    m_modelInfo = ModelParser::parseInput(m_opts.model);
    if (!m_modelInfo.isValid()) {
        QTextStream(stderr) << "错误: 无法识别的模型标识或 URL\n";
        return 1;
    }

    printStatus("正在查询模型: " + m_opts.model + " ...");
    fetchFileLists();
    return QCoreApplication::exec();
}

void CliRunner::fetchFileLists()
{
    if (m_modelInfo.isModelId()) {
        auto *mgr = new DownloadManager(this);
        connect(mgr, &DownloadManager::jsonFetched, this, &CliRunner::onModelscopeFetched);
        connect(mgr, &DownloadManager::jsonFetchError, this, &CliRunner::onFetchError);
        mgr->fetchJson(ModelParser::modelscopeApiUrl(
            m_modelInfo.namespaceName, m_modelInfo.modelName));
        return;
    }
    if (m_modelInfo.source == ModelParser::ModelScope) {
        m_hfDone = true;
        auto *mgr = new DownloadManager(this);
        connect(mgr, &DownloadManager::jsonFetched, this, &CliRunner::onModelscopeFetched);
        connect(mgr, &DownloadManager::jsonFetchError, this, &CliRunner::onFetchError);
        mgr->fetchJson(ModelParser::modelscopeApiUrl(
            m_modelInfo.namespaceName, m_modelInfo.modelName));
        return;
    }
    if (m_modelInfo.source == ModelParser::HuggingFace) {
        m_msDone = true;
        auto *mgr = new DownloadManager(this);
        connect(mgr, &DownloadManager::jsonFetched, this, &CliRunner::onHuggingfaceFetched);
        connect(mgr, &DownloadManager::jsonFetchError, this, &CliRunner::onFetchError);
        mgr->fetchJson(ModelParser::huggingfaceApiUrl(
            m_modelInfo.namespaceName, m_modelInfo.modelName));
        return;
    }

    QTextStream(stderr) << "错误: 不支持的 URL 类型\n";
    QCoreApplication::exit(1);
}

void CliRunner::onModelscopeFetched(const QByteArray &data, const QString &ctx)
{
    Q_UNUSED(ctx)
    m_msFiles = ModelService::parseModelScopeFiles(data);
    m_msDone = true;

    if (m_modelInfo.isModelId() && !m_hfDone) {
        auto *mgr = new DownloadManager(this);
        connect(mgr, &DownloadManager::jsonFetched, this, &CliRunner::onHuggingfaceFetched);
        connect(mgr, &DownloadManager::jsonFetchError, this, &CliRunner::onFetchError);
        mgr->fetchJson(ModelParser::huggingfaceApiUrl(
            m_modelInfo.namespaceName, m_modelInfo.modelName));
        return;
    }

    m_hfDone = true;
    onBothFetched();
}

void CliRunner::onHuggingfaceFetched(const QByteArray &data, const QString &ctx)
{
    Q_UNUSED(ctx)
    m_hfFiles = ModelService::parseHuggingFaceFiles(data);
    m_hfDone = true;
    onBothFetched();
}

void CliRunner::onFetchError(const QString &error)
{
    if (m_modelInfo.isModelId() && !m_msDone) {
        m_msFiles = ModelService::FileList();
        m_msDone = true;
        auto *mgr = new DownloadManager(this);
        connect(mgr, &DownloadManager::jsonFetched, this, &CliRunner::onHuggingfaceFetched);
        connect(mgr, &DownloadManager::jsonFetchError, this, &CliRunner::onFetchError);
        mgr->fetchJson(ModelParser::huggingfaceApiUrl(
            m_modelInfo.namespaceName, m_modelInfo.modelName));
        return;
    }
    if (m_modelInfo.isModelId() && !m_hfDone) {
        m_hfFiles = ModelService::FileList();
        m_hfDone = true;
        onBothFetched();
        return;
    }

    QTextStream(stderr) << "错误: " << error << "\n";
    QCoreApplication::exit(1);
}

void CliRunner::onBothFetched()
{
    auto selection = ModelService::choosePreferred(m_msFiles, m_hfFiles);
    m_downloadSource = selection.source;

    if (!selection.ok) {
        QTextStream(stderr) << "未找到可下载的文件。\n";
        QCoreApplication::exit(1);
        return;
    }

    if (m_opts.ggufOnly) {
        QStringList ggufNames;
        QList<qint64> ggufSizes;
        ModelService::filterGguf(selection.names, selection.sizes, ggufNames, ggufSizes);
        if (ggufNames.isEmpty()) {
            ModelService::FileList alt = m_downloadSource == ModelParser::ModelScope ? m_hfFiles : m_msFiles;
            ModelService::filterGguf(alt.names, alt.sizes, ggufNames, ggufSizes);
            if (!ggufNames.isEmpty()) m_downloadSource = (m_downloadSource == ModelParser::ModelScope)
                ? ModelParser::HuggingFace : ModelParser::ModelScope;
        }
        if (ggufNames.isEmpty()) {
            QTextStream(stderr) << "未找到 GGUF 格式文件。\n";
            QCoreApplication::exit(1);
            return;
        }
        m_selectedFiles = ggufNames;
        m_selectedSizes = ggufSizes;
    } else {
        m_selectedFiles = selection.names;
        m_selectedSizes = selection.sizes;
    }

    printFileList();

    if (m_opts.listOnly) {
        QCoreApplication::exit(0);
        return;
    }

    startDownloads();
}

void CliRunner::printFileList()
{
    QTextStream out(stdout);
    QString src = m_downloadSource == ModelParser::ModelScope
        ? (m_opts.ggufOnly ? "魔塔社区 (GGUF)" : "魔塔社区")
        : (m_opts.ggufOnly ? "HuggingFace (GGUF)" : "HuggingFace");
    out << "来源: " << src << "\n";
    out << "文件数: " << m_selectedFiles.size() << "\n";
    qint64 total = 0;
    for (int i = 0; i < m_selectedFiles.size(); ++i) {
        if (i < m_selectedSizes.size() && m_selectedSizes[i] > 0) total += m_selectedSizes[i];
        out << "  [" << (i + 1) << "] " << m_selectedFiles[i];
        if (i < m_selectedSizes.size() && m_selectedSizes[i] > 0)
            out << "  (" << formatSize(m_selectedSizes[i]) << ")";
        out << "\n";
    }
    if (total > 0) out << "总大小: " << formatSize(total) << "\n";
    out.flush();
}

QString CliRunner::outputPath(const QString &file) const
{
    return QDir(m_outputDir).filePath(file);
}

void CliRunner::startDownloads()
{
    m_total = m_selectedFiles.size();
    m_nextIndex = 0;
    m_completed = 0;
    m_failed = 0;
    m_running = 0;

    m_outputDir = m_opts.outputDir;
    if (m_outputDir.isEmpty()) {
        QString modelFolder = QString("model_link_%1_%2")
            .arg(m_modelInfo.namespaceName, m_modelInfo.modelName)
            .replace('/', '_').replace('\\', '_');
        m_outputDir = QDir(DownloadManager::defaultDownloadDir()).filePath(modelFolder);
    }
    QDir().mkpath(m_outputDir);

    QTextStream out(stdout);
    out << "输出目录: " << m_outputDir << "\n" << "线程数: " << m_opts.threads << "\n\n";
    out.flush();

    // Start initial batch
    for (int i = 0; i < m_opts.threads && i < m_total; ++i)
        startNextDownload();
}

void CliRunner::startNextDownload()
{
    if (m_nextIndex >= m_total) return;

    int idx = m_nextIndex++;
    const QString &file = m_selectedFiles[idx];
    QString savePath = outputPath(file);

    QUrl url;
    if (m_downloadSource == ModelParser::ModelScope)
        url = ModelParser::modelscopeDownloadUrl(m_modelInfo.namespaceName, m_modelInfo.modelName, file);
    else
        url = ModelParser::huggingfaceDownloadUrl(m_modelInfo.namespaceName, m_modelInfo.modelName, file);

    auto *mgr = new DownloadManager(this);
    m_running++;
    printStatus(QString("[%1/%2] 下载中: %3").arg(m_completed + m_failed + 1).arg(m_total).arg(file));

    connect(mgr, &DownloadManager::downloadFinished, this, [this, idx, mgr](const QString &) {
        m_completed++;
        m_running--;
        mgr->deleteLater();
        printStatus(QString::fromUtf8("[%1/%2] \u2713 %3")
            .arg(m_completed + m_failed).arg(m_total).arg(m_selectedFiles[idx]));
        if (m_completed + m_failed >= m_total) {
            QTextStream(stdout) << "\n完成: 成功 " << m_completed << ", 失败 " << m_failed << "\n";
            QCoreApplication::exit(m_failed > 0 ? 1 : 0);
        } else {
            startNextDownload();
        }
    });

    connect(mgr, &DownloadManager::downloadError, this, [this, idx, mgr](const QString &err) {
        m_failed++;
        m_running--;
        mgr->deleteLater();
        QTextStream(stderr) << "\n[" << (m_completed + m_failed) << "/" << m_total << "] "
                            << "✗ " << m_selectedFiles[idx] << " - " << err << "\n";
        if (m_completed + m_failed >= m_total) {
            QTextStream(stdout) << "\n完成: 成功 " << m_completed << ", 失败 " << m_failed << "\n";
            QCoreApplication::exit(m_failed > 0 ? 1 : 0);
        } else {
            startNextDownload();
        }
    });

    mgr->startDownload(url, savePath);
}

void CliRunner::onDownloadFinished(const QString &) { /* handled by lambdas */ }
void CliRunner::onDownloadError(const QString &)    { /* handled by lambdas */ }

void CliRunner::printStatus(const QString &msg)
{
    QTextStream out(stdout);
    out << msg << "\n";
    out.flush();
}
