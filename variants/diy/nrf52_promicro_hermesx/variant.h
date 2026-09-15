#pragma once

// Reuse the proven ProMicro pin map, but constrain HermesX to the SX126x
// radios used by the TCXO target. This keeps unrelated radio families out of
// the 1 MB nRF52840 image without changing the stock DIY environments.
#include "../nrf52_promicro_diy_tcxo/variant.h"

// Let Meshtastic use the same AutoOLEDWire probing and page-mode display path
// as the proven stock nrf52_promicro_diy_tcxo firmware. The fitted 2.42-inch
// SSD1309 works on that target without any ACK solder modification.

// HermesOne rotary controls fitted to the user's soldered fakeTec v4 carrier.
// P0.17/D2 remains reserved for the radio RXEN line.
#define PIN_BUZZER (0 + 24) // D5 / P0.24, passive buzzer positive
#define HERMESX_NRF52840_SIMPLE_BOOT 1
#define HERMESX_ROTARY_PIN_A (0 + 20)     // D3 / P0.20
#define HERMESX_ROTARY_PIN_B (0 + 22)     // D4 / P0.22
#define HERMESX_ROTARY_PIN_PRESS (32 + 0) // D6 / P1.00; also the hold/sleep/wake button
#define HERMESX_LED_PIN (0 + 6)            // board 006 / P0.06, WS2812B DATA
#define HERMESX_LED_COUNT 8

// This assembled fakeTec v4 measures 3.13 V in firmware for a verified
// 3.91 V cell when using the theoretical 2.0 equal-divider multiplier.
// Calibrate for the fitted divider/ADC loading: 2.0 * 3.91 / 3.13 = 2.498.
// Keep this board-specific value out of the inherited generic ProMicro target.
#undef ADC_MULTIPLIER
#define ADC_MULTIPLIER 2.50F

// D3/D4/D6 are the rotary inputs; D5 remains the buzzer.
// P1.06/P0.08 are the GPS UART.
#undef PIN_GPS_TX
#undef PIN_GPS_RX
#undef PIN_GPS_EN
#undef GPS_POWER_TOGGLE
#undef PIN_SERIAL1_RX
#undef PIN_SERIAL1_TX
#define PIN_GPS_TX (32 + 6) // GPS module TX -> MCU RX, D10 / P1.06
#define PIN_GPS_RX (0 + 8) // GPS module RX -> MCU TX
#define PIN_SERIAL1_RX PIN_GPS_TX
#define PIN_SERIAL1_TX PIN_GPS_RX

#undef USE_LLCC68
#undef USE_RF95
#undef USE_LR1121
