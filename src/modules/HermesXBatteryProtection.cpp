#include "modules/HermesXBatteryProtection.h"

#if !MESHTASTIC_EXCLUDE_HERMESX

#include "FSCommon.h"
#include "SPILock.h"
#include <Arduino.h>

namespace HermesXBatteryProtection
{
namespace
{
constexpr const char *kOverdischargePrefFile = "/prefs/hermesx_overdischarge_guard.txt";
constexpr const char *kOverdischargeThresholdPrefFile = "/prefs/hermesx_overdischarge_threshold_mv.txt";

bool gLoaded = false;
bool gEnabled = true;
uint16_t gThresholdMv = kDefaultThresholdMv;

uint16_t clampThresholdMv(uint16_t thresholdMv)
{
    if (thresholdMv < kMinThresholdMv) {
        return kMinThresholdMv;
    }
    if (thresholdMv > kMaxThresholdMv) {
        return kMaxThresholdMv;
    }
    return thresholdMv;
}

String readTrimmedFile(const char *path)
{
    String content;

#ifdef FSCom
    auto f = FSCom.open(path, FILE_O_READ);
    if (!f) {
        return content;
    }

    while (f.available()) {
        content += static_cast<char>(f.read());
    }
    f.close();
    content.trim();
#else
    (void)path;
#endif

    return content;
}

void ensurePrefsDir()
{
#ifdef FSCom
    if (!FSCom.exists("/prefs")) {
        FSCom.mkdir("/prefs");
    }
#endif
}

void writeFile(const char *path, const String &content)
{
#ifdef FSCom
    ensurePrefsDir();
    if (FSCom.exists(path)) {
        FSCom.remove(path);
    }

    auto f = FSCom.open(path, FILE_O_WRITE);
    if (!f) {
        return;
    }
    f.print(content);
    f.flush();
    f.close();
#else
    (void)path;
    (void)content;
#endif
}

void loadIfNeeded()
{
    if (gLoaded) {
        return;
    }
    gLoaded = true;

#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    const String enabledContent = readTrimmedFile(kOverdischargePrefFile);
    if (enabledContent == "0" || enabledContent.equalsIgnoreCase("false") || enabledContent.equalsIgnoreCase("off")) {
        gEnabled = false;
    } else if (enabledContent.length() > 0) {
        gEnabled = true;
    }

    const String thresholdContent = readTrimmedFile(kOverdischargeThresholdPrefFile);
    if (thresholdContent.length() > 0) {
        uint16_t parsedThresholdMv = 0;
        if (thresholdContent.indexOf('.') >= 0) {
            const float volts = thresholdContent.toFloat();
            if (volts > 0.0f) {
                parsedThresholdMv = static_cast<uint16_t>((volts * 1000.0f) + 0.5f);
            }
        } else {
            const long rawValue = thresholdContent.toInt();
            if (rawValue > 0) {
                if (rawValue < 10) {
                    parsedThresholdMv = static_cast<uint16_t>(rawValue * 1000L);
                } else {
                    parsedThresholdMv = static_cast<uint16_t>(rawValue);
                }
            }
        }

        if (parsedThresholdMv > 0) {
            gThresholdMv = clampThresholdMv(parsedThresholdMv);
        }
    }
#endif
}

void persistEnabled()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    writeFile(kOverdischargePrefFile, gEnabled ? "1" : "0");
#endif
}

void persistThreshold()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    writeFile(kOverdischargeThresholdPrefFile, String(gThresholdMv));
#endif
}
} // namespace

bool isEnabled()
{
    loadIfNeeded();
    return gEnabled;
}

void setEnabled(bool enabled)
{
    loadIfNeeded();
    if (gEnabled == enabled) {
        return;
    }
    gEnabled = enabled;
    persistEnabled();
}

uint16_t getThresholdMv()
{
    loadIfNeeded();
    return gThresholdMv;
}

void setThresholdMv(uint16_t thresholdMv)
{
    loadIfNeeded();
    const uint16_t clampedThresholdMv = clampThresholdMv(thresholdMv);
    if (gThresholdMv == clampedThresholdMv) {
        return;
    }
    gThresholdMv = clampedThresholdMv;
    persistThreshold();
}

} // namespace HermesXBatteryProtection

#else

namespace HermesXBatteryProtection
{

bool isEnabled()
{
    return true;
}

void setEnabled(bool)
{
}

uint16_t getThresholdMv()
{
    return kDefaultThresholdMv;
}

void setThresholdMv(uint16_t)
{
}

} // namespace HermesXBatteryProtection

#endif
