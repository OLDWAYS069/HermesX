#ifndef OLEDDISPLAYFONTSZH_h
#define OLEDDISPLAYFONTSZH_h

#ifdef ARDUINO
#include <Arduino.h>
#elif __MBED__
#define PROGMEM
#endif

#ifndef MESHTASTIC_EXCLUDE_HERMESX
#define MESHTASTIC_EXCLUDE_HERMESX 0
#endif

#ifndef HAS_SCREEN
#define HAS_SCREEN 0
#endif

#if !MESHTASTIC_EXCLUDE_HERMESX && HAS_SCREEN
extern const uint8_t HermesX_EM16_ZH[] PROGMEM;
#endif

#endif
