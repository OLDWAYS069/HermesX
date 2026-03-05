#pragma once

#include "configuration.h"

namespace HermesXBatteryProtection
{

/**
 * Returns whether over-discharge protection is enabled.
 * Default is true when no user preference file exists.
 */
bool isEnabled();

/**
 * Enables/disables over-discharge protection and persists the preference.
 */
void setEnabled(bool enabled);

} // namespace HermesXBatteryProtection

