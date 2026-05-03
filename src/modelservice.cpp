#include "modelservice.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

ModelService::FileList ModelService::parseModelScopeFiles(const QByteArray &json)
{
    FileList result;
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        result.error = "无法解析服务器响应";
        return result;
    }

    QJsonObject root = doc.object();
    int code = root["Code"].toInt(-1);
    if (code != 200) {
        result.error = root["Message"].toString("未知错误");
        return result;
    }

    QJsonArray files = root["Data"].toObject()["Files"].toArray();
    for (const auto &fileValue : files) {
        QJsonObject file = fileValue.toObject();
        QString name = file["Name"].toString();
        if (!name.isEmpty()) {
            result.names.append(name);
            result.sizes.append(static_cast<qint64>(file["Size"].toDouble()));
        }
    }

    result.ok = !result.names.isEmpty();
    if (!result.ok) result.error = "该模型没有找到文件";
    return result;
}

ModelService::FileList ModelService::parseHuggingFaceFiles(const QByteArray &json)
{
    FileList result;
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        result.error = "无法解析服务器响应";
        return result;
    }

    QJsonObject root = doc.object();
    if (root.contains("error")) {
        result.error = root["error"].toString("未知错误");
        return result;
    }

    QJsonArray siblings = root["siblings"].toArray();
    for (const auto &siblingValue : siblings) {
        QJsonObject sibling = siblingValue.toObject();
        QString name = sibling["rfilename"].toString();
        if (!name.isEmpty()) {
            result.names.append(name);
            result.sizes.append(static_cast<qint64>(sibling["size"].toDouble()));
        }
    }

    result.ok = !result.names.isEmpty();
    if (!result.ok) result.error = "该模型没有找到文件";
    return result;
}

void ModelService::filterGguf(const QStringList &names, const QList<qint64> &sizes, QStringList &outNames, QList<qint64> &outSizes)
{
    outNames.clear();
    outSizes.clear();
    for (int i = 0; i < names.size(); ++i) {
        if (names[i].endsWith(".gguf", Qt::CaseInsensitive)) {
            outNames.append(names[i]);
            outSizes.append(i < sizes.size() ? sizes[i] : -1);
        }
    }
}

ModelService::Selection ModelService::choosePreferred(const FileList &modelscope, const FileList &huggingface)
{
    Selection result;
    QStringList msGgufNames, hfGgufNames;
    QList<qint64> msGgufSizes, hfGgufSizes;

    if (modelscope.ok) filterGguf(modelscope.names, modelscope.sizes, msGgufNames, msGgufSizes);
    if (huggingface.ok) filterGguf(huggingface.names, huggingface.sizes, hfGgufNames, hfGgufSizes);

    if (!msGgufNames.isEmpty()) {
        result = {true, ModelParser::ModelScope, true, msGgufNames, msGgufSizes};
    } else if (!hfGgufNames.isEmpty()) {
        result = {true, ModelParser::HuggingFace, true, hfGgufNames, hfGgufSizes};
    } else if (modelscope.ok) {
        result = {true, ModelParser::ModelScope, false, modelscope.names, modelscope.sizes};
    } else if (huggingface.ok) {
        result = {true, ModelParser::HuggingFace, false, huggingface.names, huggingface.sizes};
    }

    return result;
}
