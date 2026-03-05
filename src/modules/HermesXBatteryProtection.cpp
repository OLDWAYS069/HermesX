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

bool gLoaded = false;
bool gEnabled = true;

void loadIfNeeded()
{
    if (gLoaded) {
        return;
    }
    gLoaded = true;

#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    auto f = FSCom.open(kOverdischargePrefFile, FILE_O_READ);
    if (!f) {
        return;
    }

    String content;
    while (f.available()) {
        content += static_cast<char>(f.read());
    }
    f.close();
    content.trim();
    if (content.length() == 0) {
        return;
    }

    if (content == "0" || content.equalsIgnoreCase("false") || content.equalsIgnoreCase("off")) {
        gEnabled = false;
    } else {
        gEnabled = true;
    }
#endif
}

void persist()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    if (!FSCom.exists("/prefs")) {
        FSCom.mkdir("/prefs");
    }
    if (FSCom.exists(kOverdischargePrefFile)) {
        FSCom.remove(kOverdischargePrefFile);
    }

    auto f = FSCom.open(kOverdischargePrefFile, FILE_O_WRITE);
    if (!f) {
        return;
    }
    f.print(gEnabled ? "1" : "0");
    f.flush();
    f.close();
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
    persist();
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

} // namespace HermesXBatteryProtection

#endif

