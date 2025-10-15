# HermesX Emergency Mode Reference

## Activation Flow
- Rotary encoder button triple-press (`registerRawButtonPress()` in `HermesXInterfaceModule`) calls `onTripleClick()` and triggers `EmergencyAdaptiveModule::sendSOS()`.
- `EmergencyAdaptiveModule::setEmergencyActive()` now allows activation even when the stored mode is `OFF`, logs an override, and broadcasts SOS via `ReliableRouter`.
- `CannedMessageModule::setActiveList()` automatically switches UI focus to the emergency menu when the module is in emergency or drill mode, so the user sees SOS/SAFE options immediately.

## UI and Feedback
- LED/face animations follow the standard HermesX scheme: send (`SEND_R2L`), receive (`RECV_L2R`), ACK (`ACK_FLASH` with green face), NACK (`NACK_FLASH` with red face), and idle breathing.
- SOS activation plays the send tone, shows the SOS banner, and keeps retrying until an ACK is received.
- ACK/NACK events are handled centrally through `handleAckNotification()` which updates `waitingForAck`, schedules success animations, or triggers NACK feedback.

## Menu Layout
Current emergency menu entries (localized via `rebuildEmergencyMenu()`):
- SOS
- SAFE
- Need (configurable codes)
- Resource broadcast (configurable codes)

## Outstanding Work
- ReliableRouter queue monitoring (REF_status: "«Ý¿ì").
- Finalize GPIO5 to GPIO4 migration for RTC constraints (planned for Alpha 0.2.1).
- EM mode UI polish: confirm final LED color scheme and typography per REF_prd requirements.

## Design Supplement (from early spec)
- Single-color/OLED/E-Ink: highlight via inverse color; reduce redraw frequency for E-Ink.
- Idle red breathing; animations pause breathing; fade out to resume.
- EM screen shows fixed 4 options; font size fixed for clarity.

## References
- `src/modules/HermesXInterfaceModule.cpp`
- `src/modules/CannedMessageModule.cpp`
- `src/modules/EmergencyAdaptiveModule.cpp`
- Docs: `REF_prd.md`, `REF_status.md`, `REF_techspec.md`
