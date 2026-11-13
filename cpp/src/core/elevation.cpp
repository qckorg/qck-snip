#include "core/elevation.h"
#include "core/feature_settings.h"
#include "core/constants.h"
#include "core/logger.h"
#include "core/platform_utils.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QScopeGuard>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define SECURITY_WIN32
#include <security.h>
#include <shellapi.h>
#include <taskschd.h>
#include <wrl/client.h>

namespace jietuba::elevation {
namespace {
const char TASK_XML[] = R"(<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.3" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <RegistrationInfo>
    <Description>qck-snip</Description>
  </RegistrationInfo>
  <Triggers>
    <LogonTrigger>
      <Enabled>true</Enabled>
      <UserId>{user}</UserId>
      <Delay>PT3S</Delay>
    </LogonTrigger>
  </Triggers>
  <Principals>
    <Principal id="Author">
      <UserId>{user}</UserId>
      <LogonType>InteractiveToken</LogonType>
      <RunLevel>HighestAvailable</RunLevel>
    </Principal>
  </Principals>
  <Settings>
    <MultipleInstancesPolicy>Parallel</MultipleInstancesPolicy>
    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>
    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>
    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>
    <Priority>4</Priority>
  </Settings>
  <Actions Context="Author">
    <Exec>
      <Command>{command}</Command>
      <WorkingDirectory>{workdir}</WorkingDirectory>
    </Exec>
  </Actions>
</Task>
)";

SchtasksRunner &test_runner() {
    static SchtasksRunner runner;
    return runner;
}

struct TestEnvironment {
    std::optional<QString> exe, state, user;
};

TestEnvironment &test_environment() {
    static TestEnvironment environment;
    return environment;
}

// Element content needs only these three characters escaped.
QString escape(QString text) {
    return text.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;");
}

bool is_sep(QChar c) { return c == '\\' || c == '/'; }

// Everything before the last separator, drive kept and trailing separators dropped: the task's working directory.
QString dirname(const QString &path) {
    int drive = 0;
    if (path.size() >= 2 && path[1] == ':') drive = 2;
    const QString rest = path.mid(drive);
    int i = static_cast<int>(rest.size());
    while (i > 0 && !is_sep(rest[i - 1])) --i;
    QString head = rest.left(i);
    int end = static_cast<int>(head.size());
    while (end > 0 && is_sep(head[end - 1])) --end;
    if (end > 0) head.truncate(end);
    return path.left(drive) + head;
}

// The hand-off runs before QApplication exists, when applicationFilePath() is still empty.
QString own_exe() {
    if (test_environment().exe) return *test_environment().exe;
    wchar_t path[MAX_PATH * 4] = {};
    const DWORD length = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    return QString::fromWCharArray(path, static_cast<int>(length));
}

QString user_name() {
    if (test_environment().user) return *test_environment().user;
    wchar_t buffer[256] = {};
    ULONG size = static_cast<ULONG>(std::size(buffer));
    if (GetUserNameExW(NameSamCompatible, buffer, &size)) return QString::fromWCharArray(buffer, static_cast<int>(size));
    log_exception(QString("[WinError %1]").arg(GetLastError()), T("查询当前用户名"));
    return {};
}

QString GetSchtasksPath() {
    wchar_t Directory[MAX_PATH + 1]{};
    const UINT Length = GetSystemDirectoryW(Directory, MAX_PATH);
    return Length && Length < MAX_PATH ? QDir(QString::fromWCharArray(Directory, Length)).filePath("schtasks.exe") : QString();
}

TaskWriter& GetTaskWriter() { static TaskWriter Writer; return Writer; }

struct BString {
    BSTR Value;
    explicit BString(const QString& Text) : Value(SysAllocStringLen(reinterpret_cast<const OLECHAR*>(Text.utf16()), static_cast<UINT>(Text.size()))) {}
    ~BString() { SysFreeString(Value); }
    BString(const BString&) = delete;
    BString& operator=(const BString&) = delete;
};

std::pair<int, QByteArray> WriteTask(bool Install, const QString& Xml, bool Validate = false) {
    if (!Validate && GetTaskWriter()) return GetTaskWriter()(Install, Xml);
    if (!Validate && !test_sandbox_dir().isEmpty()) return {-1, {}};
    const HRESULT Initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(Initialized) && Initialized != RPC_E_CHANGED_MODE) return {static_cast<int>(Initialized), {}};
    const auto Cleanup = qScopeGuard([&] { if (SUCCEEDED(Initialized)) CoUninitialize(); });
    Microsoft::WRL::ComPtr<ITaskService> Service;
    HRESULT Result = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&Service));
    VARIANT Empty{};
    if (SUCCEEDED(Result)) Result = Service->Connect(Empty, Empty, Empty, Empty);
    Microsoft::WRL::ComPtr<ITaskFolder> Root;
    const BString RootName(QStringLiteral("\\")), Name(task_name()), Definition(Xml), Account(user_name());
    if (!RootName.Value || !Name.Value || !Definition.Value || !Account.Value || user_name().isEmpty()) return {static_cast<int>(E_INVALIDARG), {}};
    if (SUCCEEDED(Result)) Result = Service->GetFolder(RootName.Value, &Root);
    if (SUCCEEDED(Result)) {
        if (Install) {
            VARIANT User{};
            User.vt = VT_BSTR;
            User.bstrVal = Account.Value;
            Microsoft::WRL::ComPtr<IRegisteredTask> Registered;
            Result = Root->RegisterTask(Name.Value, Definition.Value, Validate ? TASK_VALIDATE_ONLY : TASK_CREATE_OR_UPDATE, User, Empty,
                                       TASK_LOGON_INTERACTIVE_TOKEN, Empty, &Registered);
        } else {
            Result = Root->DeleteTask(Name.Value, 0);
            if (Result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) Result = S_OK;
        }
    }
    return {SUCCEEDED(Result) ? 0 : static_cast<int>(Result), {}};
}

QString quote(const QString &arg) { return arg.contains(' ') || arg.isEmpty() ? '"' + arg + '"' : arg; }

std::pair<int, QByteArray> run_schtasks_process(const QStringList &args) {
    // Tests never touch the real scheduled tasks.
    if (!test_sandbox_dir().isEmpty()) return {-1, {}};
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};
    HANDLE read_pipe = nullptr, write_pipe = nullptr;
    // Large enough for the task XML, so the child never blocks on a full pipe while it is awaited.
    if (!CreatePipe(&read_pipe, &write_pipe, &attributes, 1 << 16)) {
        log_exception(QString("[WinError %1]").arg(GetLastError()), T("运行 schtasks"));
        return {-1, {}};
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = write_pipe;
    startup.hStdError = write_pipe;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    const QString Program = GetSchtasksPath();
    if (Program.isEmpty()) { CloseHandle(read_pipe); CloseHandle(write_pipe); return {-1, {}}; }
    QStringList parts{quote(Program)};
    for (const auto &arg : args) parts << quote(arg);
    std::wstring command_line = parts.join(' ').toStdWString();
    PROCESS_INFORMATION process{};
    const BOOL started = CreateProcessW(reinterpret_cast<LPCWSTR>(Program.utf16()), command_line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    CloseHandle(write_pipe);
    if (!started) {
        log_exception(QString("[WinError %1]").arg(GetLastError()), T("运行 schtasks"));
        CloseHandle(read_pipe);
        return {-1, {}};
    }
    DWORD code = static_cast<DWORD>(-1);
    if (WaitForSingleObject(process.hProcess, 15000) == WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess, &code);
    else TerminateProcess(process.hProcess, 1);
    QByteArray out;
    char chunk[4096];
    DWORD read = 0;
    while (ReadFile(read_pipe, chunk, sizeof(chunk), &read, nullptr) && read > 0) out.append(chunk, static_cast<qsizetype>(read));
    CloseHandle(read_pipe);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return {static_cast<int>(code), out};
}

std::pair<int, QByteArray> run_schtasks(const QStringList &args) {
    return test_runner() ? test_runner()(args) : run_schtasks_process(args);
}

SchtasksRunner &test_elevated_runner() {
    static SchtasksRunner runner;
    return runner;
}

// Through UAC; an elevated process started this way gives no output. -1 when UAC was cancelled.
std::pair<int, QByteArray> run_schtasks_elevated_process(const QStringList &args) {
    if (!test_sandbox_dir().isEmpty()) return {-1, {}};
    if (args.size() != 1 || (args.first() != "/Create" && args.first() != "/Delete")) return {-1, {}};
    const std::wstring Program = own_exe().toStdWString();
    const std::wstring parameters = QString::fromLatin1(args.first() == "/Create" ? InstallArgument : RemoveArgument).toStdWString();
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    info.lpVerb = L"runas";
    info.lpFile = Program.c_str();
    info.lpParameters = parameters.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info) || !info.hProcess) return {-1, {}};
    DWORD code = 1;
    if (WaitForSingleObject(info.hProcess, 30000) == WAIT_OBJECT_0) GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    return {static_cast<int>(code), {}};
}

std::function<bool()> &test_self_launcher() {
    static std::function<bool()> launcher;
    return launcher;
}

// Through UAC, with the working directory the task would give the program.
bool run_self_elevated_process() {
    if (!qck::FeatureSettings::IsEnabled(qck::Feature::AdminStartup)) return false;
    if (!test_sandbox_dir().isEmpty()) return false;
    const std::wstring exe = own_exe().toStdWString();
    const std::wstring directory = dirname(own_exe()).toStdWString();
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.lpVerb = L"runas";
    info.lpFile = exe.c_str();
    info.lpDirectory = directory.c_str();
    info.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&info);
}

// Only a fixed operation crosses UAC; the elevated helper derives its definition.
std::pair<int, QByteArray> run_changing_schtasks(const QStringList &args) {
    if (!qck::FeatureSettings::IsEnabled(qck::Feature::AdminStartup)) return {-1, {}};
    if (args.size() != 1 || (args.first() != "/Create" && args.first() != "/Delete")) return {-1, {}};
    if (elevation_state() == ELEVATED) return WriteTask(args.first() == "/Create", build_task_xml(own_exe(), user_name()));
    return test_elevated_runner() ? test_elevated_runner()(args) : run_schtasks_elevated_process(args);
}
}

void set_schtasks_runner_for_test(SchtasksRunner runner) { test_runner() = std::move(runner); }
void SetTaskWriterForTest(TaskWriter Writer) { GetTaskWriter() = std::move(Writer); }
bool ValidateTask() { return WriteTask(true, build_task_xml(own_exe(), user_name()), true).first == 0; }

int RunTaskHelper(bool Install) {
    // Never trust a test environment override at this external entry point.
    HANDLE Token = nullptr;
    TOKEN_ELEVATION Elevated{};
    DWORD Length = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &Token)) return 1;
    const bool Allowed = GetTokenInformation(Token, TokenElevation, &Elevated, sizeof(Elevated), &Length) && Elevated.TokenIsElevated;
    CloseHandle(Token);
    if (!Allowed || !qck::FeatureSettings::IsEnabled(qck::Feature::AdminStartup) || !test_sandbox_dir().isEmpty()) return 1;
    return WriteTask(Install, build_task_xml(own_exe(), user_name())).first == 0 ? 0 : 1;
}

void set_elevated_schtasks_runner_for_test(SchtasksRunner runner) { test_elevated_runner() = std::move(runner); }

void set_environment_for_test(std::optional<QString> exe, std::optional<QString> state, std::optional<QString> user) {
    test_environment() = {std::move(exe), std::move(state), std::move(user)};
}

void set_self_launcher_for_test(std::function<bool()> launcher) { test_self_launcher() = std::move(launcher); }

QString elevation_state() {
    if (test_environment().state) return *test_environment().state;
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return STANDARD;
    QString state = STANDARD;
    TOKEN_ELEVATION_TYPE type{};
    DWORD size = 0;
    if (GetTokenInformation(token, TokenElevationType, &type, sizeof(type), &size)) {
        if (type == TokenElevationTypeFull) {
            state = ELEVATED;
        } else if (type == TokenElevationTypeLimited) {
            state = CAN_ELEVATE;
        } else {
            // An unsplit token: elevated when UAC is off for an administrator, else a standard user.
            TOKEN_ELEVATION elevation{};
            if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) && elevation.TokenIsElevated) state = ELEVATED;
        }
    }
    CloseHandle(token);
    return state;
}

QString task_name() {
    const QString user = user_name();
    return QString("QckSnip RunAsAdmin (%1)").arg(user.mid(user.lastIndexOf('\\') + 1));
}

QString build_task_xml(const QString &command, const QString &user) {
    return QString::fromUtf8(TASK_XML)
        .replace("{user}", escape(user))
        .replace("{command}", escape(command))
        .replace("{workdir}", escape(dirname(command)));
}

std::optional<QString> query_task_command() {
    const auto [code, out] = run_schtasks({"/Query", "/TN", task_name(), "/XML", "ONE"});
    if (code != 0) return std::nullopt;
    // /XML prints UTF-8 without a BOM under a declaration that still says UTF-16, so the bytes
    // are decoded first; a QString source makes the reader ignore the declared encoding.
    QXmlStreamReader reader(QString::fromUtf8(out));
    while (!reader.atEnd()) {
        if (reader.readNext() == QXmlStreamReader::StartElement && reader.name() == u"Command")
            return reader.readElementText().trimmed().remove('"');
    }
    if (reader.hasError()) log_exception(reader.errorString(), T("解析计划任务"));
    return QString();
}

bool points_to_self(const std::optional<QString> &command) {
    if (!command || command->isEmpty()) return false;
    const auto normalized = [](const QString &path) { return QDir::toNativeSeparators(QDir::cleanPath(path)).toLower(); };
    return normalized(*command) == normalized(own_exe());
}

bool install_task() {
    if (!qck::FeatureSettings::IsEnabled(qck::Feature::AdminStartup)) return false;
    const auto [code, out] = run_changing_schtasks({"/Create"});
    if (code != 0) {
        log_warning(T("创建管理员计划任务失败（退出码 {code}）：{output}", {{"code", code}, {"output", QString::fromLocal8Bit(out).trimmed()}}), "Elevation");
        return false;
    }
    log_info(T("已创建管理员计划任务"), "Elevation");
    return true;
}

bool remove_task() {
    if (!query_task_command()) return true;
    const auto [code, out] = run_changing_schtasks({"/Delete"});
    if (code == 0) {
        log_info(T("已删除管理员计划任务"), "Elevation");
        return true;
    }
    log_warning(T("删除管理员计划任务失败（退出码 {code}）：{output}", {{"code", code}, {"output", QString::fromLocal8Bit(out).trimmed()}}), "Elevation");
    return false;
}

bool run_task() {
    return qck::FeatureSettings::IsEnabled(qck::Feature::AdminStartup) && run_schtasks({"/Run", "/TN", task_name()}).first == 0;
}

bool task_is_stale(const std::optional<QString> &command) { return command && !command->isEmpty() && !points_to_self(command) && !QFileInfo::exists(*command); }

bool run_self_elevated() { return test_self_launcher() ? test_self_launcher()() : run_self_elevated_process(); }

bool repair_task() {
    if (elevation_state() != ELEVATED || !task_is_stale(query_task_command())) return false;
    log_info(T("管理员计划任务指向的程序已不存在，改为指向当前程序"), "Elevation");
    return install_task();
}

bool apply_startup(bool autostart, bool run_as_admin, bool was_run_as_admin, bool interactive) {
    const bool can_change = interactive || elevation_state() == ELEVATED;
    if (autostart && run_as_admin) {
        if (!can_change) return was_run_as_admin;
        // Already running this program: nothing to write, and so no UAC prompt either.
        if (was_run_as_admin && points_to_self(query_task_command())) {
            set_autostart(false);
            return true;
        }
        if (install_task()) {
            set_autostart(false);
            return true;
        }
        set_autostart(true);
        return false;
    }
    if (was_run_as_admin && can_change && !remove_task()) {
        set_autostart(false);
        return true;
    }
    set_autostart(autostart);
    return was_run_as_admin && !can_change;
}
}
