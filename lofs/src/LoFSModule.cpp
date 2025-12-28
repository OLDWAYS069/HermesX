#include "LoFSModule.h"
#include "MeshService.h"

LoFSModule::LoFSModule() : SinglePortModule("lofs", meshtastic_PortNum_TEXT_MESSAGE_APP)
{
#ifdef LOFS_PLUGIN_DIAGNOSTICS
    extern void lofs_diagnostics();
    lofs_diagnostics();
#endif
}

ProcessMessage LoFSModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    // LoFS is primarily a utility library, so we don't handle messages
    // But we need to implement this to satisfy the SinglePortModule interface
    return ProcessMessage::CONTINUE;
}

LoFSModule *lofsModule;
