#include <windows.h>
#include <cstdio>

int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX | SEM_NOGPFAULTERRORBOX);
    std::fputs("[Test:Instance] Ready\n", stdout);
    std::fflush(stdout);
    Sleep(30000);
    return 0;
}
