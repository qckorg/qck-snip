// ctest starts every test through this. Windows' critical-error dialogs are switched off first, so a
// DLL missing from the test or from anything it starts ends that process with 0xC0000135 instead of
// a system dialog that stays on the user's screen after the test is gone. Children inherit the mode.
// No Qt: it must start even when the Qt DLLs are what is missing.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>

namespace {
// The command line after this program's own path, with the test's quoting intact.
wchar_t *test_command_line() {
    wchar_t *line = GetCommandLineW();
    if (*line == L'"') {
        ++line;
        while (*line && *line != L'"') ++line;
        if (*line) ++line;
    } else {
        while (*line && *line != L' ' && *line != L'\t') ++line;
    }
    while (*line == L' ' || *line == L'\t') ++line;
    return line;
}
}

int wmain() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    wchar_t *command = test_command_line();
    if (!*command) return 2;
    // ctest ends a timed-out test by killing this process; the job takes the test down with it.
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    HANDLE Token = nullptr, Restricted = nullptr;
    TOKEN_ELEVATION Elevated{};
    DWORD Length = 0;
    wchar_t KeepToken[2]{};
    const bool Preserve = GetEnvironmentVariableW(L"QCK_TEST_KEEP_TOKEN", KeepToken, 2) == 1 && KeepToken[0] == L'1';
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY, &Token)) return 4;
    if (!GetTokenInformation(Token, TokenElevation, &Elevated, sizeof(Elevated), &Length)) { CloseHandle(Token); return 4; }
    BOOL Started = FALSE;
    if (Elevated.TokenIsElevated && !Preserve) {
        // Exercise normal desktop behavior. Lowering integrity alone retains admin authority.
        if (!CreateRestrictedToken(Token, DISABLE_MAX_PRIVILEGE | LUA_TOKEN, 0, nullptr, 0, nullptr, 0, nullptr, &Restricted)) { CloseHandle(Token); return 4; }
        BYTE Sid[SECURITY_MAX_SID_SIZE]{};
        DWORD SidSize = sizeof(Sid);
        TOKEN_MANDATORY_LABEL Label{};
        if (!CreateWellKnownSid(WinMediumLabelSid, nullptr, Sid, &SidSize)) { CloseHandle(Restricted); CloseHandle(Token); return 4; }
        Label.Label.Sid = Sid;
        Label.Label.Attributes = SE_GROUP_INTEGRITY;
        if (!SetTokenInformation(Restricted, TokenIntegrityLevel, &Label, sizeof(Label) + SidSize)) { CloseHandle(Restricted); CloseHandle(Token); return 4; }
        Started = CreateProcessAsUserW(Restricted, nullptr, command, nullptr, nullptr, TRUE, CREATE_SUSPENDED, nullptr, nullptr, &startup, &process);
        CloseHandle(Restricted);
    } else {
        Started = CreateProcessW(nullptr, command, nullptr, nullptr, TRUE, CREATE_SUSPENDED, nullptr, nullptr, &startup, &process);
    }
    const DWORD StartError = Started ? ERROR_SUCCESS : GetLastError();
    CloseHandle(Token);
    if (!Started) { std::fprintf(stderr, "[Test:Host] Process creation failed: %lu\n", StartError); return 3; }
    if (job) AssignProcessToJobObject(job, process.hProcess);
    ResumeThread(process.hThread);
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    return static_cast<int>(code);
}
