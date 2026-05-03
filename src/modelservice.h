#pragma once

#include <QByteArray>
#include <QList>
#include <QStringList>
#include "modelparser.h"

class ModelService
{
public:
    struct FileList {
        bool ok = false;
        QString error;
        QStringList names;
        QList<qint64> sizes;
    };

    struct Selection {
        bool ok = false;
        ModelParser::Source source = ModelParser::Invalid;
        bool gguf = false;
        QStringList names;
        QList<qint64> sizes;
    };

    static FileList parseModelScopeFiles(const QByteArray &json);
    static FileList parseHuggingFaceFiles(const QByteArray &json);
    static Selection choosePreferred(const FileList &modelscope, const FileList &huggingface);
    static void filterGguf(const QStringList &names, const QList<qint64> &sizes, QStringList &outNames, QList<qint64> &outSizes);
};
