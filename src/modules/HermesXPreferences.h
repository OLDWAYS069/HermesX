#pragma once

class HermesXPreferences
{
  public:
    static bool loadBool(const char *path, bool defaultValue);
    static bool saveBool(const char *path, bool value);
};
