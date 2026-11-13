# Working on qck-snip

Read `docs/INVARIANTS.md`, `docs/ARCHITECTURE.md` and `SECURITY.md` before changing
their boundaries. Preserve upstream copyright and third-party notices.

- Use PascalCase for new QCK-owned types, functions and variables. Preserve Qt
  overrides, ABI symbols and inherited APIs when a rename would break compatibility.
- Name lookup/create helpers `GetFolder()` or `GetObj()`, not `GetOrCreate...()`.
- Use `[System:SubSystem]` diagnostics. Never log captured content or credentials.
- Operational failures must not open foreground dialogs, steal focus or terminate
  unrelated applications. Explicit user-invoked settings dialogs are different.
- Keep implementations while adding feature boundaries. Flags are availability
  controls, not an authorization sandbox or a declaration of security.
- Resolve an immutable startup policy. UI checks never replace service enforcement.
- Test disabled behavior. Record unavailable qualification honestly; do not weaken
  or bypass inherited tests to get green.
- Do not rename the retained `jietuba` namespace, Rust crates or C ABI en masse.
