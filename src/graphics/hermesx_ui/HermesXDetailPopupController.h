#pragma once

#include <cstdint>

namespace graphics
{

enum class HermesXDetailPopupAction : uint8_t {
    Unhandled,
    Consumed,
    Refresh,
    Dismissed,
};

class HermesXDetailPopupController
{
  public:
    static constexpr uint32_t DismissGuardMs = 250;

    static HermesXDetailPopupController &instance();
    HermesXDetailPopupAction handle(bool dismissRequested, int8_t direction, uint32_t nowMs, uint16_t scrollStep);

  private:
    HermesXDetailPopupController() = default;
};

} // namespace graphics
