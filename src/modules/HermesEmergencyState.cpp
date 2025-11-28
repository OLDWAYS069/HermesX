#include "HermesEmergencyState.h"
#include "FSCommon.h"
#include "SPILock.h"
#include "HermesXLog.h"

static const char *kHermesEmergencyStateFile = "/prefs/lighthouse_emstate.bin";
bool gHermesEmergencyAwaitingSafe = false;

static void saveState(bool awaiting)
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    FSCom.mkdir("/prefs");
    auto f = FSCom.open(kHermesEmergencyStateFile, FILE_O_WRITE);
    if (f) {
        uint8_t v = awaiting ? 1 : 0;
        f.write(&v, sizeof(v));
        f.flush();
        f.close();
    } else {
        HERMESX_LOG_WARN("EMACT: failed to save %s", kHermesEmergencyStateFile);
    }
#endif
}

void HermesSetEmergencyAwaitingSafe(bool on, bool persist)
{
    gHermesEmergencyAwaitingSafe = on;
    if (persist)
        saveState(on);
}

void HermesClearEmergencyAwaitingSafe(bool persist)
{
    gHermesEmergencyAwaitingSafe = false;
    if (persist)
        saveState(false);
}

bool HermesIsEmergencyAwaitingSafe()
{
    return gHermesEmergencyAwaitingSafe;
}

void HermesLoadEmergencyAwaitingSafe()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    auto f = FSCom.open(kHermesEmergencyStateFile, FILE_O_READ);
    if (f) {
        uint8_t v = 0;
        if (f.available() >= 1) {
            f.read(&v, 1);
            gHermesEmergencyAwaitingSafe = (v == 1);
        }
        f.close();
    }
#endif
}
