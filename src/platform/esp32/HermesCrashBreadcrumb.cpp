#include "HermesCrashBreadcrumb.h"

#include <cstdio>
#include <cstring>

#ifdef ARCH_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

namespace {

constexpr uint32_t kHermesCrashBreadcrumbMagic = 0x48584231U;
constexpr size_t kHermesCrashBreadcrumbCapacity = 24;

struct HermesCrashBreadcrumbEntry {
    uint32_t seq;
    uint16_t id;
    uint16_t arg;
};

struct HermesCrashBreadcrumbState {
    uint32_t magic;
    uint32_t nextSeq;
    uint16_t head;
    uint16_t count;
    HermesCrashBreadcrumbEntry entries[kHermesCrashBreadcrumbCapacity];
};

RTC_DATA_ATTR static HermesCrashBreadcrumbState gHermesCrashBreadcrumbState = {};
static portMUX_TYPE gHermesCrashBreadcrumbMux = portMUX_INITIALIZER_UNLOCKED;
static HermesCrashBreadcrumbState gPendingBootReport = {};
static bool gPendingBootReportValid = false;
static int gPendingBootResetReason = 0;

void clearUnlocked()
{
    memset(&gHermesCrashBreadcrumbState, 0, sizeof(gHermesCrashBreadcrumbState));
    gHermesCrashBreadcrumbState.magic = kHermesCrashBreadcrumbMagic;
    gHermesCrashBreadcrumbState.nextSeq = 1;
}

void ensureInitializedUnlocked()
{
    if (gHermesCrashBreadcrumbState.magic != kHermesCrashBreadcrumbMagic ||
        gHermesCrashBreadcrumbState.count > kHermesCrashBreadcrumbCapacity ||
        gHermesCrashBreadcrumbState.head >= kHermesCrashBreadcrumbCapacity) {
        clearUnlocked();
    }
}

bool isCrashLikeResetReason(esp_reset_reason_t resetReason)
{
    switch (resetReason) {
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        return true;
#if defined(ESP_RST_CPU_LOCKUP)
    case ESP_RST_CPU_LOCKUP:
        return true;
#endif
    default:
        return false;
    }
}

} // namespace
#endif

const char *hermesCrashBreadcrumbName(HermesCrashBreadcrumbId id)
{
    switch (id) {
    case HermesCrashBreadcrumbId::None:
        return "None";
    case HermesCrashBreadcrumbId::PhoneConfigStart:
        return "PhoneConfigStart";
    case HermesCrashBreadcrumbId::PhoneClose:
        return "PhoneClose";
    case HermesCrashBreadcrumbId::PhoneStateMyInfo:
        return "PhoneStateMyInfo";
    case HermesCrashBreadcrumbId::PhoneStateUiData:
        return "PhoneStateUiData";
    case HermesCrashBreadcrumbId::PhoneStateOwnNodeInfo:
        return "PhoneStateOwnNodeInfo";
    case HermesCrashBreadcrumbId::PhoneStateMetadata:
        return "PhoneStateMetadata";
    case HermesCrashBreadcrumbId::PhoneStateChannels:
        return "PhoneStateChannels";
    case HermesCrashBreadcrumbId::PhoneStateConfig:
        return "PhoneStateConfig";
    case HermesCrashBreadcrumbId::PhoneStateModuleConfig:
        return "PhoneStateModuleConfig";
    case HermesCrashBreadcrumbId::PhoneStateOtherNodeInfos:
        return "PhoneStateOtherNodeInfos";
    case HermesCrashBreadcrumbId::PhoneStateFileManifest:
        return "PhoneStateFileManifest";
    case HermesCrashBreadcrumbId::PhoneStateConfigComplete:
        return "PhoneStateConfigComplete";
    case HermesCrashBreadcrumbId::PhonePrefetch:
        return "PhonePrefetch";
    case HermesCrashBreadcrumbId::XmodemPacketRx:
        return "XmodemPacketRx";
    case HermesCrashBreadcrumbId::XmodemStartMeta:
        return "XmodemStartMeta";
    case HermesCrashBreadcrumbId::XmodemOpenWrite:
        return "XmodemOpenWrite";
    case HermesCrashBreadcrumbId::XmodemOpenWriteOk:
        return "XmodemOpenWriteOk";
    case HermesCrashBreadcrumbId::XmodemOpenWriteFail:
        return "XmodemOpenWriteFail";
    case HermesCrashBreadcrumbId::BleConfigStart:
        return "BleConfigStart";
    case HermesCrashBreadcrumbId::BleConfigComplete:
        return "BleConfigComplete";
    case HermesCrashBreadcrumbId::BleToPhoneEnqueue:
        return "BleToPhoneEnqueue";
    case HermesCrashBreadcrumbId::BleToPhoneQueueFull:
        return "BleToPhoneQueueFull";
    case HermesCrashBreadcrumbId::BleReadWait:
        return "BleReadWait";
    case HermesCrashBreadcrumbId::BleReadDequeue:
        return "BleReadDequeue";
    case HermesCrashBreadcrumbId::BleFromPhoneWrite:
        return "BleFromPhoneWrite";
    case HermesCrashBreadcrumbId::BleDisconnect:
        return "BleDisconnect";
    default:
        return "Unknown";
    }
}

#ifdef ARCH_ESP32
void hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId id, uint16_t arg)
{
    portENTER_CRITICAL(&gHermesCrashBreadcrumbMux);
    ensureInitializedUnlocked();

    const uint16_t idValue = static_cast<uint16_t>(id);
    if (gHermesCrashBreadcrumbState.count > 0) {
        const size_t lastIndex =
            (gHermesCrashBreadcrumbState.head + gHermesCrashBreadcrumbState.count - 1) % kHermesCrashBreadcrumbCapacity;
        HermesCrashBreadcrumbEntry &lastEntry = gHermesCrashBreadcrumbState.entries[lastIndex];
        if (lastEntry.id == idValue && lastEntry.arg == arg) {
            lastEntry.seq = gHermesCrashBreadcrumbState.nextSeq++;
            portEXIT_CRITICAL(&gHermesCrashBreadcrumbMux);
            return;
        }
    }

    size_t writeIndex;
    if (gHermesCrashBreadcrumbState.count < kHermesCrashBreadcrumbCapacity) {
        writeIndex = (gHermesCrashBreadcrumbState.head + gHermesCrashBreadcrumbState.count) % kHermesCrashBreadcrumbCapacity;
        gHermesCrashBreadcrumbState.count++;
    } else {
        writeIndex = gHermesCrashBreadcrumbState.head;
        gHermesCrashBreadcrumbState.head = (gHermesCrashBreadcrumbState.head + 1) % kHermesCrashBreadcrumbCapacity;
    }

    gHermesCrashBreadcrumbState.entries[writeIndex] = {
        gHermesCrashBreadcrumbState.nextSeq++,
        idValue,
        arg,
    };
    portEXIT_CRITICAL(&gHermesCrashBreadcrumbMux);
}

void hermesCrashBreadcrumbClear()
{
    portENTER_CRITICAL(&gHermesCrashBreadcrumbMux);
    clearUnlocked();
    portEXIT_CRITICAL(&gHermesCrashBreadcrumbMux);
}

void hermesCrashBreadcrumbReportBoot(esp_reset_reason_t resetReason)
{
    if (!isCrashLikeResetReason(resetReason)) {
        return;
    }

    HermesCrashBreadcrumbState snapshot = {};
    portENTER_CRITICAL(&gHermesCrashBreadcrumbMux);
    ensureInitializedUnlocked();
    if (gHermesCrashBreadcrumbState.count == 0) {
        portEXIT_CRITICAL(&gHermesCrashBreadcrumbMux);
        return;
    }

    snapshot = gHermesCrashBreadcrumbState;
    clearUnlocked();
    portEXIT_CRITICAL(&gHermesCrashBreadcrumbMux);

    gPendingBootReport = snapshot;
    gPendingBootReportValid = true;
    gPendingBootResetReason = (int)resetReason;

    LOG_WARN("Recovered %u crash breadcrumbs after reset reason %d", snapshot.count, (int)resetReason);
    for (uint16_t i = 0; i < snapshot.count; ++i) {
        const size_t entryIndex = (snapshot.head + i) % kHermesCrashBreadcrumbCapacity;
        const HermesCrashBreadcrumbEntry &entry = snapshot.entries[entryIndex];
        LOG_WARN("Crash breadcrumb %u/%u: #%lu %s arg=%u", i + 1, snapshot.count, (unsigned long)entry.seq,
                 hermesCrashBreadcrumbName(static_cast<HermesCrashBreadcrumbId>(entry.id)), entry.arg);
    }
}

size_t hermesCrashBreadcrumbPendingBootReportCount()
{
    if (!gPendingBootReportValid) {
        return 0;
    }
    return (size_t)gPendingBootReport.count + 1U;
}

bool hermesCrashBreadcrumbFormatPendingBootReportLine(size_t index, char *buf, size_t len)
{
    if (!buf || len == 0 || !gPendingBootReportValid) {
        return false;
    }

    if (index == 0) {
        snprintf(buf, len, "Recovered %u crash breadcrumbs after reset reason %d", gPendingBootReport.count,
                 gPendingBootResetReason);
        return true;
    }

    if (index > gPendingBootReport.count) {
        return false;
    }

    const size_t entryIndex = (gPendingBootReport.head + index - 1U) % kHermesCrashBreadcrumbCapacity;
    const HermesCrashBreadcrumbEntry &entry = gPendingBootReport.entries[entryIndex];
    snprintf(buf, len, "Crash breadcrumb %u/%u: #%lu %s arg=%u", (unsigned)index, gPendingBootReport.count,
             (unsigned long)entry.seq, hermesCrashBreadcrumbName(static_cast<HermesCrashBreadcrumbId>(entry.id)), entry.arg);
    return true;
}

void hermesCrashBreadcrumbClearPendingBootReport()
{
    gPendingBootReport = {};
    gPendingBootReportValid = false;
    gPendingBootResetReason = 0;
}
#else
void hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId, uint16_t) {}

void hermesCrashBreadcrumbClear() {}
#endif
