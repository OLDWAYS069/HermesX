# HermesX Product Requirements (REF_prd.md)

## Acceptance Criteria
1. **Emergency 302.1 Flow**
   - Support SOS / SAFE / NEED / RESOURCE / STATUS / HEARTBEAT canned actions
   - Trigger @EmergencyActive and show the corresponding white banner
2. **Input Control**
   - GPIO43 rotary button short-press is reserved for canned message / EM UI send; it must not toggle the screen
   - GPIO43 rotary button long-press plays the HermesX power-hold animation and finally shuts down the device
   - Primary power button (BUTTON_PIN) long-press continues to drive the standard shutdown flow
3. **LED / Buzzer**
   - **Idle**: breathing animation
   - **SEND**: run from right to left with the faces [">_>", 0xFFFFFF, 160], [">.>", 0xFFFFFF, 160], [">o>", 0xFFFFFF, 160], [">.>", 0xFFFFFF, 160]
   - **RECEIVE**: run from left to right with "<_<", "<.<", "<o<", "<.<" using the same colour/timing scheme
   - **ACK**: green flash using "^_^" and "^o^" (220 ms)
   - **NACK**: red flash using ">_<" and "x_x" (260 ms)
   - **SOS**: priority animation and banner, then fade back to Idle
4. **Power Control**
   - While preparing for sleep, animate the LED progress bar and keep HermesX faces in sync
   - GPIO4 remains reserved for deep-sleep wake behaviour
5. **Reliable Messaging**
   - Preserve ACK / NACK handling; retries must surface through the HermesX callback
   - SOS messages do not retry once a NACK is received
6. **EM Mode + Canned Messages**
   - Provide canned messages such as "需要醫療" / "需要物資" / "我在這裡" while EM mode is enabled

## Constraints
- Do not modify the Meshtastic LoRa stack logic
- Keep BUTTON_PIN_ALT as an optional extension only; never replace BUTTON_PIN
- Avoid flow regressions outside the HermesX modules
