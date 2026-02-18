#pragma once

#include <GpioLogic.h>
#include <OLEDDisplay.h>
#if defined(HERMESX_TFT_FASTPATH)
#include <cstdint>
#endif

/**
 * An adapter class that allows using the LovyanGFX library as if it was an OLEDDisplay implementation.
 *
 * Remaining TODO:
 * optimize display() to only draw changed pixels (see other OLED subclasses for examples)
 * Use the fast NRF52 SPI API rather than the slow standard arduino version
 *
 * turn radio back on - currently with both on spi bus is fucked? or are we leaving chip select asserted?
 */
class TFTDisplay : public OLEDDisplay
{
  public:
    struct ColorZone {
        int16_t x;
        int16_t y;
        int16_t width;
        int16_t height;
        uint16_t fg;
        uint16_t bg;

        ColorZone() : x(0), y(0), width(0), height(0), fg(0xFFFF), bg(0x0000) {}

        ColorZone(int16_t xIn, int16_t yIn, int16_t widthIn, int16_t heightIn, uint16_t fgIn, uint16_t bgIn)
            : x(xIn), y(yIn), width(widthIn), height(heightIn), fg(fgIn), bg(bgIn)
        {
        }
    };

    static constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
    {
        return static_cast<uint16_t>(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
    }

    /* constructor
    FIXME - the parameters are not used, just a temporary hack to keep working like the old displays
    */
    TFTDisplay(uint8_t, int, int, OLEDDISPLAY_GEOMETRY, HW_I2C);

    // Write the buffer to the display memory
    virtual void display() override { display(false); };
    virtual void display(bool fromBlank);

    // Turn the display upside down
    virtual void flipScreenVertically();

    // Touch screen (static handlers)
    static bool hasTouch(void);
    static bool getTouch(int16_t *x, int16_t *y);

    // Functions for changing display brightness
    void setDisplayBrightness(uint8_t);

    /**
     * shim to make the abstraction happy
     *
     */
    void setDetected(uint8_t detected);

    // Palette customization for monochrome buffer rendering on color TFTs.
    // Zones can be updated per-frame; only palette default changes force a full redraw.
    void clearColorPaletteZones();
    void markColorPaletteDirty();
    void resetColorPalette(bool forceFullRedraw = true);
    void setColorPaletteDefaults(uint16_t fg, uint16_t bg);
    void addColorPaletteZone(const ColorZone &zone);

    /**
     * This is normally managed entirely by TFTDisplay, but some rare applications (heltec tracker) might need to replace the
     * default GPIO behavior with something a bit more complex.
     *
     * We (cruftily) make it static so that variant.cpp can access it without needing a ptr to the TFTDisplay instance.
     */
    static GpioPin *backlightEnable;

#if defined(HERMESX_TFT_FASTPATH)
    bool writeRow565(int16_t x, int16_t y, const uint16_t *row565, int len);
    uint16_t mapColor(uint32_t logicalColor) const;
#endif

  private:
    static constexpr uint8_t kMaxColorZones = 8;

    ColorZone colorZones[kMaxColorZones]{};
    uint8_t colorZoneCount = 0;
    uint16_t colorDefaultFg = 0xFFFF;
    uint16_t colorDefaultBg = 0x0000;
    uint16_t baseDefaultFg = 0xFFFF;
    uint16_t baseDefaultBg = 0x0000;
    bool colorPaletteDirty = false;

    uint16_t resolveForegroundColor(int16_t x, int16_t y) const;
    uint16_t resolveBackgroundColor(int16_t x, int16_t y) const;
    void invalidateRectForPalette(int16_t x, int16_t y, int16_t width, int16_t height);

  protected:
    // the header size of the buffer used, e.g. for the SPI command header
    virtual int getBufferOffset(void) override { return 0; }

    // Send a command to the display (low level function)
    virtual void sendCommand(uint8_t com) override;

    // Connect to the display
    virtual bool connect() override;
};
