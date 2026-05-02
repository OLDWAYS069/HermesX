#include "SerialConsole.h"
#include "Default.h"
#include "NodeDB.h"
#include "PowerFSM.h"
#include "Throttle.h"
#include "configuration.h"
#include "platform/esp32/HermesCrashBreadcrumb.h"
#include "time.h"

#if defined(ARCH_ESP32)
#include <esp_system.h>
#endif

#ifdef RP2040_SLOW_CLOCK
#define Port Serial2
#else
#ifdef USER_DEBUG_PORT // change by WayenWeng
#define Port USER_DEBUG_PORT
#else
#define Port Serial
#endif
#endif
// Defaulting to the formerly removed phone_timeout_secs value of 15 minutes
#define SERIAL_CONNECTION_TIMEOUT (15 * 60) * 1000UL

SerialConsole *console;

namespace {
#if defined(ARCH_ESP32)
const char *usbResetReasonLabel(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_UNKNOWN:
        return "UNKNOWN";
    case ESP_RST_POWERON:
        return "POWERON";
    case ESP_RST_EXT:
        return "EXT";
    case ESP_RST_SW:
        return "SW";
    case ESP_RST_PANIC:
        return "PANIC";
    case ESP_RST_INT_WDT:
        return "INT_WDT";
    case ESP_RST_TASK_WDT:
        return "TASK_WDT";
    case ESP_RST_WDT:
        return "WDT";
    case ESP_RST_DEEPSLEEP:
        return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
        return "BROWNOUT";
    case ESP_RST_SDIO:
        return "SDIO";
#if defined(ESP_RST_USB)
    case ESP_RST_USB:
        return "USB";
#endif
#if defined(ESP_RST_JTAG)
    case ESP_RST_JTAG:
        return "JTAG";
#endif
#if defined(ESP_RST_EFUSE)
    case ESP_RST_EFUSE:
        return "EFUSE";
#endif
#if defined(ESP_RST_PWR_GLITCH)
    case ESP_RST_PWR_GLITCH:
        return "PWR_GLITCH";
#endif
#if defined(ESP_RST_CPU_LOCKUP)
    case ESP_RST_CPU_LOCKUP:
        return "CPU_LOCKUP";
#endif
    default:
        return "OTHER";
    }
}
#endif
} // namespace

void consoleInit()
{
    new SerialConsole(); // Must be dynamically allocated because we are now inheriting from thread
    DEBUG_PORT.rpInit(); // Simply sets up semaphore
}

void consolePrintf(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    console->vprintf(nullptr, format, arg);
    va_end(arg);
    console->flush();
}

SerialConsole::SerialConsole() : StreamAPI(&Port), RedirectablePrint(&Port), concurrency::OSThread("SerialConsole")
{
    assert(!console);
    console = this;
    canWrite = false; // We don't send packets to our port until it has talked to us first

#ifdef RP2040_SLOW_CLOCK
    Port.setTX(SERIAL2_TX);
    Port.setRX(SERIAL2_RX);
#endif
    Port.begin(SERIAL_BAUD);
#if defined(ARCH_NRF52) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARCH_RP2040) ||   \
    defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
    time_t timeout = millis();
    while (!Port) {
        if (Throttle::isWithinTimespanMs(timeout, FIVE_SECONDS_MS)) {
            delay(100);
        } else {
            break;
        }
    }
#endif
#if !ARCH_PORTDUINO
    emitRebooted();
    emitLogRecordText(meshtastic_LogRecord_Level_WARNING, "USBUpdate", "USB update console alive");
#if defined(ARCH_ESP32)
    char resetReasonBuf[64];
    snprintf(resetReasonBuf, sizeof(resetReasonBuf), "reset reason=%s(%d)", usbResetReasonLabel(esp_reset_reason()),
             (int)esp_reset_reason());
    emitLogRecordText(meshtastic_LogRecord_Level_WARNING, "USBUpdate", resetReasonBuf);
#endif
    const size_t breadcrumbLines = hermesCrashBreadcrumbPendingBootReportCount();
    char breadcrumbLine[160];
    for (size_t i = 0; i < breadcrumbLines; ++i) {
        if (hermesCrashBreadcrumbFormatPendingBootReportLine(i, breadcrumbLine, sizeof(breadcrumbLine))) {
            emitLogRecordText(meshtastic_LogRecord_Level_WARNING, "CrashBreadcrumb", breadcrumbLine);
        }
    }
    if (breadcrumbLines > 0) {
        hermesCrashBreadcrumbClearPendingBootReport();
    }
#endif
}

int32_t SerialConsole::runOnce()
{
    return runOncePart();
}

void SerialConsole::flush()
{
    Port.flush();
}

// For the serial port we can't really detect if any client is on the other side, so instead just look for recent messages
bool SerialConsole::checkIsConnected()
{
    return Throttle::isWithinTimespanMs(lastContactMsec, SERIAL_CONNECTION_TIMEOUT);
}

/**
 * we override this to notice when we've received a protobuf over the serial
 * stream.  Then we shut off debug serial output.
 */
bool SerialConsole::handleToRadio(const uint8_t *buf, size_t len)
{
    // In dedicated update boot we intentionally skip normal mesh init, which can leave has_lora false.
    // For USB OTA we still need the serial API handshake to work as long as serial is allowed.
    if (config.security.serial_enabled) {
        // Switch to protobufs for log messages
        usingProtobufs = true;
        canWrite = true;

        return StreamAPI::handleToRadio(buf, len);
    } else {
        return false;
    }
}

void SerialConsole::log_to_serial(const char *logLevel, const char *format, va_list arg)
{
    if (usingProtobufs && config.security.debug_log_api_enabled) {
        meshtastic_LogRecord_Level ll = RedirectablePrint::getLogLevel(logLevel);
        auto thread = concurrency::OSThread::currentThread;
        emitLogRecord(ll, thread ? thread->ThreadName.c_str() : "", format, arg);
    } else
        RedirectablePrint::log_to_serial(logLevel, format, arg);
}
