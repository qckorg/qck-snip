# qck-snip

A QckOrg fork of [Jietuba](https://github.com/1003129155/jietuba), starting at
`4c0d20f9cd7a5720df58ec8426aa4b6a65d54df7`. We are separating policy, platform
services and presentation incrementally. Existing feature implementations remain.

This foundation is source work, not a qualified desktop release. Capture,
annotation, scrolling, pinning and recording remain in the legacy application.
OCR, translation and clipboard history remain enabled by default. Administrator
startup and application updates require explicit build opt-in. Fork release
packaging remains unqualified. OCR runtimes require a non-elevated process,
independently of feature flags; admin mode therefore has no OCR.

- [Engineering invariants](docs/INVARIANTS.md)
- [Architecture and migration sequence](docs/ARCHITECTURE.md)
- [Feature policy](docs/FEATURE-FLAGS.md)
- [Security brief](docs/SECURITY-BRIEF.md) and [review policy](SECURITY.md)
- [Qualification record](docs/QUALIFICATION.md)
- [Preserved upstream instructions](README.upstream.md), [MIT license](LICENSE)
  and [third-party notices](cpp/THIRD-PARTY-NOTICES.md)

## Build policy tests without Qt or Rust

With CMake 3.24+ and a C++20 compiler, from `cpp/`:

```powershell
cmake --preset policy
cmake --build build/qck-policy --config Release --parallel 2
ctest --test-dir build/qck-policy -C Release --output-on-failure
```

Repeat with `policy-capture` and `policy-all` to qualify different availability
masks. These are policy tests, not application or desktop tests.

## Build the retained application

The upstream Qt/MSVC/Rust prerequisites still apply. From `cpp/`:

```powershell
.\scripts\build.ps1 -FeatureProfile Compatible
# Disable OCR, translation and history, retaining their implementations:
.\scripts\build.ps1 -FeatureProfile Capture
```

The output is `qck-snip.exe` (`qck-snip-full.exe` for the Full variant).
Disabled services are guarded; their dependencies are not yet stripped and no RAM
reduction is claimed. See the qualification record before running a build.
Compatible runs the retained suite; Capture runs policy/service/instance/updater
contracts for unavailable capabilities. CI separately exercises enabled services,
the real task helper, mandatory desktop tests and the Capture build.

## Separate fork data

Default settings: `%APPDATA%\QckSnip\settings.ini`; logs/runtime caches:
`%LOCALAPPDATA%\QckSnip`; history:
`%APPDATA%\QckSnip\clipboard\clipboard.db`. Machine-local settings use
`HKCU\Software\QckOrg\QckSnip\ToolSettings`.

Upstream state is not automatically imported. Use explicit settings import if
desired; review imported paths, provider endpoints and credentials first.
Portable mode still selects an existing `settings.ini` beside the executable.
Do not place the fork beside an upstream portable configuration unless sharing
it is intentional. There is no automatic history or credential migration.
