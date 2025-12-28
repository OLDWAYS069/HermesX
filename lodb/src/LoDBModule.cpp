#include "LoDBModule.h"
#include "MeshService.h"

LoDBModule::LoDBModule() : SinglePortModule("lodb", meshtastic_PortNum_TEXT_MESSAGE_APP)
{
#ifdef LODB_PLUGIN_DIAGNOSTICS
    extern void lodb_diagnostics();
    lodb_diagnostics();
#endif
}

ProcessMessage LoDBModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    // LoDB is primarily a utility library, so we don't handle messages
    // But we need to implement this to satisfy the SinglePortModule interface
    return ProcessMessage::CONTINUE;
}

LoDBModule *lodbModule;
