#pragma once

#include "SinglePortModule.h"
#include "lodb/LoDB.h"
#include "lodb_demo.pb.h"

/**
 * LoDBDemoModule
 *
 * Example module that listens on TEXT_MESSAGE_APP and logs every message to
 * LoDB using the lodb_demo messages table.
 */
class LoDBDemoModule : public SinglePortModule
{
  public:
    LoDBDemoModule();
    ~LoDBDemoModule();

  protected:
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    LoDb *db;
};
