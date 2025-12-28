#pragma once

#include "plugin.h"
#include "SinglePortModule.h"

/**
 * LoDB Module
 *
 * A module for the LoDB database plugin.
 * This module provides database functionality for Meshtastic.
 */
class LoDBModule : public SinglePortModule
{
  public:
    LoDBModule();

  protected:
    /**
     * Handle an incoming message
     */
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;
};

extern LoDBModule *lodbModule;
