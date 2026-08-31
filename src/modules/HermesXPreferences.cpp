#include "HermesXPreferences.h"

#include "FSCommon.h"

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
