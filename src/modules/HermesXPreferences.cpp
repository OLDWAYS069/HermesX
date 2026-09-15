#include "HermesXPreferences.h"

#include "FSCommon.h"
#include <cstdlib>

bool HermesXPreferences::loadBool(const char *path, bool defaultValue)
{
    if (!path || !FSCom.exists(path)) {
        return defaultValue;
    }

    auto file = FSCom.open(path, FILE_O_READ);
    if (!file) {
        return defaultValue;
    }

    String raw = file.readStringUntil('\n');
    raw.trim();
    return raw != "0";
}

bool HermesXPreferences::saveBool(const char *path, bool value)
{
    if (!path) {
        return false;
    }
    if (!FSCom.exists("/prefs")) {
        FSCom.mkdir("/prefs");
    }
    if (FSCom.exists(path)) {
        FSCom.remove(path);
    }

    auto file = FSCom.open(path, FILE_O_WRITE);
    if (!file) {
        return false;
    }
    file.print(value ? "1\n" : "0\n");
    return true;
}

uint32_t HermesXPreferences::loadUInt32(const char *path, uint32_t defaultValue)
{
    if (!path || !FSCom.exists(path)) {
        return defaultValue;
    }

    auto file = FSCom.open(path, FILE_O_READ);
    if (!file) {
        return defaultValue;
    }

    String raw = file.readStringUntil('\n');
    raw.trim();
    if (raw.length() == 0) {
        return defaultValue;
    }
    char *end = nullptr;
    const unsigned long parsed = strtoul(raw.c_str(), &end, 10);
    return end != raw.c_str() && *end == '\0' ? static_cast<uint32_t>(parsed) : defaultValue;
}

bool HermesXPreferences::saveUInt32(const char *path, uint32_t value)
{
    if (!path) {
        return false;
    }
    if (!FSCom.exists("/prefs")) {
        FSCom.mkdir("/prefs");
    }
    if (FSCom.exists(path)) {
        FSCom.remove(path);
    }

    auto file = FSCom.open(path, FILE_O_WRITE);
    if (!file) {
        return false;
    }
    file.print(value);
    file.print('\n');
    return true;
}
