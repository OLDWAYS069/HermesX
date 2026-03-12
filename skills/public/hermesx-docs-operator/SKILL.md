---
name: hermesx-docs-operator
description: Convert HermesX repo docs into implementation-ready guidance and review criteria. Use when tasks reference docs files, Emergency Mode or 302.1 behavior, EM UI, rotary or power-button input rules, LED ACK/NACK contracts, or when code changes must align with HermesX requirements and changelog conventions.
---

# HermesX Docs Operator

## Workflow

1. Parse the request and extract behavior keywords.
2. Run `scripts/select_docs.sh "<request>"` to shortlist relevant source docs.
3. Read the minimum required files in `references/` first.
4. Read original docs in `../../../docs/` only when details are missing or conflicting.
5. Build an implementation contract:
   - Required behavior
   - Forbidden changes
   - Likely target files
6. Implement or review with explicit traceability to source docs.
7. Summarize changes with a doc trace.

## Guardrails

- Keep 302.1 packet text and intent stable unless explicitly requested.
- Keep HermesX naming and public API stability.
- Respect boundaries called out in `docs/CODEX_RULES.md`, especially Meshtastic core areas.
- Treat `docs/REF_*.md` as append-first documents; avoid deleting key sections.

## Reference Routing

- Start with `references/source-map.md` to choose files quickly.
- Use `references/requirements-contract.md` for acceptance criteria and constraints.
- Use `references/emergency-3021-contract.md` for EM flow, packet mapping, and lock behavior.
- Use `references/architecture-map.md` for module ownership and input/power integration.
- Use `references/status-and-changelog.md` for release-note and status alignment.

## Output Pattern

- Produce a short doc trace mapping each major claim/change to a source file.
- State assumptions when docs conflict or appear stale.
- Prefer dated changelog entries when behavior differs across references.
