# Security brief

## Basis and limits

The initial offline review used upstream commit
`4c0d20f9cd7a5720df58ec8426aa4b6a65d54df7`: 70 files fully reviewed, plus
inventory/search across 990 files. No evident deliberate backdoor or bundled
compiler replacement was identified in reviewed paths. This is not certification:
external MSVC/Rust/Qt binaries, transitive dependencies, opaque models and release
binaries were not independently authenticated. No exploits were executed.

## Actual boundaries

- Screen capture, annotation, clipboard history, OCR and recording handle
  sensitive desktop content. History is persistent SQLite/image data; direct
  clipboard publication is separate. OCR may load native code into the app.
- Translation sends chosen text and configured credentials to a selected provider.
  Custom compatible endpoints are intentional recipients, not trusted tool agents.
- Qt/UI calls native Win32 and statically linked Rust adapters. Imported files,
  shared clipboard formats and network responses are untrusted input.
- Optional administrator startup crosses an integrity boundary. Standard-user
  files do not become trusted because a process reading them is elevated.
- Model/runtime downloads check catalog hashes. Application updates trust GitHub
  HTTPS metadata and use local hashes for staging/backup consistency; those local
  hashes are not independent publisher authentication.
- Build configuration executes fetched CMake/dependency code. No bundled compiler
  was found. Installed toolchains, CI hosts and downloaded SDK tools remain external
  trust anchors; source stars do not establish their integrity.

## Inherited findings

These remain source-level findings, not demonstrated host compromises.

| Finding | Impact and prerequisites | Foundation status |
|---|---|---|
| OCR cache accepted on file presence before native loading | Same-user code could prepopulate a cache later loaded by elevated OCR | Not fixed. Admin startup default-disabled; manual elevation still exposes the condition |
| Editable instance record treated as identity | Elevated startup could terminate an unrelated process | Not fixed. Replace with secured single-instance IPC/retained verified handles |
| Closed user-temp task XML passed across UAC | Same-user code could substitute an elevated scheduled command during legitimate consent | Not fixed. Default admin-startup gate blocks the normal setup route |

The first two have high confidence in source dataflow; the XML finding has medium
confidence because timing and actual temporary-file ACLs were not exercised.
All were calibrated Medium overall due to local attacker and conditional elevation
prerequisites. No unauthenticated remote execution path was established.

## Required properties

Captured content must not be unexpectedly transmitted or logged. Disabled services
must not perform their associated sensitive operation. Native code and privileged
commands need authenticated, protected inputs. Failures must be observable without
modal interruptions or secrets. Imports need validation and explicit recipients.
Settings cannot override build-unavailable capabilities.

## Supply-chain follow-up

The ZXing version tag is replaced by its recorded immutable commit in this fork.
Other hardening remains: independently pin Qt SHA-256/signatures rather than
same-origin sidecars/static MD5; pin remaining tool/action versions; require and
validate release signing/provenance. The copied upstream release workflows still
describe upstream packaging/signing contracts. They are not a QCK release pipeline.

Do not run the whole app as administrator to improve capture compatibility.
Keep a future privileged helper narrow and keep OCR/user plugins unelevated.
No known risk is declared accepted or excluded from future review by this brief.
