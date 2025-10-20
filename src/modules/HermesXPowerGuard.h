#pragma once

#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_HERMESX

#include <stdint.h>

namespace HermesXPowerGuard
{

#if defined(HERMESX_GUARD_POWER_ANIMATIONS)

void requestDfuBypassForNextBoot();
void initialize(bool usbPresent, bool wokeFromTimer, bool wokeFromExt);
bool guardEnabled();
bool dfuBypassActive();
bool quietBootActive();
bool startupVisualsAllowed();
void setStartupVisualsAllowed(bool allowed);
bool registerInitialButtonState(bool pressed);
bool bootHoldArmed();
bool waitingForPress();
void markPressDetected();
void markBootHoldCommitted();
void markBootHoldAborted();
void requestShutdownAnimationSuppression();
bool consumeShutdownAnimationSuppression();
void setPowerHoldReady(bool ready);
bool isPowerHoldReady();
bool usbPresentAtBoot();
void logBootHoldEvent(const char *event);

#else

inline void requestDfuBypassForNextBoot() {}
inline void initialize(bool, bool, bool) {}
inline bool guardEnabled() { return false; }
inline bool dfuBypassActive() { return false; }
inline bool quietBootActive() { return false; }
inline bool startupVisualsAllowed() { return true; }
inline void setStartupVisualsAllowed(bool) {}
inline bool registerInitialButtonState(bool) { return false; }
inline bool bootHoldArmed() { return false; }
inline bool waitingForPress() { return false; }
inline void markPressDetected() {}
inline void markBootHoldCommitted() {}
inline void markBootHoldAborted() {}
inline void requestShutdownAnimationSuppression() {}
inline bool consumeShutdownAnimationSuppression() { return false; }
inline void setPowerHoldReady(bool) {}
inline bool isPowerHoldReady() { return true; }
inline bool usbPresentAtBoot() { return false; }
inline void logBootHoldEvent(const char *) {}

#endif // defined(HERMESX_GUARD_POWER_ANIMATIONS)

} // namespace HermesXPowerGuard

#else

namespace HermesXPowerGuard
{
inline void requestDfuBypassForNextBoot() {}
inline void initialize(bool, bool, bool) {}
inline bool guardEnabled() { return false; }
inline bool dfuBypassActive() { return false; }
inline bool quietBootActive() { return false; }
inline bool startupVisualsAllowed() { return true; }
inline void setStartupVisualsAllowed(bool) {}
inline bool registerInitialButtonState(bool) { return false; }
inline bool bootHoldArmed() { return false; }
inline bool waitingForPress() { return false; }
inline void markPressDetected() {}
inline void markBootHoldCommitted() {}
inline void markBootHoldAborted() {}
inline void requestShutdownAnimationSuppression() {}
inline bool consumeShutdownAnimationSuppression() { return false; }
inline void setPowerHoldReady(bool) {}
inline bool isPowerHoldReady() { return true; }
inline bool usbPresentAtBoot() { return false; }
inline void logBootHoldEvent(const char *) {}
} // namespace HermesXPowerGuard

#endif

