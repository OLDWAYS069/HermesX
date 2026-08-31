#pragma once

#include <cstdint>

namespace graphics
{

struct HermesXRotaryLockInputBindings {
    HermesXRotaryLockInputBindings(char pressValue = 0, char clockwiseValue = 0, char counterClockwiseValue = 0)
        : press(pressValue), clockwise(clockwiseValue), counterClockwise(counterClockwiseValue)
    {
    }
    char press;
    char clockwise;
    char counterClockwise;
};

enum class HermesXRotaryLockAction : uint8_t {
    Unhandled,
    Consumed,
    SelectionChanged,
    Confirmed,
    Canceled,
};

class HermesXRotaryLockUiController
{
  public:
    static HermesXRotaryLockUiController &instance();
    HermesXRotaryLockAction handle(char code, bool rotarySource, uint32_t nowMs,
                                   const HermesXRotaryLockInputBindings &bindings);

  private:
    HermesXRotaryLockUiController() = default;
};

} // namespace graphics
