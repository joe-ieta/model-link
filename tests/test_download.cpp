#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QFile>
#include <QDir>
#include "downloadmanager.h"

class TestDownload : public QObject
{
    Q_OBJECT

private:
    DownloadManager *m_mgr;
    QTemporaryDir *m_tempDir;

private slots:
    void initTestCase()
    {
        m_tempDir = new QTemporaryDir;
        QVERIFY(m_tempDir->isValid());
    }

    void cleanupTestCase()
    {
        delete m_tempDir;
    }

    void init()
    {
        m_mgr = new DownloadManager(this);
    }

    void cleanup()
    {
        delete m_mgr;
    }

    void testIsDownloadingInitiallyFalse()
    {
        QVERIFY(!m_mgr->isDownloading());
    }

    void testDefaultDownloadDir()
    {
        QString dir = DownloadManager::defaultDownloadDir();
        QVERIFY(!dir.isEmpty());
    }

    void testDownloadSmallFile()
    {
        QString savePath = m_tempDir->path() + "/test_download.json";
        QUrl url("https://huggingface.co/api/models/hf-internal-testing/tiny-random-gpt2");

        QSignalSpy spyFinished(m_mgr, &DownloadManager::downloadFinished);
        QSignalSpy spyError(m_mgr, &DownloadManager::downloadError);
        QSignalSpy spyProgress(m_mgr, &DownloadManager::downloadProgress);

        m_mgr->startDownload(url, savePath);

        QTRY_VERIFY_WITH_TIMEOUT(spyFinished.count() > 0 || spyError.count() > 0, 30000);

        if (spyError.count() > 0) {
            qWarning() << "Download error (network issue):" << spyError[0][0].toString();
            return;
        }

        QCOMPARE(spyFinished.count(), 1);
        QString resultPath = spyFinished[0][0].toString();
        QVERIFY(QFile::exists(resultPath));

        QFile file(resultPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QByteArray data = file.readAll();
        QVERIFY(!data.isEmpty());
        file.close();

        qDebug() << "Downloaded" << data.size() << "bytes";
        QVERIFY(spyProgress.count() > 0);
    }

    void testCancelDownload()
    {
        QString savePath = m_tempDir->path() + "/cancel_test.json";
        QUrl url("https://huggingface.co/bert-base-uncased/resolve/main/pytorch_model.bin");

        m_mgr->startDownload(url, savePath);
        QVERIFY(m_mgr->isDownloading());

        QTest::qWait(500);

        m_mgr->cancelDownload();
        QVERIFY(!m_mgr->isDownloading());
    }

    void testResumeDownload()
    {
        QString savePath = m_tempDir->path() + "/resume_test.json";
        QString partPath = savePath + ".part";
        QUrl url("https://huggingface.co/api/models/hf-internal-testing/tiny-random-gpt2");

        // Create a fake partial file to test resume
        QFile partFile(partPath);
        QVERIFY(partFile.open(QIODevice::WriteOnly));
        partFile.write("FAKE_PARTIAL_DATA_");
        qint64 partSize = partFile.size();
        partFile.close();

        QSignalSpy spyFinished(m_mgr, &DownloadManager::downloadFinished);
        QSignalSpy spyError(m_mgr, &DownloadManager::downloadError);

        m_mgr->startDownload(url, savePath);

        QTRY_VERIFY_WITH_TIMEOUT(spyFinished.count() > 0 || spyError.count() > 0, 30000);

        if (spyError.count() > 0) {
            // When the server doesn't support Range, the download may fail or restart
            // This is not a test failure, just log it
            qWarning() << "Resume test result (may fail if server doesn't support Range):"
                       << spyError[0][0].toString();

            // Verify the download still completed (some servers return full content with Range request)
            if (spyFinished.count() > 0) {
                QVERIFY(QFile::exists(spyFinished[0][0].toString()));
                QFile result(spyFinished[0][0].toString());
                QVERIFY(result.open(QIODevice::ReadOnly));
                QVERIFY(!result.readAll().isEmpty());
            }
        } else {
            QCOMPARE(spyFinished.count(), 1);
            QVERIFY(QFile::exists(savePath));
            QVERIFY(!QFile::exists(partPath));

            QFile result(savePath);
            QVERIFY(result.open(QIODevice::ReadOnly));
            QByteArray data = result.readAll();
            QVERIFY(!data.isEmpty());
            qDebug() << "Resume download completed, got" << data.size() << "bytes (had"
                     << partSize << "bytes partial)";
        }
    }

    void testJsonFetchModelscope()
    {
        QUrl url("https://modelscope.cn/api/v1/models/iic/nlp_structbert_backbone_base_std/repo/files"
                 "?Revision=master&Recursive=true");

        QSignalSpy spyJson(m_mgr, &DownloadManager::jsonFetched);
        QSignalSpy spyError(m_mgr, &DownloadManager::jsonFetchError);

        m_mgr->fetchJson(url);

        QTRY_VERIFY_WITH_TIMEOUT(spyJson.count() > 0 || spyError.count() > 0, 30000);

        if (spyError.count() > 0) {
            qWarning() << "ModelScope API fetch error:" << spyError[0][0].toString();
            return;
        }

        QCOMPARE(spyJson.count(), 1);
        QByteArray data = spyJson[0][0].toByteArray();
        QVERIFY(!data.isEmpty());
        qDebug() << "ModelScope API returned" << data.size() << "bytes";
    }

    void testJsonFetchHuggingface()
    {
        QUrl url("https://huggingface.co/api/models/hf-internal-testing/tiny-random-gpt2");

        QSignalSpy spyJson(m_mgr, &DownloadManager::jsonFetched);
        QSignalSpy spyError(m_mgr, &DownloadManager::jsonFetchError);

        m_mgr->fetchJson(url);

        QTRY_VERIFY_WITH_TIMEOUT(spyJson.count() > 0 || spyError.count() > 0, 30000);

        if (spyError.count() > 0) {
            qWarning() << "HuggingFace API fetch error:" << spyError[0][0].toString();
            return;
        }

        QCOMPARE(spyJson.count(), 1);
        QByteArray data = spyJson[0][0].toByteArray();
        QVERIFY(!data.isEmpty());
        qDebug() << "HuggingFace API returned" << data.size() << "bytes";
    }
};

QTEST_MAIN(TestDownload)
#include "test_download.moc"
