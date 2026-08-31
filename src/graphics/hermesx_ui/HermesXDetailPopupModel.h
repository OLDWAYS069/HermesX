#pragma once

#include <Arduino.h>
#include <cstdint>

namespace graphics
{

struct HermesXDetailPopupState {
    String title;
    String body;
    bool visible = false;
    uint16_t scrollY = 0;
    uint16_t maxScrollY = 0;
    uint32_t shownAtMs = 0;
};

class HermesXDetailPopupModel
{
  public:
    static HermesXDetailPopupModel &instance();

    HermesXDetailPopupState &state() { return state_; }
    const HermesXDetailPopupState &state() const { return state_; }
    void show(const char *title, const String &body, uint32_t nowMs);
    void dismiss();

  private:
    HermesXDetailPopupModel() = default;

    HermesXDetailPopupState state_;
};

} // namespace graphics
