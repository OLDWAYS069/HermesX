#include "HermesEmUiModule.h"

#if HAS_SCREEN

#include <OLEDDisplay.h>
#include <OLEDDisplayUi.h>

HermesEmUiModule *hermesEmUiModule = nullptr;

HermesEmUiModule::HermesEmUiModule() : MeshModule("HermesEmUi")
{
}

void HermesEmUiModule::enterEmergencyMode(const char *reason)
{
    active = true;
    if (reason != nullptr) {
        banner = reason;
    } else {
        banner = "";
    }

    requestFocus();
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REGENERATE_FRAMESET;
    notifyObservers(&e);
}

void HermesEmUiModule::exitEmergencyMode()
{
    if (!active) {
        return;
    }
    active = false;
    UIFrameEvent e;
    e.action = UIFrameEvent::Action::REGENERATE_FRAMESET;
    notifyObservers(&e);
}

void HermesEmUiModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState * /*state*/, int16_t x, int16_t y)
{
    if (!active) {
        return;
    }

    const int16_t height = display->getHeight();

    display->setTextAlignment(TEXT_ALIGN_LEFT);

#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif

    const char *title = "EMERGENCY";
    display->setFont(FONT_SMALL);
    display->drawString(x + 2, y + 2, title);

    if (banner.length() > 0) {
        display->setFont(FONT_SMALL);
        display->drawString(x + 2, y + 14, banner);
    }

    const char *items[] = {"TRAPPED", "MEDICAL", "SUPPLIES", "OK HERE"};
    const int itemCount = sizeof(items) / sizeof(items[0]);
    const int16_t top = y + 28;
    const int16_t lineHeight = 12;

    for (int i = 0; i < itemCount; ++i) {
        const int16_t rowY = top + i * lineHeight;
        if (rowY + lineHeight > y + height) {
            break;
        }
        display->drawString(x + 6, rowY, items[i]);
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);
}

#endif
