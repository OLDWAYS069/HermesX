#pragma once

#include "configuration.h"
#include <stdint.h>

namespace HermesXBatteryProtection
{

constexpr uint16_t kMinThresholdMv = 3000;
constexpr uint16_t kMaxThresholdMv = 3700;
constexpr uint16_t kDefaultThresholdMv = 3500;

/**
 * Returns whether over-discharge protection is enabled.
 * Default is true when no user preference file exists.
 */
bool isEnabled();

/**
 * Enables/disables over-discharge protection and persists the preference.
 */
void setEnabled(bool enabled);

/**
 * Returns the configured over-discharge threshold in millivolts.
 * Defaults to 3500mV when no user preference file exists.
 */
uint16_t getThresholdMv();

/**
 * Persists a new over-discharge threshold in millivolts.
 * The value is clamped to the supported 3000mV-3700mV range.
 */
void setThresholdMv(uint16_t thresholdMv);

} // namespace HermesXBatteryProtection
