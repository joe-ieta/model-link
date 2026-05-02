#include <QtTest>
#include "modelparser.h"

class TestModelParser : public QObject
{
    Q_OBJECT

private slots:
    void testParseInputModelId()
    {
        auto info = ModelParser::parseInput("Qwen/Qwen3.6-27B");
        QVERIFY(info.isValid());
        QVERIFY(info.isModelId());
        QCOMPARE(info.namespaceName, QString("Qwen"));
        QCOMPARE(info.modelName, QString("Qwen3.6-27B"));
    }

    void testParseInputModelIdComplex()
    {
        auto info = ModelParser::parseInput("deepseek-ai/DeepSeek-R1-0528");
        QVERIFY(info.isValid());
        QVERIFY(info.isModelId());
        QCOMPARE(info.namespaceName, QString("deepseek-ai"));
        QCOMPARE(info.modelName, QString("DeepSeek-R1-0528"));
    }

    void testParseInputModelIdWithUnderscore()
    {
        auto info = ModelParser::parseInput("baichuan-inc/Baichuan2_7B_Chat");
        QVERIFY(info.isValid());
        QVERIFY(info.isModelId());
        QCOMPARE(info.namespaceName, QString("baichuan-inc"));
        QCOMPARE(info.modelName, QString("Baichuan2_7B_Chat"));
    }

    void testParseInputModelscopeUrl()
    {
        auto info = ModelParser::parseInput(
            "https://modelscope.cn/models/deepseek-ai/DeepSeek-R1/files");
        QCOMPARE(info.source, ModelParser::ModelScope);
        QVERIFY(!info.isModelId());
        QCOMPARE(info.namespaceName, QString("deepseek-ai"));
        QCOMPARE(info.modelName, QString("DeepSeek-R1"));
    }

    void testParseInputHuggingfaceUrl()
    {
        auto info = ModelParser::parseInput(
            "https://huggingface.co/deepseek-ai/DeepSeek-R1/tree/main");
        QCOMPARE(info.source, ModelParser::HuggingFace);
        QVERIFY(!info.isModelId());
        QCOMPARE(info.namespaceName, QString("deepseek-ai"));
        QCOMPARE(info.modelName, QString("DeepSeek-R1"));
    }

    void testParseInputHuggingfaceResolveUrl()
    {
        auto info = ModelParser::parseInput(
            "https://huggingface.co/deepseek-ai/DeepSeek-R1/resolve/main/config.json");
        QCOMPARE(info.source, ModelParser::DirectUrl);
        QCOMPARE(info.filePath, QString("config.json"));
    }

    void testParseInputDirectUrl()
    {
        auto info = ModelParser::parseInput("https://example.com/files/model.safetensors");
        QCOMPARE(info.source, ModelParser::DirectUrl);
        QCOMPARE(info.filePath, QString("model.safetensors"));
    }

    void testParseInputEmpty()
    {
        auto info = ModelParser::parseInput("");
        QVERIFY(!info.isValid());
    }

    void testParseInputInvalidId()
    {
        auto info = ModelParser::parseInput("not_a_valid_input_without_slash");
        QVERIFY(!info.isValid());
    }

    void testModelscopeApiUrl()
    {
        QUrl url = ModelParser::modelscopeApiUrl("Qwen", "Qwen3.6-27B");
        QVERIFY(url.toString().contains("modelscope.cn"));
        QVERIFY(url.toString().contains("Qwen"));
        QVERIFY(url.toString().contains("Qwen3.6-27B"));
        QVERIFY(url.toString().contains("Recursive=true"));
    }

    void testHuggingfaceApiUrl()
    {
        QUrl url = ModelParser::huggingfaceApiUrl("Qwen", "Qwen3.6-27B");
        QVERIFY(url.toString().contains("huggingface.co/api/models"));
        QVERIFY(url.toString().contains("Qwen"));
        QVERIFY(url.toString().contains("Qwen3.6-27B"));
    }

    void testModelscopeDownloadUrl()
    {
        QUrl url = ModelParser::modelscopeDownloadUrl("Qwen", "Qwen3.6-27B", "config.json");
        QVERIFY(url.toString().contains("modelscope.cn"));
        QVERIFY(url.toString().contains("FilePath=config.json"));
    }

    void testHuggingfaceDownloadUrl()
    {
        QUrl url = ModelParser::huggingfaceDownloadUrl("Qwen", "Qwen3.6-27B", "config.json");
        QVERIFY(url.toString().contains("huggingface.co"));
        QVERIFY(url.toString().contains("resolve/main/config.json"));
    }

    void testDefaultDownloadDir()
    {
        QString dir = ModelParser::defaultDownloadDir();
        QVERIFY(!dir.isEmpty());
        QVERIFY(dir.contains("Download"));
    }

    void testParseInputSpacesAroundSlash()
    {
        auto info = ModelParser::parseInput("Qwen / Qwen3.6-27B");
        QVERIFY(info.isValid());
        QCOMPARE(info.namespaceName, QString("Qwen"));
        QCOMPARE(info.modelName, QString("Qwen3.6-27B"));
    }
};

QTEST_MAIN(TestModelParser)
#include "test_modelparser.moc"
