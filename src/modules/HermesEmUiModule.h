#pragma once

#include "configuration.h"

#if HAS_SCREEN

#include "SinglePortModule.h"
#include "mesh/generated/meshtastic/mesh.pb.h"
#include "input/InputBroker.h"
#include "modules/HermesXInterfaceModule.h"
#include <Arduino.h>

class HermesXEmUiModule : public SinglePortModule, public Observable<const UIFrameEvent *>
{
    CallbackObserver<HermesXEmUiModule, const InputEvent *> inputObserver =
        CallbackObserver<HermesXEmUiModule, const InputEvent *>(this, &HermesXEmUiModule::handleInputEvent);

  public:
    HermesXEmUiModule();

    void enterEmergencyMode(const char *reason = nullptr);
    void exitEmergencyMode();
    bool isActive() const { return active; }
    void tickSiren(uint32_t now);
    void drawOverlay(OLEDDisplay *display, OLEDDisplayUiState *state);

  protected:
    bool wantPacket(const meshtastic_MeshPacket *p) override
    {
        return awaitingAck && p->decoded.portnum == meshtastic_PortNum_ROUTING_APP;
    }
    bool wantUIFrame() override { return active; }
    Observable<const UIFrameEvent *> *getUIFrameObservable() override { return this; }
    void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) override;
    bool interceptingKeyboardInput() override { return active; }
    int handleInputEvent(const InputEvent *event);
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    enum class EmAction {
        Trapped,
        Medical,
        Supplies,
        OkHere,
    };

    bool active = false;
    String banner;
    int selectedIndex = 0;
    bool awaitingAck = false;
    PacketId lastRequestId = 0;
    bool lastAckSuccess = false;
    uint32_t lastAckAtMs = 0;
    bool selectionArmed = false;
    uint32_t lastNavAtMs = 0;
    uint32_t lastSendAtMs = 0;
    bool hasSentOnce = false;
    bool screamActive = false;
    uint32_t nextScreamAtMs = 0;
    LedTheme savedTheme{};
    bool themeSaved = false;

    void setThemeActive(bool enabled);
    void sendEmergencyAction(EmAction action);
    void startScream();
    void stopScream();
    void tickScream(uint32_t now);
};

extern HermesXEmUiModule *hermesXEmUiModule;

#endif
