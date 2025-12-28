#pragma once

#include "plugin.h"
#include "SinglePortModule.h"

/**
 * LoFS Module
 *
 * A module for the LoFS unified filesystem plugin.
 * This module provides filesystem functionality for Meshtastic.
 */
class LoFSModule : public SinglePortModule
{
  public:
    LoFSModule();

  protected:
    /**
     * Handle an incoming message
     */
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
};

extern LoFSModule *lofsModule;
