# Engineering invariants

These are requirements for new and migrated code. They do not declare every
inherited implementation compliant. Gaps are tracked in the
[security brief](SECURITY-BRIEF.md) and [qualification record](QUALIFICATION.md).

1. **Policy precedes side effects.** Resolve configuration on the bootstrap
   thread and publish an immutable snapshot. Check it before initialization and
   sensitive execution. Settings, imports and saved startup preferences cannot
   reenable a build-unavailable capability.
2. **Separate policy, readiness and UI.** Policy allows initialization; the
   service owns readiness and cleanup. A visible button, existing DLL or true
   setting proves neither readiness nor authorization. Name dependency queries.
3. **Own lifetimes.** Every worker, buffer, native handle and callback has an owner,
   cancellation path and bounded shutdown. No callback may access destroyed UI.
   Measure actual allocations and latency; working-set trimming is not proof of
   memory efficiency.
4. **Recover quietly.** Clear busy state after failure, retain actionable
   diagnostics and permit the next operation. Bound retries. Background errors
   must not steal focus, show modal error dialogs or kill unrelated applications.
5. **Keep data local by default.** Captures and clipboard content leave the
   computer only through an explicit selected operation/provider. Disabled history
   must not open its database or monitor the clipboard. Direct image copy remains
   independent. Do not retain sensitive content for diagnostics.
6. **Preserve authority.** Editable PID records, temporary XML and user-cache DLLs
   do not authenticate privileged identity or code. No silent elevation; no
   lower-trust native code in a higher-trust process. Flags are not privilege separation.
7. **Preserve compatibility deliberately.** Retain upstream implementations and
   interfaces until replacements are qualified. QCK state is separate by default;
   imports are explicit. Schema/path changes need migration and rollback notes,
   including effects on credentials and machine-local settings.
8. **Verify claims.** Test enabled/disabled paths, malformed configuration and
   dependency combinations. State executed and unavailable scope. Policy tests do
   not qualify the desktop app; mitigations do not prove vulnerabilities fixed;
   source review does not certify external dependencies or release binaries.

New QCK code uses PascalCase, `Get...()` lookup/create helpers and
`[System:SubSystem]` logs. Framework overrides, ABI symbols and inherited APIs
keep required names. Avoid unrelated naming churn during extraction.
