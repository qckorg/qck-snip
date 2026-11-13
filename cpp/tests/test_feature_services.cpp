#include "core/platform_utils.h"
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
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QHostAddress>
#include <QTest>
#include <stdexcept>
#include <cstdio>
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
        qck::FeatureSettings::ReportDiagnostics();
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
        QTcpServer Server;
        QVERIFY(Server.listen(QHostAddress::LocalHost));
        const auto Response = http_request("POST", QString("http://127.0.0.1:%1/").arg(Server.serverPort()), {}, "private text", 0.1);
        QCOMPARE(Response.kind, HttpResponse::NetworkError);
        QCOMPARE(Response.reason, QString("Translation disabled by feature policy"));
        QVERIFY(!Server.waitForNewConnection(100));
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
        const QString Program = QDir(QCoreApplication::applicationDirPath()).filePath(QCK_TEST_APP_NAME);
        QVERIFY(QFile::exists(Program));
        for (const QStringList Arguments : {QStringList{"--qck-install-admin-task"}, QStringList{"--qck-remove-admin-task"},
                                            QStringList{"--qck-install-admin-task", "--xml", "untrusted.xml"}}) {
            QProcess Child;
            Child.start(Program, Arguments);
            QVERIFY(Child.waitForStarted());
            // The instrumented full application has a cold native-loader cost;
            // this checks fixed helper exit/no event loop, not startup speed.
            QVERIFY2(Child.waitForFinished(30000), qPrintable(QString("%1: %2; stderr: %3")
                .arg(Arguments.join(' '), Child.errorString(), QString::fromLocal8Bit(Child.readAllStandardError()))));
            QCOMPARE(Child.exitStatus(), QProcess::NormalExit);
            QCOMPARE(Child.exitCode(), 1); // Tests inherit the isolated sandbox; no real tasks.
        }
    }
    void MixedPrivilegeInstanceIdentity() {
        if (Mode != "privileged") return;
        QProcess Owned;
        Owned.start(QCoreApplication::applicationFilePath(), {"--QckIdentityChild"});
        QVERIFY(Owned.waitForStarted());
        QVERIFY(Owned.waitForReadyRead());
        const auto Cleanup = qScopeGuard([&] { if (Owned.state() != QProcess::NotRunning) { Owned.kill(); Owned.waitForFinished(); } });
        const auto Identity = get_process_identity(Owned.processId());
        QVERIFY(Identity);
        QProcess Medium;
        auto Environment = QProcessEnvironment::systemEnvironment();
        Environment.insert("QCK_TEST_KEEP_TOKEN", "0");
        Medium.setProcessEnvironment(Environment);
        const QString Launcher = QDir(QCoreApplication::applicationDirPath()).filePath("jietuba_test_launcher.exe");
        Medium.start(Launcher, {QCoreApplication::applicationFilePath(), "--QckMatchChild", QString::number(Owned.processId()), QString::number(Identity->first)});
        QVERIFY(Medium.waitForStarted());
        QVERIFY(Medium.waitForFinished(10000));
        QVERIFY2(Medium.exitCode() == 0, Medium.readAllStandardError().constData());
        QVERIFY(TerminateInstance(Owned.processId(), Identity->first));
        QVERIFY(Owned.waitForFinished());

        QTemporaryDir Directory;
        QVERIFY(Directory.isValid());
        const QString OtherPath = Directory.filePath(QFileInfo(QCoreApplication::applicationFilePath()).fileName());
        QVERIFY(QFile::copy(QStringLiteral(QCK_FAKE_INSTANCE_PATH), OtherPath));
        QProcess Other;
        Other.start(OtherPath, QStringList{});
        QVERIFY(Other.waitForStarted());
        QVERIFY(Other.waitForReadyRead());
        const auto OtherCleanup = qScopeGuard([&] { Other.kill(); Other.waitForFinished(); });
        const auto OtherIdentity = get_process_identity(Other.processId());
        QVERIFY(OtherIdentity);
        QVERIFY(!TerminateInstance(Other.processId(), OtherIdentity->first));
        QVERIFY(!TerminateInstance(Other.processId(), std::nullopt));
        QVERIFY(!Other.waitForFinished(50));
    }
};

int main(int argc, char** argv) {
    if (argc == 2 && QByteArray(argv[1]) == "--QckIdentityChild") {
        std::fputs("ready\n", stdout);
        std::fflush(stdout);
        Sleep(30000);
        return 0;
    }
    if (argc == 4 && QByteArray(argv[1]) == "--QckMatchChild") {
        bool ValidPid = false, ValidTime = false;
        const auto Pid = QByteArray(argv[2]).toLongLong(&ValidPid);
        const auto Time = QByteArray(argv[3]).toULongLong(&ValidTime);
        const bool Standard = qck::Security::CanLoadOcrRuntime();
        const bool Matched = ValidPid && ValidTime && MatchesInstance(Pid, Time);
        std::fprintf(stderr, "[Test:Instance] Standard=%d Matched=%d\n", int(Standard), int(Matched));
        return Standard && Matched ? 0 : 1;
    }
    QApplication App(argc, argv);
    FeatureServiceTests Tests;
    return QTest::qExec(&Tests, argc, argv);
}
#include "test_feature_services.moc"
