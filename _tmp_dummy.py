from pathlib import Path
path = Path('src/input/InputBroker.cpp')
text = path.read_text(encoding='utf-8', errors='surrogateescape')
old = "int InputBroker::handleInputEvent(const InputEvent *event)\n{\n    powerFSM.trigger(EVENT_INPUT);\n    this->notifyObservers(event);\n    return 0;\n}\n"
new = "int InputBroker::handleInputEvent(const InputEvent *event)\n{\n    powerFSM.trigger(EVENT_INPUT);\n#if !MESHTASTIC_EXCLUDE_HERMESX\n    if (event and event.source and event.source == 'rotEnc1'):\n        press_char = chr(moduleConfig.canned_message.inputbroker_event_press & 0xFF)\n        if event.inputEvent == press_char and HermesXInterfaceModule.instance:\n            HermesXInterfaceModule.instance.registerRawButtonPress(HermesXInterfaceModule.HermesButtonSource.Alt)\n#endif\n    this->notifyObservers(event);\n    return 0;\n}\n"
"""Cannot use Python-like check yet"""
