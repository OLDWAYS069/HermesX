#pragma once

#include "graphics/hermesx_input/HermesXBpmfEngine.h"
#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace graphics
{

struct HermesXComposerInput {
    HermesXComposerInput(const char *sourceValue = nullptr, char codeValue = 0, char keyValue = 0)
        : source(sourceValue), code(codeValue), key(keyValue)
    {
    }

    const char *source;
    char code;
    char key;
};

struct HermesXComposerInputBindings {
    HermesXComposerInputBindings(char pressValue = 0, char clockwiseValue = 0, char counterClockwiseValue = 0)
        : press(pressValue), clockwise(clockwiseValue), counterClockwise(counterClockwiseValue)
    {
    }

    char press;
    char clockwise;
    char counterClockwise;
};

enum class HermesXComposerAction : uint8_t {
    Consumed,
    Refresh,
    CloseRequested,
    SendRequested,
};

class HermesXDirectMessageComposer
{
  public:
    static HermesXDirectMessageComposer &instance();

    void start(uint32_t destination, bool fromGroupDetail);
    void stop();
    HermesXComposerAction handleInput(const HermesXComposerInput &event,
                                      const HermesXComposerInputBindings &bindings,
                                      uint32_t nowMs,
                                      size_t payloadLimit);
    void showSendFailure(uint32_t nowMs);

    bool active() const { return active_; }
    bool fromGroupDetail() const { return fromGroupDetail_; }
    uint32_t destination() const { return destination_; }
    const String &draft() const { return draft_; }
    String previewText() const;
    String compositionText() const;

    bool candidateMode() const { return candidateMode_; }
    uint8_t candidateCursor() const { return candidateCursor_; }
    const std::vector<std::string> &candidates() const { return bpmf_.candidates(); }

    const char *const (*keyRows() const)[10];
    const uint8_t *keyRowLengths() const;
    uint8_t keyRowCount() const;
    uint8_t keyRow() const { return keyRow_; }
    uint8_t keyCol() const { return keyCol_; }

    String &toast() { return toast_; }
    uint32_t &toastUntilMs() { return toastUntilMs_; }

  private:
    HermesXDirectMessageComposer() = default;

    void resetState();
    void removeLastDraftCharacter();
    bool openCandidates(uint32_t nowMs);
    int8_t navigationDirection(const HermesXComposerInput &event,
                               const HermesXComposerInputBindings &bindings) const;
    HermesXComposerAction requestSend(uint32_t nowMs);

    bool active_ = false;
    bool fromGroupDetail_ = false;
    uint32_t destination_ = 0;
    String draft_;
    String toast_;
    uint32_t toastUntilMs_ = 0;
    uint8_t keyRow_ = 0;
    uint8_t keyCol_ = 0;
    bool lowercase_ = false;
    bool bopomofoMode_ = false;
    bool candidateMode_ = false;
    uint8_t candidateCursor_ = 0;
    hermesx_bpmf::HermesXBpmfEngine bpmf_;
};

} // namespace graphics
