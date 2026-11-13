#include "core/ocr_security.h"
#include <windows.h>
#include <cstdio>
#include <cstring>

int main(int Count, char** Arguments) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX | SEM_NOGPFAULTERRORBOX);
    if (Count == 2 && !std::strcmp(Arguments[1], "--input-diagnostic")) {
        wchar_t Actions[8]{}, Environment[32]{};
        if (GetEnvironmentVariableW(L"GITHUB_ACTIONS", Actions, 8) != 4 || lstrcmpW(Actions, L"true") ||
            GetEnvironmentVariableW(L"RUNNER_ENVIRONMENT", Environment, 32) != 13 || lstrcmpW(Environment, L"github-hosted")) return 2;
        DWORD ForegroundPid = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &ForegroundPid);
        HANDLE Process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, ForegroundPid), Token = nullptr;
        int ForegroundElevated = -1;
        TOKEN_ELEVATION Elevation{};
        DWORD Length = 0;
        if (Process && OpenProcessToken(Process, TOKEN_QUERY, &Token)) {
            if (GetTokenInformation(Token, TokenElevation, &Elevation, sizeof(Elevation), &Length)) ForegroundElevated = int(Elevation.TokenIsElevated);
            CloseHandle(Token);
        }
        if (Process) CloseHandle(Process);
        INPUT Event{};
        Event.type = INPUT_KEYBOARD;
        Event.ki.wVk = VK_F24;
        Event.ki.dwFlags = KEYEVENTF_KEYUP; // No-op release on the disposable runner only.
        const UINT Sent = SendInput(1, &Event, sizeof(Event));
        std::printf("[Test:Desktop] OrdinaryCaller=%d ForegroundPid=%lu ForegroundElevated=%d Sent=%u\n",
                    int(qck::Security::CanLoadOcrRuntime()), ForegroundPid, ForegroundElevated, Sent);
        return 0; // Diagnostic snapshot; the actual input suites remain the required checks.
    }
    if (Count != 2 || (std::strcmp(Arguments[1], "--allow") && std::strcmp(Arguments[1], "--deny"))) return 2;
    const bool Allowed = qck::Security::CanLoadOcrRuntime();
    std::printf("[Test:NativeHost] NativeAllowed=%d\n", int(Allowed));
    return Allowed == (std::strcmp(Arguments[1], "--allow") == 0) ? 0 : 1;
}
