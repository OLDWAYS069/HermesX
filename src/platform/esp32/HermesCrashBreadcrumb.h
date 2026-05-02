#pragma once

#include "configuration.h"
#include <stdint.h>

#ifdef ARCH_ESP32
#include <esp_system.h>
#endif

enum class HermesCrashBreadcrumbId : uint16_t {
    None = 0,
    PhoneConfigStart,
    PhoneClose,
    PhoneStateMyInfo,
    PhoneStateUiData,
    PhoneStateOwnNodeInfo,
    PhoneStateMetadata,
    PhoneStateChannels,
    PhoneStateConfig,
    PhoneStateModuleConfig,
    PhoneStateOtherNodeInfos,
    PhoneStateFileManifest,
    PhoneStateConfigComplete,
    PhonePrefetch,
    XmodemPacketRx,
    XmodemStartMeta,
    XmodemOpenWrite,
    XmodemOpenWriteOk,
    XmodemOpenWriteFail,
    BleConfigStart,
    BleConfigComplete,
    BleToPhoneEnqueue,
    BleToPhoneQueueFull,
    BleReadWait,
    BleReadDequeue,
    BleFromPhoneWrite,
    BleDisconnect,
};

const char *hermesCrashBreadcrumbName(HermesCrashBreadcrumbId id);
void hermesCrashBreadcrumbRecord(HermesCrashBreadcrumbId id, uint16_t arg = 0);
void hermesCrashBreadcrumbClear();

#ifdef ARCH_ESP32
void hermesCrashBreadcrumbReportBoot(esp_reset_reason_t resetReason);
size_t hermesCrashBreadcrumbPendingBootReportCount();
bool hermesCrashBreadcrumbFormatPendingBootReportLine(size_t index, char *buf, size_t len);
void hermesCrashBreadcrumbClearPendingBootReport();
#else
inline void hermesCrashBreadcrumbReportBoot(int) {}
inline size_t hermesCrashBreadcrumbPendingBootReportCount() { return 0; }
inline bool hermesCrashBreadcrumbFormatPendingBootReportLine(size_t, char *, size_t) { return false; }
inline void hermesCrashBreadcrumbClearPendingBootReport() {}
#endif
