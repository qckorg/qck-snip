#include "core/ocr_security.h"
#include <windows.h>
#include <vector>

namespace qck::Security {
bool CanLoadOcrRuntime() {
    HANDLE Token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &Token)) return false;
    TOKEN_ELEVATION Elevation{};
    DWORD Length = 0;
    bool Allowed = GetTokenInformation(Token, TokenElevation, &Elevation, sizeof(Elevation), &Length) && !Elevation.TokenIsElevated;
    GetTokenInformation(Token, TokenIntegrityLevel, nullptr, 0, &Length);
    std::vector<BYTE> Buffer(Length);
    if (!Length || !GetTokenInformation(Token, TokenIntegrityLevel, Buffer.data(), Length, &Length)) {
        Allowed = false;
    } else {
        const auto* Label = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(Buffer.data());
        const auto Count = *GetSidSubAuthorityCount(Label->Label.Sid);
        Allowed = Allowed && Count && *GetSidSubAuthority(Label->Label.Sid, Count - 1) <= SECURITY_MANDATORY_MEDIUM_RID;
    }
    CloseHandle(Token);
    return Allowed;
}
}
