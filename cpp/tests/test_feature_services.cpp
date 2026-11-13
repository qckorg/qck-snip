#include "core/feature_settings.h"
#include "core/data_location.h"
#include "core/ocr_security.h"
#include "core/clipboard_utils.h"
#include "core/http_client.h"
#include "core/native.h"
#include "core/updater_process.h"
#include "clipboard/core/manager.h"
#include "ocr/ocr_manager.h"
#include "ocr/snipping_tool_ocr.h"
#include "translation/translation_manager.h"
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QScopeGuard>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>
#include <stdexcept>
#include <windows.h>

using namespace jietuba;

class FeatureServiceTests : public QObject {
    Q_OBJECT
    QString Mode;
private slots:
    void initTestCase() {
        Mode = qEnvironmentVariable("QCK_TEST_FEATURE_MODE");
        QSettings Settings(data_location().settings_file(), QSettings::IniFormat);
        Settings.beginGroup("features");
        for (const auto& Item : qck::GetFeatures())
            Settings.setValue(QString::fromUtf8(Item.Key.data(), qsizetype(Item.Key.size())), Mode == "enabled" || Mode == "unavailable" || Mode == "privileged");
        if (Mode == "malformed") {
            Settings.setValue("ocr.enabled", "not-a-boolean");
            Settings.setValue("unknown.secret-key", "not-logged");
        }
        Settings.endGroup();
        Settings.sync();
        QCOMPARE(Settings.status(), QSettings::NoError);
    }

    void PolicyAndSideEffects() {
        const auto& Policy = qck::FeatureSettings::GetPolicy();
        if (Mode == "enabled") {
            for (const auto& Item : qck::GetFeatures()) {
                QVERIFY(Policy.IsConfigured(Item.Id));
                QCOMPARE(Policy.IsEnabled(Item.Id), Policy.IsAvailable(Item.Id));
            }
            return;
        }
        if (Mode == "privileged") {
            // CI explicitly selects this no-write probe on an elevated host.
            QVERIFY2(!qck::Security::CanLoadOcrRuntime(), "The privileged-host probe requires an elevated host");
            QVERIFY(QDir().mkpath(snipping_tool_cache_root()));
            QTemporaryDir Planted(QDir(snipping_tool_cache_root()).filePath("Planted-Oneocr-XXXXXX"));
            QVERIFY(Planted.isValid());
            const QString Cache = Planted.path();
            for (const auto& Name : SNIPPING_TOOL_REQUIRED_FILES) {
                QFile File(QDir(Cache).filePath(Name));
                QVERIFY(File.open(QIODevice::WriteOnly));
                File.write("untrusted native payload");
            }
            const std::pair<QString, QString> Package{QFileInfo(Cache).fileName(), Cache};
            QVERIFY_EXCEPTION_THROWN(prepare_snipping_tool_files(Package), std::runtime_error);
            QVERIFY_EXCEPTION_THROWN(SnippingToolOcr{Package}, std::runtime_error);
            const auto Dll = QDir::toNativeSeparators(QDir(Cache).filePath("oneocr.dll")).toStdWString();
            QVERIFY(!GetModuleHandleW(Dll.c_str()));
            char Error[512]{};
            const QByteArray Runtime = QDir(Cache).filePath("onnxruntime.dll").toUtf8();
            QVERIFY(!jb_ppocr_init_runtime(Runtime.constData(), Error, sizeof(Error)));
            QVERIFY(QByteArray(Error).contains("non-elevated"));
            QVERIFY(!OCRManager::instance().initialize());
            return;
        }
        for (const auto& Item : qck::GetFeatures()) QVERIFY(!Policy.IsEnabled(Item.Id));
        if (Mode == "unavailable")
            for (const auto& Item : qck::GetFeatures()) { QVERIFY(Policy.IsConfigured(Item.Id)); QVERIFY(!Policy.IsAvailable(Item.Id)); }
        auto& History = clipboard::ClipboardManager::instance();
        QVERIFY(!QFile::exists(clipboard::ClipboardManager::default_db_path()));
        QVERIFY(!History.reset_storage());
        History.start_monitoring({});
        QVERIFY(!History.is_monitoring());
        QVERIFY(!QFile::exists(clipboard::ClipboardManager::default_db_path()));
        QVERIFY(!initialize_ocr());
        QVERIFY(!find_snipping_tool());
        QVERIFY_EXCEPTION_THROWN(prepare_snipping_tool_files({"disabled", "missing"}), std::runtime_error);
        QVERIFY(!updater_bundled());
        QVERIFY_EXCEPTION_THROWN(helper_path(), std::runtime_error);
        const auto Response = http_request("POST", "http://127.0.0.1:1/", {}, "private text", 0.1);
        QCOMPARE(Response.kind, HttpResponse::NetworkError);
        QVERIFY(Response.reason.contains("disabled"));
        auto* Translation = TranslationManager::instance();
        Translation->translate("private text");
        QVERIFY(!Translation->is_dialog_open());
        QImage Image(16, 16, QImage::Format_ARGB32);
        Image.fill(Qt::green);
        copy_image_to_clipboard(Image);
        QVERIFY(OpenClipboard(nullptr));
        QImage Copied;
        {
            const auto Cleanup = qScopeGuard([] { CloseClipboard(); });
            const UINT Format = RegisterClipboardFormatW(L"PNG");
            HANDLE Data = GetClipboardData(Format);
            QVERIFY(Data);
            const auto* Bytes = static_cast<const uchar*>(GlobalLock(Data));
            QVERIFY(Bytes);
            Copied = QImage::fromData(Bytes, static_cast<int>(GlobalSize(Data)), "PNG");
            GlobalUnlock(Data);
        }
        QVERIFY(!Copied.isNull());
        QCOMPARE(Copied.pixelColor(3, 3), QColor(Qt::green));
        QVERIFY(!QFile::exists(clipboard::ClipboardManager::default_db_path()));
        QSettings Settings(data_location().settings_file(), QSettings::IniFormat);
        Settings.setValue("features/ocr.enabled", true);
        Settings.sync();
        QVERIFY(!qck::FeatureSettings::IsEnabled(qck::Feature::Ocr));
    }
    void FixedHelperArgumentsNeverOpenTheDesktop() {
        const QString Program = QDir(QCoreApplication::applicationDirPath()).filePath("qck-snip.exe");
        QVERIFY(QFile::exists(Program));
        for (const QStringList Arguments : {QStringList{"--qck-install-admin-task"}, QStringList{"--qck-remove-admin-task"},
                                            QStringList{"--qck-install-admin-task", "--xml", "untrusted.xml"}}) {
            QProcess Child;
            Child.start(Program, Arguments);
            QVERIFY(Child.waitForStarted());
            QVERIFY(Child.waitForFinished(10000));
            QCOMPARE(Child.exitStatus(), QProcess::NormalExit);
            QCOMPARE(Child.exitCode(), 1); // Tests inherit the isolated sandbox; no real tasks.
        }
    }
};

QTEST_MAIN(FeatureServiceTests)
#include "test_feature_services.moc"
