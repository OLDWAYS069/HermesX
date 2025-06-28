// EmergencyAdaptiveModule.h 
#pragma once

#include "mesh/MeshModule.h"
#include "emergency.pb.h"  // 自定義的 config protobuf
#include "mesh/NodeDB.h"

class EmergencyAdaptiveModule : public MeshModule {
public:
    EmergencyAdaptiveModule();

    void setup() override;
    void loop() ;

private:
    void applyEmergencyConfig();
    void saveConfig();
    void loadConfig();

    meshtastic_EmergencyConfig config;
    bool emergency_mode = false;
};