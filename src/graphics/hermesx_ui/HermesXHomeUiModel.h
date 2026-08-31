#pragma once

#include <cstdint>

namespace graphics
{

struct HermesXHomeBaseState {
    bool stealth = false;
    bool hasBattery = false;
    uint8_t batteryPercent = 0;
    uint8_t satelliteCount = 0;
    int32_t role = 0;
    const char *date = nullptr;
};

struct HermesXHomeBaseDecision {
    bool telemetryChanged = false;
    bool telemetryRefreshDue = false;
    bool telemetryDirty = false;
    bool baseDirty = false;
};

struct HermesXHomeDogCache {
    bool valid = false;
    uint16_t frame = 0xFFFF;
    uint8_t pose = 0xFF;
    int16_t x = -1;
    int16_t y = -1;
    int16_t width = 0;
    int16_t height = 0;
};

class HermesXHomeUiModel
{
  public:
    static HermesXHomeUiModel &instance();

    void invalidate() { valid_ = false; }
    void reset();
    void invalidateDog() { dogCache_.valid = false; }
    void resetDog();
    bool shouldRenderDog(uint16_t frame, uint8_t pose, bool forceRedraw) const;
    const HermesXHomeDogCache &dogCache() const { return dogCache_; }
    void commitDogBounds(int16_t x, int16_t y, int16_t width, int16_t height);
    void commitDogFrame(uint16_t frame, uint8_t pose);
    HermesXHomeBaseDecision evaluateBase(const HermesXHomeBaseState &state,
                                         bool entering,
                                         bool basePainted,
                                         uint32_t nowMs,
                                         uint32_t telemetryRefreshIntervalMs) const;
    void commitBase(const HermesXHomeBaseState &state, uint32_t nowMs);

  private:
    HermesXHomeUiModel() = default;

    bool valid_ = false;
    bool stealth_ = false;
    bool hasBattery_ = false;
    uint8_t batteryPercent_ = 0;
    uint8_t satelliteCount_ = 0;
    int32_t role_ = 0;
    char date_[24]{};
    uint32_t lastBaseRefreshMs_ = 0;
    HermesXHomeDogCache dogCache_;
};

} // namespace graphics
