#include "configuration.h"

#if defined(HERMESX_GUARD_POWER_ANIMATIONS) && !MESHTASTIC_EXCLUDE_HERMESX

#include "modules/HermesXPowerGuard.h"

#ifndef DEBUG_BUTTONS
#define DEBUG_BUTTONS 0
#endif

#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_NRF52)
#include "sleep.h"
#endif

namespace HermesXPowerGuard
{

RTC_DATA_ATTR static uint32_t gGuardRtcFlags = 0;

static constexpr uint32_t FLAG_DFU_BYPASS = 0x01;

static bool gGuardEnabled = false;
static bool gStartupVisualsAllowed = true;
static bool gSuppressShutdownAnim = false;
static bool gPowerHoldReady = true;
static bool gRequireLongPress = false;
static bool gWaitingForPress = false;
static bool gUsbPresent = false;
static bool gBootHoldArmed = false;
static bool gQuietBoot = false;
static bool gDfuBypass = false;

void requestDfuBypassForNextBoot()
{
    gGuardRtcFlags |= FLAG_DFU_BYPASS;
}

static void clearDfuFlagIfSet()
{
    if (gGuardRtcFlags & FLAG_DFU_BYPASS) {
        gDfuBypass = true;
        gGuardRtcFlags &= ~FLAG_DFU_BYPASS;
    } else {
        gDfuBypass = false;
    }
}

void initialize(bool usbPresent, bool wokeFromTimer, bool wokeFromExt)
{
    (void)wokeFromTimer;

    gUsbPresent = usbPresent;
    gSuppressShutdownAnim = false;
    gBootHoldArmed = false;
    gWaitingForPress = false;
    gRequireLongPress = false;
    gQuietBoot = false;

    clearDfuFlagIfSet();

    gGuardEnabled = !gDfuBypass;
    gStartupVisualsAllowed = true;
    gPowerHoldReady = true;

    if (!gGuardEnabled)
        return;

#ifdef HERMESX_USB_CHARGING_QUIET_BOOT
    if (usbPresent) {
        gQuietBoot = true;
    }
#endif

    if (gQuietBoot || wokeFromExt) {
        gRequireLongPress = true;
        gWaitingForPress = true;
        gStartupVisualsAllowed = false;
        gPowerHoldReady = false;
    }

#if DEBUG_BUTTONS
    if (gQuietBoot || wokeFromExt) {
        LOG_DEBUG("BootHold: armed (usb=%d, wokeFromExt=%d)", usbPresent ? 1 : 0, wokeFromExt ? 1 : 0);
    }
#endif
}

bool guardEnabled()
{
    return gGuardEnabled;
}

bool dfuBypassActive()
{
    return gDfuBypass;
}

bool quietBootActive()
{
    return gQuietBoot;
}

bool startupVisualsAllowed()
{
    return !gGuardEnabled || gStartupVisualsAllowed;
}

void setStartupVisualsAllowed(bool allowed)
{
    if (!gGuardEnabled)
        return;
    gStartupVisualsAllowed = allowed;
}

bool registerInitialButtonState(bool pressed)
{
    if (!gGuardEnabled)
        return false;

    if (pressed) {
        gRequireLongPress = true;
        gBootHoldArmed = true;
        gWaitingForPress = false;
        gStartupVisualsAllowed = false;
        gPowerHoldReady = false;
#if DEBUG_BUTTONS
        LOG_DEBUG("BootHold: armed (button held)");
#endif
        return true;
    }

    if (gRequireLongPress) {
        gBootHoldArmed = true;
#if DEBUG_BUTTONS
        LOG_DEBUG("BootHold: armed (await press)");
#endif
        return true;
    }

    return false;
}

bool bootHoldArmed()
{
    return gGuardEnabled && gBootHoldArmed;
}

bool waitingForPress()
{
    return gGuardEnabled && gWaitingForPress;
}

void markPressDetected()
{
    if (!gGuardEnabled)
        return;
    gWaitingForPress = false;
}

void markBootHoldCommitted()
{
    if (!gGuardEnabled)
        return;
    gRequireLongPress = false;
    gBootHoldArmed = false;
    gWaitingForPress = false;
    gStartupVisualsAllowed = true;
    gPowerHoldReady = true;
#if DEBUG_BUTTONS
    LOG_DEBUG("BootHold: committed (ready)");
#endif
}

void markBootHoldAborted()
{
    if (!gGuardEnabled)
        return;
    gRequireLongPress = false;
    gBootHoldArmed = false;
    gWaitingForPress = false;
    gStartupVisualsAllowed = false;
    gPowerHoldReady = false;
    gSuppressShutdownAnim = true;
#if DEBUG_BUTTONS
    LOG_DEBUG("BootHold: aborted (sleep)");
#endif
}

void requestShutdownAnimationSuppression()
{
    if (!gGuardEnabled)
        return;
    gSuppressShutdownAnim = true;
}

bool consumeShutdownAnimationSuppression()
{
    if (!gGuardEnabled)
        return false;
    bool value = gSuppressShutdownAnim;
    gSuppressShutdownAnim = false;
    return value;
}

void setPowerHoldReady(bool ready)
{
    if (!gGuardEnabled)
        return;
    gPowerHoldReady = ready;
}

bool isPowerHoldReady()
{
    return !gGuardEnabled || gPowerHoldReady;
}

bool usbPresentAtBoot()
{
    return gUsbPresent;
}

void logBootHoldEvent(const char *event)
{
#if DEBUG_BUTTONS
    if (event)
        LOG_DEBUG("%s", event);
#else
    (void)event;
#endif
}

} // namespace HermesXPowerGuard

#endif // defined(HERMESX_GUARD_POWER_ANIMATIONS) && !MESHTASTIC_EXCLUDE_HERMESX
