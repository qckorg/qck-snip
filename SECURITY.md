# Security review policy

## System and scope

qck-snip is a Windows capture/editor application with retained OCR, translation,
recording, pinning, history and update code. Review product source, native bridges,
configuration, build acquisition, CI and release paths. Maintainers: QckOrg.
This is review guidance, not evidence that a control is implemented.

## Threat boundaries and invariants

Untrusted sources include desktop clipboard/media, imports, configured provider
responses, dependency artifacts and lower-integrity local files. Protected assets
include captured content, provider credentials, user data, executable integrity and
conditional administrator/release authority.

Apply [engineering invariants](docs/INVARIANTS.md) and the actual boundaries in
[the security brief](docs/SECURITY-BRIEF.md). In particular:
- Feature-disabled services must not initialize sensitive work. Settings/UI cannot
  reenable a build-unavailable capability; policy is distinct from readiness.
- No unexpected capture/clipboard transmission or diagnostic retention.
- No privileged trust based solely on editable PID files, temporary XML or cached
  native-file existence. Validate identity and preserve it through consumption.
- Imported formats/paths and native dimensions/lengths require bounded validation.
- Operational failures must recover without focus stealing or unrelated termination.

## Reporting and known limitations

Report concrete source-to-sink failures with realistic attacker authority,
prerequisites, counterevidence and impact. Treat same-user behavior as added impact
only when it crosses a meaningful boundary, such as higher integrity. Explicitly
state optional elevation and consent requirements. Do not equate a dangerous API,
mutable upstream reference or user-selected endpoint with a demonstrated exploit.

The brief records three unresolved inherited administrator-related findings.
Default-off gates mitigate routes; they do not close those findings. External
compiler/release provenance, runtime ACLs and unexecuted tests are unknowns.
No owner-approved suppressions, exclusions or accepted-risk decisions have been
established. Contact maintainers privately for sensitive reports; no private
reporting channel is configured here, so do not publish secrets or weaponized inputs.
