#include "modules/HermesXPowerGuard.h"

#if defined(HERMESX_GUARD_POWER_ANIMATIONS) && !MESHTASTIC_EXCLUDE_HERMESX

#include "configuration.h"

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
static bool gBootHoldPending = false;
static bool gRequireLongPress = false;
static bool gWaitingForPress = false;
static bool gUsbPresent = false;
static bool gBootHoldArmed = false;
static bool gQuietBoot = false;
static bool gDfuBypass = false;
static bool gWokeFromSleep = false;

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
    gWokeFromSleep = wokeFromTimer || wokeFromExt;
    gBootHoldPending = false;

    const char *wakeReason = "reset";
    if (wokeFromTimer) {
        wakeReason = "timer";
    } else if (wokeFromExt) {
        wakeReason = "ext";
    }
    LOG_INFO("BootHold wake reason: %s (usb=%d, guard=%d)", wakeReason, usbPresent ? 1 : 0, gGuardEnabled ? 1 : 0);

    if (!gGuardEnabled)
        return;

#ifdef HERMESX_USB_CHARGING_QUIET_BOOT
    if (usbPresent) {
        gQuietBoot = true;
    }
#endif

    // 只有從睡眠喚醒時才要求長按，避免冷開機被攔住
    if (gWokeFromSleep) {
        gRequireLongPress = true;
        gWaitingForPress = true;
        gStartupVisualsAllowed = false;
        gPowerHoldReady = false;
        gBootHoldPending = true;
    }

#if DEBUG_BUTTONS
    LOG_DEBUG("BootHold: armed (usb=%d, wokeFromExt=%d)", usbPresent ? 1 : 0, wokeFromExt ? 1 : 0);
#endif
}

bool wokeFromSleep()
{
    return gWokeFromSleep;
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
    if (!gGuardEnabled || gPowerHoldReady)
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
    gBootHoldPending = false;
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
    gBootHoldPending = false;
    gSuppressShutdownAnim = false; // 關機動畫不要被抑制
#if DEBUG_BUTTONS
    LOG_DEBUG("BootHold: aborted (sleep)");
#endif
}

void requestShutdownAnimationSuppression()
{
    // 保留接口但不再抑制關機動畫
    if (!gGuardEnabled)
        return;
    gSuppressShutdownAnim = false;
}

bool consumeShutdownAnimationSuppression()
{
    if (!gGuardEnabled)
        return false;
    gSuppressShutdownAnim = false;
    return false;
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

bool bootHoldPending()
{
    return gGuardEnabled && gBootHoldPending;
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

void enterGateDeepSleep(bool skipSave)
{
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_NRF52)
    suppressPmuShutdownOnce();
    doDeepSleep(UINT32_MAX, true, skipSave);
#else
    (void)skipSave;
#endif
}

} // namespace HermesXPowerGuard

#endif // defined(HERMESX_GUARD_POWER_ANIMATIONS) && !MESHTASTIC_EXCLUDE_HERMESX
