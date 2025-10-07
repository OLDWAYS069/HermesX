#pragma once

// 導入 Meshtastic logging 系統定義（內含 LOG_INFO 等）
#include "DebugConfiguration.h"

// HermesX 專用 log prefix（可改成你想要的標記）
#define HERMESX_LOG_PREFIX "[HermesX] "

// 內部包裝 LOG_* 宏
#define HERMESX_LOG_INFO(fmt, ...)   LOG_INFO(HERMESX_LOG_PREFIX fmt, ##__VA_ARGS__)
#define HERMESX_LOG_WARN(fmt, ...)   LOG_WARN(HERMESX_LOG_PREFIX fmt, ##__VA_ARGS__)
#define HERMESX_LOG_ERROR(fmt, ...)  LOG_ERROR(HERMESX_LOG_PREFIX fmt, ##__VA_ARGS__)
#define HERMESX_LOG_DEBUG(fmt, ...)  LOG_DEBUG(HERMESX_LOG_PREFIX fmt, ##__VA_ARGS__)
#define HERMESX_LOG_CRIT(fmt, ...)   LOG_CRIT(HERMESX_LOG_PREFIX fmt, ##__VA_ARGS__)
#define HERMESX_LOG_TRACE(fmt, ...)  LOG_TRACE(HERMESX_LOG_PREFIX fmt, ##__VA_ARGS__)