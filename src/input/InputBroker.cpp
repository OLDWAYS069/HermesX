#include "InputBroker.h"
#include "PowerFSM.h" // needed for event trigger
#include "graphics/Screen.h"

extern graphics::Screen *screen;

InputBroker *inputBroker = nullptr;
static uint32_t s_wakeOnlyUntilMs = 0;
static constexpr uint32_t kWakeInputGuardMs = 220;

InputBroker::InputBroker(){};

void InputBroker::registerSource(Observable<const InputEvent *> *source)
{
    this->inputEventObserver.observe(source);
}

int InputBroker::handleInputEvent(const InputEvent *event)
{
    if (!event) {
        return 0;
    }

    const uint32_t now = millis();
    if (s_wakeOnlyUntilMs != 0 && now < s_wakeOnlyUntilMs) {
        return 0;
    }

    const bool wasScreenOff = (screen != nullptr) && !screen->isOn();

    powerFSM.trigger(EVENT_INPUT);

    // If display was off, first input should only wake the display.
    // This prevents wake presses/rotations from immediately entering menus.
    if (wasScreenOff) {
        s_wakeOnlyUntilMs = now + kWakeInputGuardMs;
        return 0;
    }

    // During wake transition, screen->setOn(true) is queued asynchronously.
    // Keep consuming input until the panel is actually marked on.
    if (screen != nullptr && !screen->isOn()) {
        s_wakeOnlyUntilMs = now + kWakeInputGuardMs;
        return 0;
    }

    this->notifyObservers(event);
    return 0;
}
