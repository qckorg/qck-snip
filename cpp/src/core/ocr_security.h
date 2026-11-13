#pragma once

namespace qck::Security {
// Unconditional boundary: writable OCR runtimes must never inherit elevated authority.
bool CanLoadOcrRuntime();
}
