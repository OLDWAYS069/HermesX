#pragma once

#include "HermesXLowMemoryUiModel.h"
#include <cstdint>

namespace graphics
{

struct HermesXLowMemoryInput {
    HermesXLowMemoryInput(char codeValue = 0) : code(codeValue) {}
    char code;
};

struct HermesXLowMemoryInputBindings {
    HermesXLowMemoryInputBindings(char pressValue = 0, char clockwiseValue = 0, char counterClockwiseValue = 0)
        : press(pressValue), clockwise(clockwiseValue), counterClockwise(counterClockwiseValue)
    {
    }
    char press;
    char clockwise;
    char counterClockwise;
};

enum class HermesXLowMemoryAction : uint8_t {
    Unhandled,
    Consumed,
    Refresh,
    Dismissed,
    CleanNodesRequested,
};

class HermesXLowMemoryUiController
{
  public:
    static HermesXLowMemoryUiController &instance();

    HermesXLowMemoryAction handle(const HermesXLowMemoryInput &event,
                                  const HermesXLowMemoryInputBindings &bindings,
                                  uint32_t nowMs,
                                  uint32_t suppressMs);

  private:
    HermesXLowMemoryUiController() = default;
};

} // namespace graphics
