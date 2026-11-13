# Architecture-foundation qualification

Baseline: upstream `4c0d20f9cd7a5720df58ec8426aa4b6a65d54df7`.
This record covers the initial QCK policy/source foundation, not a desktop release.
No upstream application or dependency implementation was removed.

## Executed locally

Toolchain: installed Visual Studio 18 / MSVC 19.50.35728.0, Windows SDK 10.0.26100.0.
Only the tiny Qt-free policy library and check executable were compiled locally,
with parallelism 2. No sustained full native build was started.

| Check | Pass | Fail | Scope |
|---|---:|---:|---|
| Policy CTest, default availability | 1 | 0 | 58 internal assertions |
| Policy CTest, OCR/translation/history unavailable | 1 | 0 | 58 internal assertions |
| Policy CTest, all capabilities available | 1 | 0 | 58 internal assertions |
| Invalid CMake boolean rejected | 1 | 0 | Configure must fail for `QCK_ENABLE_OCR=garbage` |

The three policy configurations total 174 passing assertions, zero failed
assertions and zero skipped CTests. Coverage includes default configuration,
stable IDs, unknown/malformed switches, noninterference, dependency combinations,
invalid enum handling and settings being unable to override build availability.
These counts do not include unexecuted application tests.

PowerShell build-wrapper syntax parsing, `git diff --check`, local documentation
link validation and the SECURITY.md resolver are additional static checks.
The root policy resolves for `cpp/src/core`; there are no nested policy overrides.

## Blocked and unverified

A network-disconnected full-app configure probe stopped at `find_package(Qt6)`:
no Qt6 C++ SDK/configuration was available to that build. It did not fetch or
execute third-party build configuration. This is a blocked prerequisite, not a
passing application build. The remote worker was reachable, but a sustained
build/toolchain installation was not started.

Not run: full application compilation, retained upstream suites, Qt QSettings
round trips/import/export, service initialization/worker side-effect tests,
desktop selection/copy, shutdown, administrator behavior, updater packaging,
Rust updater tests, sanitizer/static-analysis suites or memory benchmarks.
No application was launched and no user clipboard, settings or scheduled tasks
were modified during qualification.

Service gates, cache isolation and UI wiring received source review only. We do
not claim that pure policy tests verify their runtime behavior. Inherited tests
still contain upstream product/data-path assumptions and require explicit fork
compatibility updates; do not skip or loosen them to declare an application pass.
The inherited privileged vulnerabilities remain open; default-disabled admin
startup is mitigation, not proof of a fix.

## Next qualification gate

Provide an MSVC-compatible Qt SDK and Rust on the bounded remote worker, capture
the retained application's baseline, then qualify the Compatible and Capture
profiles with Qt integration tests before merging/releasing. Specifically verify
no OCR DLL loading, translation HTTP request or history database/monitor startup
when disabled; check malformed INI behavior, settings import persistence, direct
copy independence, restart/shutdown and product-data isolation.

Fork CI adds a dependency-free policy matrix. Existing upstream workflows and
tests remain present; their release/package identity contracts have not yet been
ported or qualified for QCK publication.
