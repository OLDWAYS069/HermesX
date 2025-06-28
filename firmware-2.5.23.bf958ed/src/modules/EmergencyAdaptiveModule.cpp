// EmergencyAdaptiveModule.cpp
#include "EmergencyAdaptiveModule.h"
#include "configuration.h"
#include <cstring>

EmergencyAdaptiveModule::EmergencyAdaptiveModule() : MeshModule("EmergencyAdaptive") {}

void EmergencyAdaptiveModule::setup() {
    loadConfig();
}

void EmergencyAdaptiveModule::loop() {
    if (!emergency_mode) {
        // ⚠️ Replace this with actual message trigger logic in production
        const char *msg = "@EmergencyActive";
        if (msg && strstr(msg, "@EmergencyActive")) {
            emergency_mode = true;
            applyEmergencyConfig();
            saveConfig();
        }
    }
}

void EmergencyAdaptiveModule::applyEmergencyConfig() {
    config.emergency_mode_active = true;
    config.lora_tx_power_override = 20;
    config.lora_freq_override = 923.0f;
    config.device_role = meshtastic_DeviceRole_CLIENT;
    config.position_broadcast_secs = 43200;

    // 實際套用到運作中的配置
    ::config.lora.tx_power = config.lora_tx_power_override;
    ::config.lora.override_frequency = config.lora_freq_override;
    ::config.device.role = static_cast<meshtastic_Config_DeviceConfig_Role>(config.device_role);
    ::config.position.position_broadcast_secs = config.position_broadcast_secs;
    ::config.power.ls_secs = 86400;
    ::config.position.gps_update_interval = 86400;
    ::config.power.is_power_saving = true;
}

void EmergencyAdaptiveModule::saveConfig() {
    nodeDB->saveProto("/prefs/emergency_config.proto",
                      sizeof(meshtastic_EmergencyConfig),
                      &meshtastic_EmergencyConfig_msg,
                      &config);
}

void EmergencyAdaptiveModule::loadConfig() {
    if (!nodeDB->loadProto("/prefs/emergency_config.proto",
                           sizeof(meshtastic_EmergencyConfig),
                           sizeof(config),
                           &meshtastic_EmergencyConfig_msg,
                           &config)) {
        config.emergency_mode_active = false;
        config.allow_public_text_messages = true;
        config.lora_tx_power_override = 17;
        config.lora_freq_override = 0.0f;
        config.device_role = meshtastic_DeviceRole_CLIENT;
        config.position_broadcast_secs = 43200;
    }
}