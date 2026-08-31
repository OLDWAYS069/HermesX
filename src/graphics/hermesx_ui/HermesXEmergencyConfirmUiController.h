#pragma once

#include <cstdint>

namespace graphics
{

struct HermesXEmergencyConfirmInputBindings {
    HermesXEmergencyConfirmInputBindings(char pressValue = 0, char clockwiseValue = 0, char counterClockwiseValue = 0)
        : press(pressValue), clockwise(clockwiseValue), counterClockwise(counterClockwiseValue)
    {
    }
    char press;
    char clockwise;
    char counterClockwise;
};

enum class HermesXEmergencyConfirmAction : uint8_t {
    Unhandled,
    Consumed,
    Canceled,
};

class HermesXEmergencyConfirmUiController
{
  public:
    static HermesXEmergencyConfirmUiController &instance();
    HermesXEmergencyConfirmAction handle(char code, const HermesXEmergencyConfirmInputBindings &bindings);

  private:
    HermesXEmergencyConfirmUiController() = default;
};

} // namespace graphics
