#include <QtTest>
#include "modelservice.h"

class TestModelService : public QObject
{
    Q_OBJECT
private slots:
    void testParseModelScopeFiles()
    {
        QByteArray json = R"({"Code":200,"Data":{"Files":[{"Name":"a.gguf","Size":10},{"Name":"b.txt","Size":2}]}})";
        auto files = ModelService::parseModelScopeFiles(json);
        QVERIFY(files.ok);
        QCOMPARE(files.names.size(), 2);
        QCOMPARE(files.names[0], QString("a.gguf"));
        QCOMPARE(files.sizes[0], 10LL);
    }

    void testParseHuggingFaceFiles()
    {
        QByteArray json = R"({"siblings":[{"rfilename":"dir/a.bin","size":33},{"rfilename":"q.gguf","size":99}]})";
        auto files = ModelService::parseHuggingFaceFiles(json);
        QVERIFY(files.ok);
        QCOMPARE(files.names.size(), 2);
        QCOMPARE(files.names[1], QString("q.gguf"));
        QCOMPARE(files.sizes[1], 99LL);
    }

    void testChooseModelScopeGgufFirst()
    {
        ModelService::FileList ms{true, {}, {"a.gguf", "a.bin"}, {10, 20}};
        ModelService::FileList hf{true, {}, {"h.gguf"}, {30}};
        auto selected = ModelService::choosePreferred(ms, hf);
        QVERIFY(selected.ok);
        QCOMPARE(selected.source, ModelParser::ModelScope);
        QVERIFY(selected.gguf);
        QCOMPARE(selected.names, QStringList{"a.gguf"});
    }

    void testChooseHuggingFaceGgufBeforeModelScopeNonGguf()
    {
        ModelService::FileList ms{true, {}, {"a.bin"}, {20}};
        ModelService::FileList hf{true, {}, {"h.gguf"}, {30}};
        auto selected = ModelService::choosePreferred(ms, hf);
        QVERIFY(selected.ok);
        QCOMPARE(selected.source, ModelParser::HuggingFace);
        QVERIFY(selected.gguf);
        QCOMPARE(selected.names, QStringList{"h.gguf"});
    }

    void testFallbackToModelScopeNonGguf()
    {
        ModelService::FileList ms{true, {}, {"a.bin"}, {20}};
        ModelService::FileList hf{true, {}, {"h.bin"}, {30}};
        auto selected = ModelService::choosePreferred(ms, hf);
        QVERIFY(selected.ok);
        QCOMPARE(selected.source, ModelParser::ModelScope);
        QVERIFY(!selected.gguf);
        QCOMPARE(selected.names, QStringList{"a.bin"});
    }
};

QTEST_MAIN(TestModelService)
#include "test_modelservice.moc"
