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
    bool isSirenEnabled() const { return sirenEnabled; }
    void setSirenEnabled(bool enabled);
    void tickSiren(uint32_t now);
    void drawOverlay(OLEDDisplay *display, OLEDDisplayUiState *state);
    void sendResetLighthouseNow();

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

    enum class UiMode {
        Menu,
        PassphraseEdit,
    };

    bool active = false;
    UiMode uiMode = UiMode::Menu;
    String banner;
    int selectedIndex = 0;
    int listOffset = 0;
    bool awaitingAck = false;
    PacketId lastRequestId = 0;
    bool lastAckSuccess = false;
    uint32_t lastAckAtMs = 0;
    bool selectionArmed = false;
    uint32_t lastNavAtMs = 0;
    int8_t lastNavDir = 0;
    uint32_t lastSendAtMs = 0;
    bool hasSentOnce = false;
    bool screamActive = false;
    bool sirenEnabled = true;
    uint32_t nextScreamAtMs = 0;
    LedTheme savedTheme{};
    bool themeSaved = false;
    uint8_t editingPassSlot = 0;
    String passDraft;
    String lastAlertMessage;
    int keyRow = 0;
    int keyCol = 0;

    void setThemeActive(bool enabled);
    void sendEmergencyAction(EmAction action);
    void sendResetLighthouse();
    void enterPassphraseEdit(uint8_t slot);
    void exitPassphraseEdit(bool save);
    void sendLocalTextToPhone(const String &text);
    void notifyPassphraseSaved(uint8_t slot, const String &value, bool ok);
    void updateListOffset();
    int handleMenuInput(const InputEvent *event);
    int handlePassphraseInput(const InputEvent *event);
    void startScream();
    void stopScream();
    void tickScream(uint32_t now);
};

extern HermesXEmUiModule *hermesXEmUiModule;

#endif
