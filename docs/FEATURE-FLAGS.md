# Feature policy

## Three different states

1. **Build availability:** CMake `QCK_ENABLE_*` values become the generated
   `qck/FeatureConfig.h` used by the policy target.
2. **Configured enablement:** known keys under the `[features]` INI section,
   loaded once before workers start. Missing keys retain defaults. These keys
   participate in explicit settings import/export; normal saves leave them intact.
3. **Runtime readiness:** owned by the existing service. Enabled OCR may lack a
   model; enabled translation may lack credentials; updates may have no helper.

Effective enablement is availability AND configuration. It is not a sandbox.
Restart after changing feature configuration. There is no mutable live-reload
state or remote flag service.

| Stable key | CMake option | Default availability | Enforced boundary |
|---|---|---|---|
| `ocr.enabled` | `QCK_ENABLE_OCR` | ON | Preload/seeding, manager initialization/recognition, package lookup/cache preparation |
| `translation.enabled` | `QCK_ENABLE_TRANSLATION` | ON | Hotkey/tray, manager UI/workers, provider HTTP before request handles |
| `clipboard.history` | `QCK_ENABLE_CLIPBOARD_HISTORY` | ON | Hotkey/tray, database opening/reset, monitoring, preload |
| `admin.startup` | `QCK_ENABLE_ADMIN_STARTUP` | OFF | Startup handoff, task creation/mutation/run, UAC self-launch, settings card |
| `updates.enabled` | `QCK_ENABLE_UPDATES` | OFF | Updater helper availability and extraction/execution |

Configured defaults are true; compile availability therefore controls the defaults
above. Existing per-feature settings still apply. For example history monitoring
also needs the existing clipboard-enabled preference; admin startup also needs the
existing local administrator-startup preference and OS authorization.

`FeaturePolicy::CanTranslateImage()` explicitly requires OCR AND translation.
Text translation is independent of OCR. History is independent of direct copy.
Other features remain unflagged until their service boundaries are extracted.

## Runtime configuration

In the fork's selected `settings.ini`:

```ini
[features]
ocr.enabled=false
translation.enabled=false
clipboard.history=false
```

Boolean values are trimmed, case-insensitive `true`/`false` or `1`/`0`.
Other values disable that capability and log a generic diagnostic after logger
setup. Unknown keys are ignored and diagnosed; they never create a capability.
An INI access/format error disables every optional capability in this snapshot.
Do not log arbitrary keys or values.

## Build profiles

`scripts/build.ps1 -FeatureProfile Compatible` preserves OCR/translation/history.
`-FeatureProfile Capture` makes those three unavailable. Both retain the code;
neither claims a smaller binary or lower measured memory. Admin startup stays OFF
in the build wrapper. Expert direct CMake builds can enable it. A separate fixed
security gate prevents both Snipping and PP-OCR native runtime loading in an
elevated/high-integrity or unqueryable host. Settings and build flags cannot
override it; elevated capture has no OCR until a separate standard-user worker is added.

`-Updates` explicitly enables both `QCK_ENABLE_UPDATES` and legacy
`JIETUBA_UPDATES` helper bundling. Static builds no longer implicitly bundle an
updater. The fork's release packaging/provenance is not qualified; do not publish
an update-enabled release yet. The Rust updater now targets QckOrg, not upstream.

The `policy`, `policy-capture`, and `policy-all` presets are policy-only
qualification configurations. They do not build the desktop app. The all preset
cannot launch privileged code because it builds only the pure policy test.

## Remaining work

Gate all legacy UI routes and worker entry points with integration-tested service
adapters, add a settings UI for feature policy, then split optional link targets and
Cargo dependencies. Do not advertise a new flag as operational until initialization
and sensitive execution are enforced and tested.
