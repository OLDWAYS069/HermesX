#pragma once

#include "HermesXTakModeUiModel.h"
#include <cstdint>

namespace graphics
{

struct HermesXTakModeInput {
    HermesXTakModeInput(const char *sourceValue = nullptr, char codeValue = 0) : source(sourceValue), code(codeValue) {}

    const char *source;
    char code;
};

struct HermesXTakModeInputBindings {
    HermesXTakModeInputBindings(char pressValue = 0, char clockwiseValue = 0, char counterClockwiseValue = 0)
        : press(pressValue), clockwise(clockwiseValue), counterClockwise(counterClockwiseValue)
    {
    }

    char press;
    char clockwise;
    char counterClockwise;
};

enum class HermesXTakModeAction : uint8_t {
    Unhandled,
    Consumed,
    Refresh,
    ExitToActionRequested,
    ToggleModeRequested,
    OpenGroupSettingsRequested,
    OpenEmUiRequested,
    OpenFinderRequested,
    SettingSelected,
    ChannelSelected,
};

class HermesXTakModeUiController
{
  public:
    static HermesXTakModeUiController &instance();

    HermesXTakModeAction handle(const HermesXTakModeInput &event,
                                const HermesXTakModeInputBindings &bindings,
                                HermesXTakModeUiState &state,
                                uint8_t popupRowCount,
                                uint8_t settingsRowCount,
                                uint8_t channelRowCount,
                                uint8_t selectedChannelRow);

  private:
    HermesXTakModeUiController() = default;
};

} // namespace graphics
