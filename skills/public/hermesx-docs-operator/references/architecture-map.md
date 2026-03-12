# Architecture Map

Sources:
- `../../../docs/REF_techspec.md`
- `../../../docs/CODEX_RULES.md`
- `../../../docs/ISSUE_powerhold_longpress.md`
- `../../../docs/ISSUE_boot_node_id_alignment.md`

## Ownership and interfaces

- HermesX modules should keep `HermesX` naming prefix.
- Keep HermesX interface APIs stable, including power-hold related hooks.
- Use hook-based extension points for reliability behavior where possible.

## Input and power behavior split

- Primary power button:
  - Short press: page/screen path via power FSM.
  - Long press: power-hold animation then shutdown flow.
- Rotary button:
  - Short press: input broker/canned submit behavior only.
  - Long press: same hold animation and shutdown behavior path.
- Screen toggle ownership stays in power FSM, not rotary handlers.

## Modules and likely touch points

- `src/modules/HermesEmUiModule.cpp`: EM UI, action send, status update.
- `src/modules/LighthouseModule.cpp`: emergency activation and auto SOS policy.
- `src/mesh/MeshService.cpp`: transmit lock and send gating logic.
- `src/input/*` or button thread files: press routing behavior.

## Restricted or high-risk zones

- `src/mesh/` and sleep core paths are high-risk unless request explicitly demands changes.
- Proto schema and generated files should remain untouched unless protocol update is requested.

## Troubleshooting cues from issue docs

- Power-hold failures can come from button polarity, wake path, or init timing.
- Boot-screen alignment issues are usually UI layout-level, not protocol-level.
