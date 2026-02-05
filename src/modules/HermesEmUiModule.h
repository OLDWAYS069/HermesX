#pragma once

#if HAS_SCREEN

#include "MeshModule.h"
#include <Arduino.h>

class HermesEmUiModule : public MeshModule, public Observable<const UIFrameEvent *>
{
  public:
    HermesEmUiModule();

    void enterEmergencyMode(const char *reason = nullptr);
    void exitEmergencyMode();
    bool isActive() const { return active; }

  protected:
    bool wantPacket(const meshtastic_MeshPacket * /*p*/) override { return false; }
    bool wantUIFrame() override { return active; }
    Observable<const UIFrameEvent *> *getUIFrameObservable() override { return this; }
    void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) override;

  private:
    bool active = false;
    String banner;
};

extern HermesEmUiModule *hermesEmUiModule;

#endif
