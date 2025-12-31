#include "GenericThreadModule.h"
#include "MeshService.h"
#include "SPILock.h"
#include "configuration.h"
#include "concurrency/LockGuard.h"
#include "gps/RTC.h"
#include "mesh/Router.h"
#include "FSCommon.h"
#include <Arduino.h>
#include <cstring>
#include <time.h>

/*
Generic Thread Module allows for the execution of custom code at a set interval.
*/
GenericThreadModule *genericThreadModule;

namespace
{
constexpr ChannelIndex kGreetingChannel = 1;
constexpr uint16_t kEarliestValidYear = 2024;
constexpr const char *kGreetingMessage = u8"HermesTrack全體團隊祝大家，新年快樂！！";
constexpr const char *kLastGreetingFile = "/prefs/newyear_greeting.bin";

uint16_t lastGreetingYear = 0;
bool lastGreetingYearLoaded = false;

void loadLastGreetingYear()
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);
    auto f = FSCom.open(kLastGreetingFile, FILE_O_READ);
    if (f) {
        if (f.available() >= (int)sizeof(lastGreetingYear)) {
            f.read((uint8_t *)&lastGreetingYear, sizeof(lastGreetingYear));
            LOG_INFO("Loaded last New Year greeting year: %u", lastGreetingYear);
        }
        f.close();
    }
#endif
    lastGreetingYearLoaded = true;
}

void saveLastGreetingYear(uint16_t year)
{
#ifdef FSCom
    concurrency::LockGuard g(spiLock);

    if (!FSCom.exists("/prefs"))
        FSCom.mkdir("/prefs");

    auto f = FSCom.open(kLastGreetingFile, FILE_O_WRITE);
    if (f) {
        f.write((uint8_t *)&year, sizeof(year));
        f.flush();
        f.close();
        LOG_INFO("Persisted New Year greeting year: %u", year);
    } else {
        LOG_WARN("Failed to open %s for writing", kLastGreetingFile);
    }
#endif
}

bool sendNewYearGreeting(uint16_t year)
{
    if (!router || !service) {
        LOG_WARN("Router or service not ready, skip New Year greeting");
        return false;
    }

    meshtastic_MeshPacket *p = router->allocForSending();
    if (!p) {
        LOG_WARN("Unable to allocate packet for New Year greeting");
        return false;
    }

    p->to = NODENUM_BROADCAST;
    p->channel = kGreetingChannel;
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->want_ack = false;

    size_t msgLen = strlen(kGreetingMessage);
    if (msgLen > sizeof(p->decoded.payload.bytes)) {
        msgLen = sizeof(p->decoded.payload.bytes);
    }
    p->decoded.payload.size = msgLen;
    memcpy(p->decoded.payload.bytes, kGreetingMessage, msgLen);

    service->sendToMesh(p, RX_SRC_LOCAL, false);
    LOG_INFO("Sent New Year greeting for %u on channel %u", year, kGreetingChannel);
    return true;
}
} // namespace

GenericThreadModule::GenericThreadModule() : concurrency::OSThread("GenericThreadModule") {}

int32_t GenericThreadModule::runOnce()
{

    bool enabled = true;
    if (!enabled)
        return disable();

    if (firstTime) {
        firstTime = 0;
        loadLastGreetingYear();
    }

    uint32_t now = getValidTime(RTCQualityDevice, true);
    if (now == 0)
        return my_interval;

    time_t nowSec = now;
    struct tm *nowTmPtr = gmtime(&nowSec);
    if (!nowTmPtr)
        return my_interval;

    struct tm nowTm = *nowTmPtr;
    uint16_t currentYear = nowTm.tm_year + 1900;

    if (currentYear < kEarliestValidYear)
        return my_interval;

    bool isMidnightOnNewYear = nowTm.tm_mon == 0 && nowTm.tm_mday == 1 && nowTm.tm_hour == 0 && nowTm.tm_min == 0;
    if (isMidnightOnNewYear && currentYear != lastGreetingYear) {
        if (sendNewYearGreeting(currentYear)) {
            lastGreetingYear = currentYear;
            if (!lastGreetingYearLoaded) {
                // ensure we don't attempt to load again after the first send
                lastGreetingYearLoaded = true;
            }
            saveLastGreetingYear(currentYear);
        }
    }

    return my_interval;
}
