#include "InputBroker.h"
#include "PowerFSM.h" // needed for event trigger
#include "configuration.h"
#if !MESHTASTIC_EXCLUDE_HERMESX
#include "modules/HermesXInterfaceModule.h"
#endif
#include <cstring>

InputBroker *inputBroker = nullptr;

InputBroker::InputBroker(){};

void InputBroker::registerSource(Observable<const InputEvent *> *source)
{
    this->inputEventObserver.observe(source);
}

int InputBroker::handleInputEvent(const InputEvent *event)
{
    powerFSM.trigger(EVENT_INPUT);
#if !MESHTASTIC_EXCLUDE_HERMESX
    if (event && event->source && std::strcmp(event->source, "rotEnc1") == 0) {
        const char pressChar = static_cast<char>(moduleConfig.canned_message.inputbroker_event_press);
        if (event->inputEvent == pressChar && HermesXInterfaceModule::instance) {
            HermesXInterfaceModule::instance->registerRawButtonPress(HermesXInterfaceModule::HermesButtonSource::Alt);
        }
    }
#endif
    this->notifyObservers(event);
    return 0;
}