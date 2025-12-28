#pragma once

#define LODB_VERSION "1.2.0"

#include "LoDBModule.h"

/**
 * This plugin registers a Module with Meshtastic. Modules are the primary way
 * to extend Meshtastic with new functionality. Modules are registered with the
 * MPM_REGISTER_MESHTASTIC_MODULE comment directive below.
 */
// MPM_REGISTER_MESHTASTIC_MODULE: LoDBModule, lodbModule, []
