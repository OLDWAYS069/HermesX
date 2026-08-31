#pragma once

#include <cstdint>

namespace graphics
{

enum class HermesXTakModePage : uint8_t {
    Main,
    Popup,
    Settings,
    ChannelSelect,
    TransitionEnter,
    TransitionExit,
};

struct HermesXTakModeUiState {
    HermesXTakModePage page = HermesXTakModePage::Main;
    uint8_t popupSelected = 0;
    uint8_t popupOffset = 0;
    uint8_t settingsSelected = 0;
    uint8_t settingsOffset = 0;
    uint32_t transitionStartedAtMs = 0;
};

class HermesXTakModeUiModel
{
  public:
    static HermesXTakModeUiModel &instance();

    HermesXTakModeUiState &state() { return state_; }
    const HermesXTakModeUiState &state() const { return state_; }

    void showMain();
    void showPopup(uint8_t selected = 0);
    void showSettings();
    void showChannelSelect(uint8_t selected = 0);
    void startTransition(bool entering, uint32_t nowMs);

  private:
    HermesXTakModeUiModel() = default;

    HermesXTakModeUiState state_;
};

} // namespace graphics
