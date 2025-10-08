#include "EmUiModule.h"

#if HAS_SCREEN && !MESHTASTIC_EXCLUDE_HERMESX

#include "EmergencyAdaptiveModule.h"
#include "HermesXInterfaceModule.h"
#include "configuration.h"
#include "graphics/ScreenFonts.h"
#include "mesh/NodeDB.h"
#include "mesh/generated/meshtastic/module_config.pb.h"
#include <OLEDDisplay.h>
#include <OLEDDisplayUi.h>

EmUiModule *emUiModule = nullptr;

namespace {
constexpr uint16_t kHighlightPadding = 2;
constexpr uint32_t kStatusDisplayMs = 3000;
} // namespace

EmUiModule::EmUiModule() : MeshModule("em_ui")
{
    emUiModule = this;
}

void EmUiModule::setup()
{
    MeshModule::setup();

    if (inputBroker) {
        inputObserver.observe(inputBroker);
    }

    if (emergencyModule) {
        emergencyModule->addModeListener([this](bool active) { handleModeChanged(active); });
        isActive = emergencyModule->isEmergencyActive();
    }

    rebuildMenu();
}

#if HAS_SCREEN
bool EmUiModule::wantUIFrame()
{
    return isActive && !menu.empty();
}

void EmUiModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    (void)state;

    if (!display) {
        return;
    }

    if (!isActive) {
        return;
    }

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();

    display->setColor(OLEDDISPLAY_COLOR::WHITE);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_MEDIUM);

    const String title = localize("Emergency Mode", "?綽??鈭方???);
    display->drawString(x, y, title);

    int16_t cursorY = y + FONT_HEIGHT_MEDIUM + 2;

    display->setFont(FONT_SMALL);
    const String hint = localize("Rotate to choose, press to send", "????鞊?????蹓鳴");
    display->drawString(x, cursorY, hint);
    cursorY += FONT_HEIGHT_SMALL + 4;

    display->setFont(FONT_MEDIUM);

    const int16_t optionWidth = width - (x + 4);
    for (size_t idx = 0; idx < menu.size(); ++idx) {
        const bool selected = (idx == selectedIndex);
        const String &label = menu[idx].label;
        const int16_t drawY = cursorY + static_cast<int16_t>(idx) * (FONT_HEIGHT_MEDIUM + 2);

        if (selected) {
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
            display->fillRect(x, drawY - kHighlightPadding, optionWidth, FONT_HEIGHT_MEDIUM + kHighlightPadding * 2);
            display->setColor(OLEDDISPLAY_COLOR::BLACK);
        } else {
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
            display->drawRect(x, drawY - kHighlightPadding, optionWidth, FONT_HEIGHT_MEDIUM + kHighlightPadding * 2);
        }

        display->drawString(x + 3, drawY, label);

        if (selected) {
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
        }
    }

    if (!statusMessage.isEmpty()) {
        if (statusDeadlineMs && millis() > statusDeadlineMs) {
            statusMessage = "";
            statusDeadlineMs = 0;
        } else {
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
            display->setFont(FONT_SMALL);
            const int16_t statusY = height - FONT_HEIGHT_SMALL - 2;
            display->drawString(x, statusY, statusMessage);
        }
    }
}
#endif

void EmUiModule::rebuildMenu()
{
    menu.clear();

    if (!emergencyModule) {
        return;
    }

    const bool drillMode = (moduleConfig.emergency.mode == meshtastic_ModuleConfig_EmergencyConfig_Mode_DRILL);

    auto addOption = [&](const String &label, Action action, std::initializer_list<uint32_t> codes = {}) {
        Option opt;
        opt.label = label;
        opt.action = action;
        opt.codes.assign(codes);
        menu.push_back(opt);
    };

    String sosLabel = localize("Send SOS", "?瞏捍蹓???);
    if (drillMode) {
        sosLabel += " (DRILL)";
    }
    addOption(sosLabel, Action::SOS);

    addOption(localize("Send SAFE", "?蹓鳴?堆??), Action::SAFE);

    addOption(localize("Need Medical", "???秋撚???), Action::NEED, {1});
    addOption(localize("Need Water", "???秋撚?∠??), Action::NEED, {2});
    addOption(localize("Need Power", "???秋撚謆??), Action::NEED, {3});
    addOption(localize("Need Food", "???秋撚???), Action::NEED, {4});
    addOption(localize("Need Evac", "???秋撮???), Action::NEED, {5});

    if (moduleConfig.emergency.role == meshtastic_ModuleConfig_EmergencyConfig_Role_SHELTER) {
        addOption(localize("Offer Medical", "??????"), Action::RESOURCE, {1});
        addOption(localize("Offer Water", "??????), Action::RESOURCE, {2});
        addOption(localize("Offer Power", "????擗?"), Action::RESOURCE, {3});
        addOption(localize("Offer Food", "???????), Action::RESOURCE, {4});
        addOption(localize("Offer Evac", "?????伍?"), Action::RESOURCE, {5});
    }

    if (selectedIndex >= menu.size()) {
        selectedIndex = 0;
    }
}

void EmUiModule::handleModeChanged(bool active)
{
    const bool changed = (isActive != active);
    isActive = active;

    if (isActive) {
        rebuildMenu();
        requestFocus();
    }

    if (changed) {
        requestUiUpdate(UIFrameEvent::Action::REGENERATE_FRAMESET);
    } else if (isActive) {
        requestUiUpdate(UIFrameEvent::Action::REDRAW_ONLY);
    }
}

void EmUiModule::showStatus(const String &message, uint32_t durationMs)
{
    statusMessage = message;
    statusDeadlineMs = durationMs ? millis() + durationMs : 0;
    requestUiUpdate(UIFrameEvent::Action::REDRAW_ONLY);
}

void EmUiModule::requestUiUpdate(UIFrameEvent::Action action)
{
    UIFrameEvent evt;
    evt.action = action;
    notifyObservers(&evt);
}

bool EmUiModule::triggerAction(const Option &opt)
{
    if (!emergencyModule) {
        return false;
    }

    bool ok = false;

    switch (opt.action) {
    case Action::SOS:
        ok = emergencyModule->sendSOS();
        if (ok && HermesXInterfaceModule::instance) {
            HermesXInterfaceModule::instance->playSOSFeedback();
        }
        break;
    case Action::SAFE:
        ok = emergencyModule->sendSafe();
        break;
    case Action::NEED:
        if (!opt.codes.empty()) {
            ok = emergencyModule->sendNeed(opt.codes.front());
        }
        break;
    case Action::RESOURCE:
        if (!opt.codes.empty()) {
            ok = emergencyModule->broadcastResource(opt.codes.data(), opt.codes.size());
        }
        break;
    }

    const String successText = localize("Request sent", "??蹓鳴????);
    const String failureText = localize("Failed to send", "?瞏捍蹓颱???);

    showStatus(ok ? successText : failureText, kStatusDisplayMs);

    return ok;
}

String EmUiModule::localize(const char *en, const char *zh) const
{
    if (moduleConfig.emergency.lang == meshtastic_ModuleConfig_EmergencyConfig_Language_ZH) {
        return String(zh);
    }
    return String(en);
}

int EmUiModule::handleInputEvent(const InputEvent *event)
{
    if (!isActive || !event) {
        return 0;
    }

    if (!emergencyModule || !emergencyModule->isEmergencyActive()) {
        return 0;
    }

    bool handled = false;
    const char input = event->inputEvent;

    const char up = static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const char down = static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const char select = static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const char cancel = static_cast<char>(meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL);

    if (input == up && !menu.empty()) {
        if (selectedIndex == 0) {
            selectedIndex = menu.size() - 1;
        } else {
            --selectedIndex;
        }
        handled = true;
    } else if (input == down && !menu.empty()) {
        selectedIndex = (selectedIndex + 1) % menu.size();
        handled = true;
    } else if (input == select && selectedIndex < menu.size()) {
        handled = triggerAction(menu[selectedIndex]);
    } else if (input == cancel) {
        showStatus(localize("Hold SAFE to cancel", "?ｇ?蹓鳴?堆?脤?鈭???), kStatusDisplayMs);
        handled = true;
    }

    if (handled) {
        requestUiUpdate(UIFrameEvent::Action::REDRAW_ONLY);
    }

    return handled ? 1 : 0;
}

#endif

