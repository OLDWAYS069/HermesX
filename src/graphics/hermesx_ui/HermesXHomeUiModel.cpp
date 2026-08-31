#include "HermesXHomeUiModel.h"

#include <cstring>

namespace graphics
{

HermesXHomeUiModel &HermesXHomeUiModel::instance()
{
    static HermesXHomeUiModel model;
    return model;
}

void HermesXHomeUiModel::reset()
{
    valid_ = false;
    lastBaseRefreshMs_ = 0;
}

void HermesXHomeUiModel::resetDog()
{
    dogCache_ = HermesXHomeDogCache{};
}

bool HermesXHomeUiModel::shouldRenderDog(uint16_t frame, uint8_t pose, bool forceRedraw) const
{
    return forceRedraw || !dogCache_.valid || dogCache_.frame != frame || dogCache_.pose != pose;
}

void HermesXHomeUiModel::commitDogBounds(int16_t x, int16_t y, int16_t width, int16_t height)
{
    dogCache_.x = x;
    dogCache_.y = y;
    dogCache_.width = width;
    dogCache_.height = height;
}

void HermesXHomeUiModel::commitDogFrame(uint16_t frame, uint8_t pose)
{
    dogCache_.valid = true;
    dogCache_.frame = frame;
    dogCache_.pose = pose;
}

HermesXHomeBaseDecision HermesXHomeUiModel::evaluateBase(const HermesXHomeBaseState &state,
                                                         bool entering,
                                                         bool basePainted,
                                                         uint32_t nowMs,
                                                         uint32_t telemetryRefreshIntervalMs) const
{
    HermesXHomeBaseDecision decision;
    decision.telemetryChanged = !valid_ || hasBattery_ != state.hasBattery ||
                                batteryPercent_ != state.batteryPercent ||
                                satelliteCount_ != state.satelliteCount;
    decision.telemetryRefreshDue =
        lastBaseRefreshMs_ == 0 || (nowMs - lastBaseRefreshMs_) >= telemetryRefreshIntervalMs;
    decision.telemetryDirty = decision.telemetryChanged && (!basePainted || decision.telemetryRefreshDue);
    const char *date = state.date ? state.date : "";
    decision.baseDirty = entering || !valid_ || stealth_ != state.stealth || role_ != state.role ||
                         std::strncmp(date_, date, sizeof(date_)) != 0 || decision.telemetryDirty;
    return decision;
}

void HermesXHomeUiModel::commitBase(const HermesXHomeBaseState &state, uint32_t nowMs)
{
    valid_ = true;
    stealth_ = state.stealth;
    hasBattery_ = state.hasBattery;
    batteryPercent_ = state.batteryPercent;
    satelliteCount_ = state.satelliteCount;
    role_ = state.role;
    const char *date = state.date ? state.date : "";
    std::strncpy(date_, date, sizeof(date_) - 1);
    date_[sizeof(date_) - 1] = '\0';
    lastBaseRefreshMs_ = nowMs;
}

} // namespace graphics
