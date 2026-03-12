# HermesX Docs Source Map

Use this file first to choose which references or original docs to open.

## Core docs in this skill

| Need | Open first |
| --- | --- |
| Acceptance criteria and constraints | `requirements-contract.md` |
| Emergency mode and 302.1 packet behavior | `emergency-3021-contract.md` |
| Module boundaries and ownership map | `architecture-map.md` |
| Release/status context | `status-and-changelog.md` |

## Original docs in repo

Open these under `../../../docs/` when exact text matters:

- `REF_prd.md`
- `REF_techspec.md`
- `REF_EmergencyMode.md`
- `REF_3021.md`
- `HermesX_EM_UI_v2.1.md`
- `CODEX_RULES.md`
- `REF_status.md`
- `CHANGELOG.md`
- `CHANGELOG_MINI.md`

## Fast routing hints

- Query mentions `SOS`, `SAFE`, `NEED`, `RESOURCE`, `HEARTBEAT`: open `emergency-3021-contract.md`.
- Query mentions `button`, `rotary`, `GPIO`, `power hold`, `screen toggle`: open `architecture-map.md`.
- Query mentions `ACK`, `NACK`, `LED`, `buzzer`: open both `requirements-contract.md` and `emergency-3021-contract.md`.
- Query mentions `version`, `release`, `changelog`: open `status-and-changelog.md`.
