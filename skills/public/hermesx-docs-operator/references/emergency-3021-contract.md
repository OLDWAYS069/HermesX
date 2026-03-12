# Emergency 302.1 Contract

Sources:
- `../../../docs/REF_EmergencyMode.md`
- `../../../docs/REF_3021.md`
- `../../../docs/HermesX_EM_UI_v2.1.md`
- `../../../docs/REF_EMACT_lite.md`

## Entry and activation

- Enter EM UI on valid `@EmergencyActive` flow or local rotary triple-press.
- On local trigger, broadcast `@EmergencyActive`.
- If activation source is phone (`from==0`), enable EM transmit lock immediately.
- If activation source is node, send Emergency OK first, then enable lock after ACK.

## EM UI behavior contract

- Keep four action options mapped to packet payloads.
- Require rotary navigation arm window before press-to-send.
- Keep send cooldown and status-text transitions.
- Stop siren on first successful send attempt dispatch.

## Packet mapping contract

- Port: `PORTNUM_HERMESX_EMERGENCY` (`300`).
- Broadcast actions:
  - Trapped -> `STATUS: TRAPPED` with `want_ack=true`
  - Medical -> `NEED: MEDICAL` with `want_ack=true`
  - Supplies -> `NEED: SUPPLIES` with `want_ack=true`
  - Safe -> `STATUS: OK` with `want_ack=false`
- Text port is for control commands, not EM status packets.

## Auto SOS contract

- If EM active and not SAFE, send SOS after grace period and repeat by interval.
- SAFE must stop auto SOS.

## EM transmit lock contract

- Allow outbound emergency packets on port 300.
- Allow phone control commands:
  - `@EmergencyActive`
  - `@ResetLighthouse`
  - `@GoToSleep`
  - `@HiHermes`
  - `@Status`
