# Requirements Contract

Sources:
- `../../../docs/REF_prd.md`
- `../../../docs/CODEX_RULES.md`

## Acceptance criteria to preserve

- Keep Emergency 302.1 canned actions and active-banner behavior.
- Reserve rotary short press for canned/EM submit only.
- Keep rotary long press aligned with HermesX power-hold shutdown flow.
- Keep primary power button long-press behavior intact.
- Preserve LED behavior contract: idle breathing plus SEND/RECV/ACK/NACK/SOS sequences.
- Keep reliable ACK/NACK flow via HermesX callback path.
- Stop SOS retry after NACK for SOS-specific behavior.

## Constraints to enforce during implementation

- Do not rewrite Meshtastic LoRa stack logic.
- Keep `BUTTON_PIN` as primary power key.
- Keep `BUTTON_PIN_ALT` optional only.
- Avoid regressions outside HermesX modules.
- Keep HermesX naming prefix on classes/helpers/logs.
- Avoid changing files listed as restricted in `CODEX_RULES.md` unless explicitly requested.

## Review checklist

- Does the patch preserve rotary and power-button role split?
- Do ACK/NACK paths remain observable to HermesX feedback handlers?
- Are SOS retry rules unchanged?
- Are restricted paths untouched or explicitly justified?
