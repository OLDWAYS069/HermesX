# HermesX Technical Specification (REF_techspec.md)

## Naming & Structure
- Prefix all HermesX classes, helpers, and logs with HermesX
- Public API for HermesX Interface: startPowerHoldAnimation, updatePowerHoldAnimation, stopPowerHoldAnimation, egisterRawButtonPress
- BUTTON_PIN is the primary power key; BUTTON_PIN_ALT is optional and used only for EXT1 wake/support wiring

## Input Agent (ButtonThread, RotaryEncoder)
- Primary button short-press delegates to switchPage() and wakes the screen via PowerFSM
- Primary button long-press feeds updatePowerHoldAnimation() until HermesX finishes the power-hold flow
- Rotary encoder button (BUTTON_PIN_ALT) short-press emits InputBroker events only (canned message submit). No screen toggling
- Rotary encoder button long-press shares the same hold animation path and shutdown behaviour as the primary button

## HermesX Interface Module
- egisterRawButtonPress() collects multi-press and hold timing, invoking EM UI callbacks (double, triple press) and starting the power-hold animation
- Loopback packets (loopbackOk = true) ensure SEND feedback triggers immediately
- ACK/NACK feedback is handled in handleReceived() via reliable router callbacks

## Power & Screen Integration
- PowerFSM screenPress continues to call screen->onPress() to advance frames without affecting rotary behaviour
- Sleep entry calls startPowerHoldAnimation() then stopPowerHoldAnimation() based on hold completion
- Screen toggling is handled exclusively by PowerFSM; ButtonThread no longer flips screen_flag from rotary input

## LED / UI Contract
- Idle breathing animation remains default when no events are active
- SEND/RECV/ACK/NACK/SOS sequences follow the face tables defined in REF_prd.md
- During sleep preparation, LED progress bar runs while HermesX faces stay synchronised

## Messaging & Reliability
- ACK/NACK events are propagated through hermesXCallback
- SOS messages do not retry after a NACK; other messages still follow the reliable-router retry policy

## Platform/Build Flags
- GPIO43 maps to BUTTON_PIN for Heltec Wireless Tracker builds
- BUTTON_PIN_ALT remains unassigned by default in firmware; use environment uild_flags only for extension boards
- Keep -DMESHTASTIC_EXCLUDE_* flags aligned with project defaults to avoid side effects outside HermesX modules
