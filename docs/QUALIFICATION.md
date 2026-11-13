# Build and security qualification

This record distinguishes the initial foundation checks from the required PR
qualification. The exact validated head, run links and results are recorded in
[the PR](https://github.com/qckorg/qck-snip/pull/1) and its CI checks. A required
matrix describes the checks to execute; it does not itself assert a pass.

## Initial foundation evidence

Upstream baseline: `4c0d20f9cd7a5720df58ec8426aa4b6a65d54df7`.
The Qt-free policy library passed three local Release configurations: 58 assertions
each, 174 total, with zero failures or skipped CTests. A malformed CMake boolean
was rejected. PowerShell parsing, documentation links and diff checks also passed.

The initial local full-app configure lacked the Qt6 C++ SDK. The first hosted CI
run subsequently compiled the full application and passed Rust on x64 and ARM64,
but failed retained C++ tests, including stale fork-identity expectations and
assumptions that admin startup and updates were available. That run was not an
application pass. These results do not imply the security patches were qualified.

The security change also compiled the small Windows OCR host guard and native
test launcher with MSVC locally; the ordinary-host control returned NativeAllowed=1.
No sustained local application build or desktop tests were run.

## Required PR matrix

- **Policy:** default, capture and all-capability CMake presets, with Release CTest.
- **Full C++:** `scripts/coverage.ps1 -BuildOnly -PrivilegedHost`, then the full
  offscreen suite with four test workers. Optional service availability is enabled
  so retained enabled behavior remains exercised; production defaults remain off
  for administrator startup and updates.
- **Privileges:** the launcher runs ordinary tests with a restricted medium token
  when the CI host is elevated. A separately opted-in elevated-host probe checks
  prepopulated Snipping caches, direct construction and the PP-OCR C ABI before
  native loading. The probe never creates a scheduled task.
  Its controlled child processes also check medium-to-high identity queries,
  legitimate replacement, and rejection of a different same-name executable.
- **Administrator setup:** `scripts/smoke_admin_task.ps1` uses the real fixed helper
  on a disposable GitHub-hosted elevated runner. It registers, reads back and
  removes only its own previously absent task, checks command/account/settings,
  and cleans up on failure. Ordinary Qt tests retain the sandbox prohibition.
- **Desktop:** a 1920x1080 hosted display, serialized `_desktop`/`_windows_parity`
  suites, and mandatory failure status. The previous soft-failure allowance is
  removed. Existing assertions and the 77% total / 75% changed-line coverage gates
  remain enforced.
- **Capture:** `scripts/check_capture_profile.ps1` rebuilds the application, policy,
  service, core and updater test targets with all five capabilities unavailable.
  It checks configuration cannot enable them, no history database/monitor starts,
  no translation request reaches a loopback listener, and direct PNG clipboard
  publication still works. Core/updater cases assert refusal in this configuration;
  their original enabled cases run in the full integration configuration.
- **Rust:** retained workspace tests, third-party notices and independent updater
  builds on both x64 and ARM64. The C++ build also compiles the dynamic PP-OCR bridge.

The ordinary Compatible build runs its retained suite. Capture selects the policy,
service, core and updater contracts that apply when optional implementations are
unavailable; enabled feature behavior is qualified separately by the full matrix.
The elevated-host probe is explicit, so ordinary developer coverage needs no UAC.

## Interpretation and remaining limits

Merge requires green checks for the proposed head, not just policy success or an
older run. Record actual failures and prerequisite skips; never count unexecuted
or skipped cases as verified. Real Snipping package tests can lack the Microsoft
Store package on hosted Windows; stand-in DLL tests and the supplied PP-OCR pack
exercise the corresponding normal load/recognition paths.

OCR is intentionally unavailable in elevated/high-integrity or unqueryable hosts.
Using OCR with administrative capture would require a separate non-elevated OCR
worker. No elevated native-cache authenticity claim is made.

The real task smoke does not exercise a human UAC consent dialog; fake runners
cover consent cancellation and fallback, while the fixed elevated entry point is
exercised directly. Release signing, updater package publication/provenance,
external compiler certification, RAM/latency benchmarking and sanitizer campaigns
are separate qualification work. No release package is claimed qualified here.
