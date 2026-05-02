#include "modelparser.h"
#include <QStandardPaths>
#include <QRegularExpression>

ModelParser::ModelInfo ModelParser::parseInput(const QString &input)
{
    ModelInfo info;
    if (input.isEmpty()) {
        info.source = Invalid;
        return info;
    }

    info = parseUrl(input);
    if (info.isValid()) {
        return info;
    }

    if (tryParseModelId(input, info)) {
        return info;
    }

    info.source = Invalid;
    return info;
}

ModelParser::ModelInfo ModelParser::parseUrl(const QString &url)
{
    ModelInfo info;

    // ModelScope URL
    QRegularExpression msRe(
        R"(modelscope\.cn/models/([^/]+)/([^/?#]+))",
        QRegularExpression::CaseInsensitiveOption
    );
    auto msMatch = msRe.match(url);
    if (msMatch.hasMatch()) {
        info.namespaceName = msMatch.captured(1);
        info.modelName = msMatch.captured(2);
        info.source = ModelScope;

        QRegularExpression msFileRe(
            R"(modelscope\.cn/models/[^/]+/[^/?#]+(?:\?|/files/|/repo\?).*(?:FilePath=([^&?#]+)))",
            QRegularExpression::CaseInsensitiveOption
        );
        auto msFileMatch = msFileRe.match(url);
        if (msFileMatch.hasMatch()) {
            info.filePath = msFileMatch.captured(1);
        }
        return info;
    }

    // HuggingFace URL
    QRegularExpression hfRe(
        R"(huggingface\.co/([^/]+)/([^/?#]+))",
        QRegularExpression::CaseInsensitiveOption
    );
    auto hfMatch = hfRe.match(url);
    if (hfMatch.hasMatch()) {
        info.namespaceName = hfMatch.captured(1);
        info.modelName = hfMatch.captured(2);

        QRegularExpression hfResolveRe(
            R"(huggingface\.co/[^/]+/[^/]+/resolve/(?:main|master)/([^?#]+))",
            QRegularExpression::CaseInsensitiveOption
        );
        auto hfResolveMatch = hfResolveRe.match(url);
        if (hfResolveMatch.hasMatch()) {
            info.filePath = hfResolveMatch.captured(1);
            info.source = DirectUrl;
        } else {
            info.source = HuggingFace;
        }
        return info;
    }

    // Direct URL with http/https
    QUrl parsedUrl(url);
    if (parsedUrl.isValid() && (parsedUrl.scheme() == "http" || parsedUrl.scheme() == "https")) {
        info.source = DirectUrl;
        info.filePath = parsedUrl.fileName();
        return info;
    }

    info.source = Invalid;
    return info;
}

bool ModelParser::tryParseModelId(const QString &input, ModelInfo &info)
{
    // Model ID format: {namespace}/{model_name}
    // Examples: Qwen/Qwen3.6-27B, deepseek-ai/DeepSeek-R1, baichuan-inc/Baichuan2-7B-Chat
    QRegularExpression idRe(R"(^([a-zA-Z0-9][\w\-\.]*)\s*/\s*([a-zA-Z0-9][\w\-\.]+)$)");
    auto match = idRe.match(input.trimmed());
    if (!match.hasMatch()) {
        return false;
    }

    // Exclude URLs (contain :// or common TLDs used as file extensions)
    if (input.contains("://") || input.contains("modelscope.cn") || input.contains("huggingface.co")) {
        return false;
    }

    QString ns = match.captured(1);
    QString model = match.captured(2);

    // Model ID components should look like identifiers, not file names
    // A file would have a dot like "model.safetensors"
    if (model.contains('.') && !model.contains('-') && model.indexOf('.') < model.length() - 5) {
        return false;
    }

    info.namespaceName = ns;
    info.modelName = model;
    info.source = ModelId;
    return true;
}

QUrl ModelParser::modelscopeApiUrl(const QString &ns, const QString &model)
{
    return QUrl(QString(
        "https://modelscope.cn/api/v1/models/%1/%2/repo/files"
        "?Revision=master&Recursive=true"
    ).arg(ns, model));
}

QUrl ModelParser::huggingfaceApiUrl(const QString &ns, const QString &model)
{
    return QUrl(QString(
        "https://huggingface.co/api/models/%1/%2"
    ).arg(ns, model));
}

QUrl ModelParser::modelscopeDownloadUrl(const QString &ns, const QString &model, const QString &file)
{
    return QUrl(QString(
        "https://modelscope.cn/api/v1/models/%1/%2/repo"
        "?Revision=master&FilePath=%3"
    ).arg(ns, model, file));
}

QUrl ModelParser::huggingfaceDownloadUrl(const QString &ns, const QString &model, const QString &file)
{
    return QUrl(QString(
        "https://huggingface.co/%1/%2/resolve/main/%3"
    ).arg(ns, model, file));
}

QString ModelParser::defaultDownloadDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
}
