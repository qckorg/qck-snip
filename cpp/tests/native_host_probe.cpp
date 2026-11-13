#include "core/ocr_security.h"
#include <windows.h>
#include <cstdio>
#include <cstring>

int main(int Count, char** Arguments) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX | SEM_NOGPFAULTERRORBOX);
    if (Count != 2 || (std::strcmp(Arguments[1], "--allow") && std::strcmp(Arguments[1], "--deny"))) return 2;
    const bool Allowed = qck::Security::CanLoadOcrRuntime();
    std::printf("[Test:NativeHost] NativeAllowed=%d\n", int(Allowed));
    return Allowed == (std::strcmp(Arguments[1], "--allow") == 0) ? 0 : 1;
}
