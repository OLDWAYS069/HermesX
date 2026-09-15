#pragma once

#include <cstdint>

class HermesXPreferences
{
  public:
    static bool loadBool(const char *path, bool defaultValue);
    static bool saveBool(const char *path, bool value);
    static uint32_t loadUInt32(const char *path, uint32_t defaultValue);
    static bool saveUInt32(const char *path, uint32_t value);
};
