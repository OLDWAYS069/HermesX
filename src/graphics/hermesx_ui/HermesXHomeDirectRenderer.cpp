#include "HermesXHomeDirectRenderer.h"

#include "HermesXDirectTftPrimitives.h"
#include "HermesXHomeUiModel.h"
#include "HermesXHomeUiRenderer.h"
#include "graphics/TFTDisplay.h"

#include <Arduino.h>
#include <cstddef>
#include <esp_task_wdt.h>

namespace graphics
{

void renderDirectHomeDog(TFTDisplay *tft,
                                int16_t displayW,
                                int16_t displayH,
                                uint16_t dogFrame,
                                uint8_t dogPose)
{
    if (!tft) {
        return;
    }

    static constexpr int16_t kDogSpriteW = 24;
    static constexpr int16_t kDogSpriteH = 24;
    static constexpr int16_t kDogScale = 2;
    static constexpr int16_t kDogDrawW = kDogSpriteW * kDogScale;
    static constexpr int16_t kDogDrawH = kDogSpriteH * kDogScale;
    if (displayW <= 0 || displayH <= 0 || displayW < kDogDrawW || displayH < kDogDrawH) {
        return;
    }

    static constexpr uint8_t kDogLyingFrameCount = 4;
    static constexpr uint8_t kDogSittingFrameCount = 4;
    static constexpr const char *kDogLyingFrames[kDogLyingFrameCount][kDogSpriteH] = {
        {
            "...............O....O...",
            "..............OTO..OHO..",
            "..............OLO..OHO..",
            "..............OLO..OHO..",
            "..............OHT..HLO..",
            "..............OHTOOHLO..",
            ".............OOLTHHHHO..",
            ".............OOLHHTHHO..",
            ".............OSLHHOHHO..",
            ".............SSHHHOHHS..",
            ".............SOHHHHHHHO.",
            "...........OSOHHHHHHSOO.",
            ".....OSOOOOOOSSHHHOOOSOO",
            "TOOOOOSSSSOOSSSOHHOOSSSO",
            ".OHLLOSSSSSSSSOHHHOOOO..",
            "..OSTOSHHSSSLOSOHHHHHO..",
            "...OOOHHHLTHLHHOHHHHHO..",
            "......HHHTOHLHHHH..LSO..",
            "......HHHTOTTHHHHHHLLO..",
            "......HHHTSTTHHHHHLLOOO.",
            ".OTLOHHHOOTTTHHHOOOOHHHS",
            "..TLOHHHTLSTSHHHHHHHOHLO",
            "..WWOHHHHHLOOHHHHHHHTHLO",
            ".....OOOOOOO.OOOOOOOOO..",
        },
        {
            "...............O....O...",
            "..............OTO..OHO..",
            "..............OLO..OHO..",
            "..............OLO..OHO..",
            "..............OHT..HLO..",
            "..............OHTOOHLO..",
            ".............OOLTHHHHO..",
            ".............OOLHHTHHO..",
            ".............OSLHHOHHO..",
            ".............SSHHHOHHS..",
            ".............SOHHHHHHHO.",
            "...........OSOHHHHHHSOO.",
            "....OOSOOOOOOSSHHHOOOSOO",
            ".TOOOOSSSSOOSSSOHHOOSSSO",
            "..OHLOSSSSSSSSOHHHOOOO..",
            "...OSTSHHSSSLOSOHHHHHO..",
            "....OOHHHLTHLHHOHHHHHO..",
            "......HHHTOHLHHHH..LSO..",
            "......HHHTOTTHHHHHHLLO..",
            "......HHHTSTTHHHHHLLOOO.",
            ".OTLOHHHOOTTTHHHOOOOHHHS",
            "..TLOHHHTLSTSHHHHHHHOHLO",
            "..WWOHHHHHLOOHHHHHHHTHLO",
            ".....OOOOOOO.OOOOOOOOO..",
        },
        {
            "...............O....O...",
            "..............OTO..OHO..",
            "..............OLO..OHO..",
            "..............OLO..OHO..",
            "..............OHT..HLO..",
            "..............OHTOOHLO..",
            ".............OOLTHHHHO..",
            ".............OOLHHTHHO..",
            ".............OSLHHOHHO..",
            ".............SSHHHOHHS..",
            ".............SOHHHHHHHO.",
            "...........OSOHHHHHHSOO.",
            ".....OSOOOOOOSSHHHOOOSOO",
            "....TOSSSSOOSSSOHHOOSSSO",
            "...OHOSSSSSSSSOHHHOOOO..",
            "....OSTHHSSSLOSOHHHHHO..",
            ".....OHHHLTHLHHOHHHHHO..",
            "......HHHTOHLHHHH..LSO..",
            "......HHHTOTTHHHHHHLLO..",
            "......HHHTSTTHHHHHLLOOO.",
            ".OTLOHHHOOTTTHHHOOOOHHHS",
            "..TLOHHHTLSTSHHHHHHHOHLO",
            "..WWOHHHHHLOOHHHHHHHTHLO",
            ".....OOOOOOO.OOOOOOOOO..",
        },
        {
            "...............O....O...",
            "..............OTO..OHO..",
            "..............OLO..OHO..",
            "..............OLO..OHO..",
            "..............OHT..HLO..",
            "..............OHTOOHLO..",
            ".............OOLTHHHHO..",
            ".............OOLHHTHHO..",
            ".............OSLHHOHHO..",
            ".............SSHHHOHHS..",
            ".............SOHHHHHHHO.",
            "...........OSOHHHHHHSOO.",
            "....OOSOOOOOOSSHHHOOOSOO",
            ".TOOOOSSSSOOSSSOHHOOSSSO",
            "..OHLOSSSSSSSSOHHHOOOO..",
            "...OSTSHHSSSLOSOHHHHHO..",
            "....OOHHHLTHLHHOHHHHHO..",
            "......HHHTOHLHHHH..LSO..",
            "......HHHTOTTHHHHHHLLO..",
            "......HHHTSTTHHHHHLLOOO.",
            ".OTLOHHHOOTTTHHHOOOOHHHS",
            "..TLOHHHTLSTSHHHHHHHOHLO",
            "..WWOHHHHHLOOHHHHHHHTHLO",
            ".....OOOOOOO.OOOOOOOOO..",
        },
    };
    static constexpr const char *kDogSittingFrames[kDogSittingFrameCount][kDogSpriteH] = {
        {
            "..............OO...OO...",
            "..............TS...HT...",
            "..............LTO.OHTO..",
            "..............LTOOOTTO..",
            ".............OTLHHHHHO..",
            ".............OTLHOOHHT..",
            ".............OTTHOOHHOOO",
            ".............OHHHHHHSSOO",
            ".............OHHHOOOSSSO",
            "............OOHHHOOSTL..",
            "............OOOHHHOHHT..",
            "............OSOHHHHOOO..",
            "...........OOOSLHHHHHO..",
            "..........OSSSSLLSHHHO..",
            "..........OSSSSTLS.HHO..",
            ".........OOOSHHHHS..H...",
            ".........OSSSHHHHH.HHG..",
            "........OSSSSTTHHTHTTG..",
            "........OSHHOSHHHTHTHG..",
            "TSOOO...OLHHHOOHHSSHHG..",
            ".OHLSS..HHHHHOOHHTSHHG..",
            "..OSTO..HHHHOOOHHOOHHG..",
            ".....OOOHHHHHHOHHHOHHHO.",
            ".........OOOOOOOOOOOOOO.",
        },
        {
            "..............OO...OO...",
            "..............TS...HT...",
            "..............LTO.OHTO..",
            "..............LTOOOTTO..",
            ".............OTLHHHHHO..",
            ".............OTLHOOHHT..",
            ".............OTTHOOHHOOO",
            ".............OHHHHHHSSOO",
            ".............OHHHOOOSSSO",
            "............OOHHHOOSTL..",
            "............OOOHHHOHHT..",
            "............OSOHHHHOOO..",
            "...........OOOSLHHHHHO..",
            "..........OSSSSLLSHHHO..",
            "..........OSSSSTLS.HHO..",
            ".........OOOSHHHHS..H...",
            ".........OSSSHHHHH.HHG..",
            "........OSSSSTTHHTHTTG..",
            "........OSHHOSHHHTHTHG..",
            "..TSOO..OLHHHOOHHSSHHG..",
            "..OHLSO.HHHHHOOHHTSHHG..",
            "...OST..HHHHOOOHHOOHHG..",
            ".....OOOHHHHHHOHHHOHHHO.",
            ".........OOOOOOOOOOOOOO.",
        },
        {
            "..............OO...OO...",
            "..............TS...HT...",
            "..............LTO.OHTO..",
            "..............LTOOOTTO..",
            ".............OTLHHHHHO..",
            ".............OTLHOOHHT..",
            ".............OTTHOOHHOOO",
            ".............OHHHHHHSSOO",
            ".............OHHHOOOSSSO",
            "............OOHHHOOSTL..",
            "............OOOHHHOHHT..",
            "............OSOHHHHOOO..",
            "...........OOOSLHHHHHO..",
            "..........OSSSSLLSHHHO..",
            "..........OSSSSTLS.HHO..",
            ".........OOOSHHHHS..H...",
            ".........OSSSHHHHH.HHG..",
            "........OSSSSTTHHTHTTG..",
            "........OSHHOSHHHTHTHG..",
            "....TSOOOLHHHOOHHSSHHG..",
            "...OHLSOHHHHHOOHHTSHHG..",
            "....OST.HHHHOOOHHOOHHG..",
            ".....OOOHHHHHHOHHHOHHHO.",
            ".........OOOOOOOOOOOOOO.",
        },
        {
            "..............OO...OO...",
            "..............TS...HT...",
            "..............LTO.OHTO..",
            "..............LTOOOTTO..",
            ".............OTLHHHHHO..",
            ".............OTLHOOHHT..",
            ".............OTTHOOHHOOO",
            ".............OHHHHHHSSOO",
            ".............OHHHOOOSSSO",
            "............OOHHHOOSTL..",
            "............OOOHHHOHHT..",
            "............OSOHHHHOOO..",
            "...........OOOSLHHHHHO..",
            "..........OSSSSLLSHHHO..",
            "..........OSSSSTLS.HHO..",
            ".........OOOSHHHHS..H...",
            ".........OSSSHHHHH.HHG..",
            "........OSSSSTTHHTHTTG..",
            "........OSHHOSHHHTHTHG..",
            "..TSOO..OLHHHOOHHSSHHG..",
            "..OHLSO.HHHHHOOHHTSHHG..",
            "...OST..HHHHOOOHHOOHHG..",
            ".....OOOHHHHHHOHHHOHHHO.",
            ".........OOOOOOOOOOOOOO.",
        },
    };

    const uint16_t outline = TFTDisplay::rgb565(0x18, 0x18, 0x1C);
    const uint16_t saddle = TFTDisplay::rgb565(0x4A, 0x49, 0x46);
    const uint16_t tan = TFTDisplay::rgb565(0xA7, 0x74, 0x55);
    const uint16_t tanHi = TFTDisplay::rgb565(0xD4, 0xA5, 0x80);
    const uint16_t chest = TFTDisplay::rgb565(0xF8, 0xF7, 0xF8);
    const uint16_t tongue = TFTDisplay::rgb565(0xBC, 0x55, 0x51);
    const uint16_t ground = TFTDisplay::rgb565(0xC6, 0xB8, 0xBB);
    const uint16_t bg = TFTDisplay::rgb565(0x00, 0x00, 0x00);
    const uint8_t tailPhase = static_cast<uint8_t>(dogFrame % 6U);

    HermesXHomeNeonClockLayout clockLayout;
    const bool hasClockRegion = HermesXHomeUiRenderer::makeNeonClockLayout(displayW, 0, clockLayout);
    const int16_t dogX = 2;
    const int16_t dogY =
        hasClockRegion
            ? std::max<int16_t>(0,
                                clockLayout.regionY + std::max<int16_t>(0, (clockLayout.regionHeight - kDogDrawH) / 2))
                       : std::max<int16_t>(0, (displayH - kDogDrawH) / 2);
    auto &homeUiModel = HermesXHomeUiModel::instance();
    const HermesXHomeDogCache previousDog = homeUiModel.dogCache();
    const int16_t previousDogX = previousDog.x;
    const int16_t previousDogY = previousDog.y;
    const int16_t previousDogW = previousDog.width;
    const int16_t previousDogH = previousDog.height;
    const bool fullDogRepaint = !previousDog.valid || previousDogX != dogX || previousDogY != dogY ||
                                previousDogW != kDogDrawW || previousDogH != kDogDrawH || previousDog.pose != dogPose;
    int16_t clearX = dogX;
    int16_t clearY = dogY;
    int16_t clearX2 = dogX + kDogDrawW;
    int16_t clearY2 = dogY + kDogDrawH;
    if (fullDogRepaint && previousDog.valid && previousDogW > 0 && previousDogH > 0) {
        clearX = std::min<int16_t>(clearX, previousDogX);
        clearY = std::min<int16_t>(clearY, previousDogY);
        clearX2 = std::max<int16_t>(clearX2, static_cast<int16_t>(previousDogX + previousDogW));
        clearY2 = std::max<int16_t>(clearY2, static_cast<int16_t>(previousDogY + previousDogH));
    }
    clearX = std::max<int16_t>(0, clearX);
    clearY = std::max<int16_t>(0, clearY);
    clearX2 = std::min<int16_t>(displayW, clearX2);
    clearY2 = std::min<int16_t>(displayH, clearY2);
    const int16_t clearW = clearX2 - clearX;
    const int16_t clearH = clearY2 - clearY;
    if (fullDogRepaint && clearW > 0 && clearH > 0) {
        tft->fillRect565(clearX, clearY, clearW, clearH, bg);
        tft->overlayBufferForegroundRect565(clearX, clearY, clearW, clearH);
    }

    DirectDrawClipRect clip{};
    if (!makeDirectDrawClipRect(dogX, dogY, kDogDrawW, kDogDrawH, displayW, displayH, clip)) {
        return;
    }

    const uint8_t spriteFrame = (dogPose == 0) ? static_cast<uint8_t>(tailPhase % kDogLyingFrameCount)
                                               : static_cast<uint8_t>(tailPhase % kDogSittingFrameCount);
    const char *const *sprite = (dogPose == 0) ? kDogLyingFrames[spriteFrame] : kDogSittingFrames[spriteFrame];
    const char *const *previousSprite = nullptr;
    if (!fullDogRepaint) {
        const uint8_t previousTailPhase = static_cast<uint8_t>(previousDog.frame % 6U);
        const uint8_t previousSpriteFrame =
            (dogPose == 0) ? static_cast<uint8_t>(previousTailPhase % kDogLyingFrameCount)
                           : static_cast<uint8_t>(previousTailPhase % kDogSittingFrameCount);
        previousSprite = (dogPose == 0) ? kDogLyingFrames[previousSpriteFrame] : kDogSittingFrames[previousSpriteFrame];
    }
    auto colorFor = [&](char token) -> uint16_t {
        switch (token) {
        case 'O':
            return outline;
        case 'H':
            return tanHi;
        case 'S':
            return saddle;
        case 'T':
            return tan;
        case 'L':
            return tongue;
        case 'W':
            return chest;
        case 'G':
            return ground;
        default:
            return bg;
        }
    };

    for (int16_t sy = 0; sy < kDogSpriteH; ++sy) {
        const char *row = sprite[sy];
        const char *previousRow = previousSprite ? previousSprite[sy] : nullptr;
        for (int16_t sx = 0; sx < kDogSpriteW; ++sx) {
            const char token = row[sx];
            if (previousRow && previousRow[sx] == token) {
                continue;
            }
            if (token == '.') {
                if (previousRow) {
                    directFillRect565Clipped(tft,
                                             displayW,
                                             displayH,
                                             dogX + (sx * kDogScale),
                                             dogY + (sy * kDogScale),
                                             kDogScale,
                                             kDogScale,
                                             bg,
                                             &clip);
                }
                continue;
            }
            directFillRect565Clipped(tft,
                                     displayW,
                                     displayH,
                                     dogX + (sx * kDogScale),
                                     dogY + (sy * kDogScale),
                                     kDogScale,
                                     kDogScale,
                                     colorFor(token),
                                     &clip);
        }
    }

    homeUiModel.commitDogBounds(dogX, dogY, kDogDrawW, kDogDrawH);
}

} // namespace graphics
