#pragma once

#include <QString>
#include <QUrl>

class ModelParser
{
public:
    enum Source {
        Invalid,
        ModelScope,
        HuggingFace,
        DirectUrl,
        ModelId
    };

    struct ModelInfo {
        QString namespaceName;
        QString modelName;
        QString filePath;
        Source source;
        bool isValid() const { return source != Invalid; }
        bool isModelId() const { return source == ModelId; }
    };

    static ModelInfo parseInput(const QString &input);
    static ModelInfo parseUrl(const QString &url);
    static bool tryParseModelId(const QString &input, ModelInfo &info);

    static QUrl modelscopeApiUrl(const QString &ns, const QString &model);
    static QUrl huggingfaceApiUrl(const QString &ns, const QString &model);
    static QUrl modelscopeDownloadUrl(const QString &ns, const QString &model, const QString &file);
    static QUrl huggingfaceDownloadUrl(const QString &ns, const QString &model, const QString &file);

    static QString defaultDownloadDir();
};
