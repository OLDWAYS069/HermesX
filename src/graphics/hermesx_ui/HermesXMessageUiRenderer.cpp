#include "HermesXMessageUiRenderer.h"

#include "HermesXMessageUiModel.h"
#include "HermesXFastSetupUiRenderer.h"
#include "HermesXHomeUiRenderer.h"
#include "HermesXTextLayout.h"
#include "graphics/ScreenFonts.h"
#include "graphics/fonts/HermesX_zh/HermesX_CN12.h"
#include "configuration.h"
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
#include "graphics/TFTDisplay.h"
#endif
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace graphics
{
namespace
{

size_t messageUtf8SequenceLength(uint8_t firstByte)
{
    if ((firstByte & 0x80u) == 0)
        return 1;
    if ((firstByte & 0xE0u) == 0xC0u)
        return 2;
    if ((firstByte & 0xF0u) == 0xE0u)
        return 3;
    if ((firstByte & 0xF8u) == 0xF0u)
        return 4;
    return 1;
}

size_t countMessageCodepoints(const char *text, size_t bytes)
{
    size_t count = 0;
    for (size_t offset = 0; text && offset < bytes && text[offset] != '\0'; ++count) {
        const size_t sequenceLength = messageUtf8SequenceLength(static_cast<uint8_t>(text[offset]));
        if (offset + sequenceLength > bytes)
            break;
        offset += sequenceLength;
    }
    return count;
}

void buildMessageMarqueeWindow(const char *text, uint32_t nowMs, char *output, size_t outputSize)
{
    if (!text || !output || outputSize == 0) {
        return;
    }
    output[0] = '\0';
    const size_t textBytes = strlen(text);
    const size_t codepointCount = countMessageCodepoints(text, textBytes);
    if (codepointCount == 0) {
        return;
    }
    constexpr size_t gapCodepoints = 3;
    const size_t marqueeLength = codepointCount + gapCodepoints;
    size_t sourceIndex = (nowMs / 400U) % marqueeLength;
    size_t sourceOffset = 0;
    for (size_t index = 0; index < sourceIndex && index < codepointCount; ++index) {
        sourceOffset += messageUtf8SequenceLength(static_cast<uint8_t>(text[sourceOffset]));
    }
    size_t outputBytes = 0;
    for (size_t position = 0; position < marqueeLength; ++position) {
        if (sourceIndex >= codepointCount) {
            if (outputBytes + 1 >= outputSize)
                break;
            output[outputBytes++] = ' ';
            output[outputBytes] = '\0';
        } else {
            const size_t sequenceLength = messageUtf8SequenceLength(static_cast<uint8_t>(text[sourceOffset]));
            if (sourceOffset + sequenceLength > textBytes)
                break;
            if (outputBytes + sequenceLength >= outputSize)
                break;
            memcpy(output + outputBytes, text + sourceOffset, sequenceLength);
            outputBytes += sequenceLength;
            output[outputBytes] = '\0';
            sourceOffset += sequenceLength;
        }
        if (++sourceIndex == marqueeLength) {
            sourceIndex = 0;
            sourceOffset = 0;
        }
    }
}

} // namespace

void HermesXMessageUiRenderer::drawComposerSending(
    OLEDDisplay *display, int16_t width, int16_t height, uint32_t elapsedMs, bool failed)
{
#if defined(HERMESX_NRF52840_PROMICRO)
    HermesXHomeUiRenderer::drawHermesOneParcelNotification(
        display, width, height,
        failed ? HermesXParcelAnimationMode::Failed : HermesXParcelAnimationMode::Outgoing, elapsedMs);
#else
    (void)elapsedMs;
    (void)failed;
    HermesXHomeUiRenderer::drawSittingDogNotification(display, width, height, u8"送出啦", millis());
#endif
}

uint8_t HermesXMessageUiRenderer::bodyHanziPixelSize()
{
    return static_cast<uint8_t>(std::min<int>(18, std::max<int>(14, FONT_HEIGHT_MEDIUM - 3)));
}

void HermesXMessageUiRenderer::drawRecentList(OLEDDisplay *display,
                                               OLEDDisplayUiState *state,
                                               int16_t x,
                                               int16_t y,
                                               const HermesXMessageRenderContext &context)
{
    (void)state;
    if (!display) {
        return;
    }

    auto &model = HermesXMessageUiModel::instance();
    auto &recent = model.recentState();
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
    if (context.inverted) {
        display->fillRect(x, y, x + display->getWidth(), y + FONT_HEIGHT_SMALL);
        display->setColor(BLACK);
    }
    display->drawString(x, y, "Recent Send");
    display->setColor(WHITE);

    model.clampRecentIndices();
    const int16_t width = display->getWidth();
    const bool compact64 = width <= 128 && display->getHeight() <= 64;
    const int16_t rowH = compact64 ? HermesX_zh::GLYPH_HEIGHT : FONT_HEIGHT_SMALL + 3;
    const int16_t listTop = y + FONT_HEIGHT_SMALL + (compact64 ? 1 : 2);
    int8_t visibleRows = (display->getHeight() - listTop) / rowH;
    visibleRows = std::max<int8_t>(1, std::min<int8_t>(visibleRows, 4));

    const uint8_t totalRows = model.recentListEntryCount();
    uint8_t startCursor = 0;
    if (recent.listCursor >= static_cast<uint8_t>(visibleRows)) {
        startCursor = recent.listCursor - static_cast<uint8_t>(visibleRows) + 1;
    }

    for (int8_t row = 0; row < visibleRows; ++row) {
        const uint8_t cursorIndex = startCursor + row;
        if (cursorIndex >= totalRows) {
            break;
        }
        const int16_t rowY = listTop + row * rowH;
        if (cursorIndex == recent.listCursor) {
            display->drawRect(x, rowY - 1, width - 2, rowH);
        }
        if (cursorIndex == 0) {
            HermesX_zh::drawMixedBounded(*display, x + 2, rowY, width - 4, u8"返回", HermesX_zh::GLYPH_WIDTH, rowH, nullptr);
            continue;
        }

        const meshtastic_MeshPacket *packet = model.recentMessageAt(cursorIndex - 1);
        if (!packet) {
            continue;
        }
        meshtastic_NodeInfoLite *node = context.resolveNode ? context.resolveNode(*packet) : nullptr;
        char senderBuf[24] = {0};
        if (node && node->has_user) {
            const char *name = node->user.short_name[0] != 0 ? node->user.short_name : node->user.long_name;
            const size_t nameSize = node->user.short_name[0] != 0 ? sizeof(node->user.short_name) : sizeof(node->user.long_name);
            HermesXTextLayout::copyUtf8Snippet(name, strnlen(name, nameSize), senderBuf, sizeof(senderBuf), 8);
        }
        if (senderBuf[0] == 0) {
            std::snprintf(senderBuf, sizeof(senderBuf), "???");
        }

        const char *channelName = context.channelName ? context.channelName(packet->channel) : "";
        String line(senderBuf);
        line.reserve(line.length() + packet->decoded.payload.size + (channelName ? strlen(channelName) : 0) + 8);
        line += " : ";
        for (size_t byte = 0; byte < packet->decoded.payload.size; ++byte) {
            const char value = static_cast<char>(packet->decoded.payload.bytes[byte]);
            line += (value == '\r' || value == '\n' || value == '\t') ? ' ' : value;
        }
        line += " -";
        line += (channelName && channelName[0]) ? channelName : "?";

        const int16_t lineX = x + 3;
        const int16_t lineWidth = std::max<int16_t>(1, width - 7);
        char marquee[320];
        const char *visibleLine = line.c_str();
        if (HermesX_zh::stringAdvance(visibleLine, HermesX_zh::GLYPH_WIDTH, display) > lineWidth) {
            buildMessageMarqueeWindow(visibleLine, millis(), marquee, sizeof(marquee));
            visibleLine = marquee;
        }
        HermesXTextLayout::drawMixedSingleLineBounded(
            display, lineX, rowY, lineWidth, visibleLine, rowH, HermesX_zh::GLYPH_WIDTH);
    }

    if (!model.hasRecentMessages() && visibleRows > 1) {
        display->drawString(x + 2, listTop + rowH, "No messages");
    }
}

void HermesXMessageUiRenderer::drawIncomingPopup(OLEDDisplay *display,
                                                  OLEDDisplayUiState *state,
                                                  const HermesXMessageRenderContext &context)
{
    (void)state;
    if (!display) {
        return;
    }
    auto &popup = HermesXMessageUiModel::instance().popupState();
    const meshtastic_MeshPacket &packet = popup.packet;
    if (packet.from == 0) {
        return;
    }

#if defined(HERMESX_NRF52840_PROMICRO)
    const uint32_t elapsedMs = popup.startedAtMs == 0 ? 0U : static_cast<uint32_t>(millis() - popup.startedAtMs);
    char snippet[88] = {0};
    char marquee[88] = {0};
    const char *message = nullptr;
    if (elapsedMs >= 1800U) {
        const char *payload = reinterpret_cast<const char *>(packet.decoded.payload.bytes);
        const size_t payloadBytes = packet.decoded.payload.size;
        HermesXTextLayout::copyUtf8Snippet(payload, payloadBytes, snippet, sizeof(snippet), 20);
        if (countMessageCodepoints(payload, payloadBytes) > 20U) {
            const size_t used = strlen(snippet);
            if (used + 3U < sizeof(snippet)) {
                memcpy(snippet + used, "...", 4);
            }
        }
        message = snippet;
        if (HermesX_zh::stringAdvance(snippet, HermesX_zh::GLYPH_WIDTH, display) > 70) {
            buildMessageMarqueeWindow(snippet, elapsedMs - 1800U, marquee, sizeof(marquee));
            message = marquee;
        }
    }
    HermesXHomeUiRenderer::drawHermesOneParcelNotification(
        display, display->getWidth(), display->getHeight(), HermesXParcelAnimationMode::Incoming,
        elapsedMs, true, popup.selectedOption, message);
#else
    HermesXHomeUiRenderer::drawSittingDogNotification(
        display, display->getWidth(), display->getHeight(), u8"你有新訊息！", millis(), true, popup.selectedOption);
#endif
    return;

    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const bool compactLayout = width < 200 || height < 120;
    const bool largePopupLayout = height >= 110;
    const int16_t boxX = 3;
    const int16_t boxY = 2;
    const int16_t boxW = width - 6;
    const int16_t boxH = height - 4;
#if defined(ST7735_CS) || defined(ILI9341_DRIVER) || defined(ILI9342_DRIVER) || defined(ST7701_CS) || defined(ST7789_CS) ||       \
    defined(RAK14014) || defined(HX8357_CS) || defined(ILI9488_CS)
    auto *tft = static_cast<TFTDisplay *>(display);
    tft->clearColorPaletteZones();
    tft->setColorPaletteDefaults(0xFFFF, 0x0000);
#endif

    display->setFont(largePopupLayout ? FONT_MEDIUM : FONT_SMALL_LOCAL);
    const uint8_t hanziScale = largePopupLayout ? 2 : 1;
    const int16_t lineHeight = std::max<int16_t>(largePopupLayout ? FONT_HEIGHT_MEDIUM : _fontHeight(FONT_SMALL_LOCAL),
                                                 HermesX_zh::GLYPH_HEIGHT * hanziScale);
    const int16_t titleBarH = lineHeight;
    const int16_t optionH = lineHeight + 2;
    const int16_t optionY = boxY + boxH - optionH - 2;
#if defined(USE_EINK)
    const auto dialogBg = EINK_WHITE;
    const auto dialogFg = EINK_BLACK;
#else
    const auto dialogBg = OLEDDISPLAY_COLOR::WHITE;
    const auto dialogFg = OLEDDISPLAY_COLOR::BLACK;
#endif

    display->setColor(dialogBg);
    display->fillRect(boxX, boxY, boxW, boxH);
    display->setColor(dialogFg);
    display->drawRect(boxX, boxY, boxW, boxH);
    display->fillRect(boxX + 1, boxY + 1, boxW - 2, titleBarH);
    display->setColor(dialogBg);

    meshtastic_NodeInfoLite *node = context.resolveNode ? context.resolveNode(packet) : nullptr;
    char shortId[16] = {0};
    if (node && node->has_user && node->user.short_name[0] != 0) {
        HermesXTextLayout::copyUtf8Snippet(node->user.short_name, strnlen(node->user.short_name, sizeof(node->user.short_name)),
                                           shortId, sizeof(shortId), 6);
    }
    if (shortId[0] == 0) {
        std::snprintf(shortId, sizeof(shortId), "%04lx", static_cast<unsigned long>(packet.from & 0xFFFFu));
    }

    char snippetBuf[96];
    HermesXTextLayout::copyUtf8Snippet(reinterpret_cast<const char *>(packet.decoded.payload.bytes), packet.decoded.payload.size,
                                       snippetBuf, sizeof(snippetBuf), compactLayout ? 18 : 36);
    if (snippetBuf[0] == 0) {
        std::snprintf(snippetBuf, sizeof(snippetBuf), "%s", u8"(空白訊息)");
    }

    const char *title = "NEW MSG";
    const int titleW = HermesX_zh::stringAdvance(title, HermesX_zh::GLYPH_WIDTH, display);
    const int16_t titleX = std::max<int16_t>(boxX + 2, boxX + (boxW - titleW) / 2);
    HermesXTextLayout::drawLargeMixedLine(*display, titleX, boxY + 1, boxW - 4, title, hanziScale);
    display->setColor(dialogFg);
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    const int16_t bodyX = boxX + 5;
    const int16_t bodyW = boxW - 10;
    const int16_t bodyY = boxY + titleBarH + 1;
    const int advance = HermesX_zh::GLYPH_WIDTH * hanziScale;
    const String fromLine = String("FROM \"") + shortId + "\"";
    HermesXTextLayout::drawLargeMixedLine(*display, bodyX, bodyY, bodyW, fromLine.c_str(), hanziScale);

    const int16_t snippetY = bodyY + lineHeight + 1;
    const int16_t snippetH = std::max<int16_t>(0, optionY - snippetY - 1);
    const std::vector<String> snippetLines =
        HermesXTextLayout::buildMixedWrappedLines(display, snippetBuf, bodyW, advance);
    if (snippetH >= HermesX_zh::GLYPH_HEIGHT * hanziScale) {
        HermesXTextLayout::drawVisibleWrappedLines(display, snippetLines, bodyX, snippetY, bodyW, snippetH, lineHeight, 0,
                                                   advance, hanziScale);
    }

    display->setColor(dialogFg);
    const int16_t optionW = (boxW - 13) / 2;
    const int16_t viewX = boxX + 4;
    const int16_t skipX = viewX + optionW + 5;
    auto drawOption = [&](int16_t optionX, const char *label, bool selected) {
        if (selected) {
            display->fillRect(optionX, optionY, optionW, optionH);
            display->setColor(dialogBg);
        } else {
            display->drawRect(optionX, optionY, optionW, optionH);
            display->setColor(dialogFg);
        }
        const int textW = HermesX_zh::stringAdvance(label, advance, display);
        const int16_t textX = std::max<int16_t>(optionX + 1, optionX + (optionW - textW) / 2);
        const int16_t textY = optionY + std::max<int16_t>(1, (optionH - lineHeight) / 2);
        HermesXTextLayout::drawLargeMixedLine(*display, textX, textY, optionW - 2, label, hanziScale);
        display->setColor(dialogFg);
    };
    drawOption(viewX, u8"查看", popup.selectedOption == 0);
    drawOption(skipX, u8"略過", popup.selectedOption == 1);
    display->setColor(WHITE);
}

void HermesXMessageUiRenderer::drawDetail(OLEDDisplay *display,
                                           OLEDDisplayUiState *state,
                                           int16_t x,
                                           int16_t y,
                                           const meshtastic_MeshPacket *packet,
                                           const char *timeLabel,
                                           const HermesXMessageRenderContext &context)
{
    (void)state;
    if (!display) {
        return;
    }
    if (!packet) {
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->setFont(FONT_SMALL);
        display->drawString(x, y, "No message");
        return;
    }

    auto &recent = HermesXMessageUiModel::instance().recentState();
    meshtastic_NodeInfoLite *node = context.resolveNode ? context.resolveNode(*packet) : nullptr;
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
    const int16_t width = display->getWidth();
    const int16_t height = display->getHeight();
    const bool compact64 = width <= 128 && height <= 64;
    const int16_t headerLineHeight = FONT_HEIGHT_SMALL;
    const int16_t dividerY = compact64 ? y + headerLineHeight + 1 : y + headerLineHeight * 2 + 2;
    const int16_t bodyTop = dividerY + 2;
    const int16_t scrollbarW = 4;
    const int16_t bodyW = std::max<int16_t>(width - scrollbarW - 3, 12);
    const int16_t bodyH = std::max<int16_t>(height - bodyTop - 1, headerLineHeight);
    const uint8_t hanziSize = compact64 ? HermesX_zh::GLYPH_HEIGHT : bodyHanziPixelSize();
    const int lineHeight = compact64 ? FONT_HEIGHT_SMALL + 1 : std::max<int>(FONT_HEIGHT_MEDIUM, hanziSize) + 2;

    if (context.inverted) {
        display->fillRect(x, y, width, dividerY - y);
        display->setColor(BLACK);
    }

    char senderBuf[24] = {0};
    if (node && node->has_user) {
        const char *name = node->user.short_name[0] != 0 ? node->user.short_name : node->user.long_name;
        const size_t nameSize = node->user.short_name[0] != 0 ? sizeof(node->user.short_name) : sizeof(node->user.long_name);
        HermesXTextLayout::copyUtf8Snippet(name, strnlen(name, nameSize), senderBuf, sizeof(senderBuf), 10);
    }
    if (senderBuf[0] == 0) {
        std::snprintf(senderBuf, sizeof(senderBuf), "???");
    }

    if (compact64) {
        const char *timestamp = timeLabel ? timeLabel : "";
        const int16_t timeW = display->getStringWidth(timestamp);
        const int16_t timeX = std::max<int16_t>(x, x + width - timeW - 1);
        const int16_t senderW = std::max<int16_t>(12, timeX - x - 3);
        HermesX_zh::drawMixedBounded(*display, x, y, senderW, senderBuf, HermesX_zh::GLYPH_WIDTH,
                                     headerLineHeight, nullptr);
        display->drawString(timeX, y, timestamp);
    } else {
        HermesX_zh::drawMixedBounded(*display, x, y, width - 1, senderBuf, HermesX_zh::GLYPH_WIDTH,
                                     headerLineHeight, nullptr);
        HermesX_zh::drawMixedBounded(*display, x, y + headerLineHeight, width - 1, timeLabel ? timeLabel : "",
                                     HermesX_zh::GLYPH_WIDTH, headerLineHeight, nullptr);
    }
    display->drawLine(x, dividerY, x + width - 1, dividerY);
    display->setColor(WHITE);

    static char payload[237];
    const size_t payloadLen = std::min<size_t>(packet->decoded.payload.size, sizeof(payload) - 1);
    std::memcpy(payload, packet->decoded.payload.bytes, payloadLen);
    payload[payloadLen] = 0;
    display->setFont(compact64 ? FONT_SMALL : FONT_MEDIUM);
    const std::vector<String> bodyLines = HermesXTextLayout::buildMixedWrappedLines(display, payload, bodyW, hanziSize);
    const uint16_t contentHeight = static_cast<uint16_t>(std::max<size_t>(1, bodyLines.size()) * lineHeight);
    const uint16_t visibleLineCount = static_cast<uint16_t>(std::max<int>(1, bodyH / lineHeight));
    recent.detailMaxScrollY =
        bodyLines.size() > visibleLineCount ? static_cast<uint16_t>((bodyLines.size() - visibleLineCount) * lineHeight) : 0;
    recent.detailScrollY = std::min<uint16_t>(recent.detailScrollY, recent.detailMaxScrollY);

    const bool drewIcon = context.drawEmoji ? context.drawEmoji(display, x, y, width, height, payload) : false;
    if (!drewIcon) {
        HermesXTextLayout::drawVisibleWrappedLines(display, bodyLines, x, bodyTop, bodyW, bodyH, lineHeight,
                                                   recent.detailScrollY, hanziSize, 1, hanziSize);
    }

    if (recent.detailMaxScrollY > 0) {
        const int16_t trackX = x + width - scrollbarW;
        display->drawRect(trackX, bodyTop, scrollbarW, bodyH);
        const int16_t thumbH = std::max<int16_t>(6, static_cast<int16_t>((static_cast<int32_t>(bodyH) * bodyH) / contentHeight));
        const int16_t thumbTravel = std::max<int16_t>(bodyH - thumbH - 2, 0);
        const int16_t thumbY = bodyTop + 1 + static_cast<int16_t>((static_cast<int32_t>(thumbTravel) * recent.detailScrollY) /
                                                                  recent.detailMaxScrollY);
        if (context.inverted) {
            display->fillRect(trackX + 1, thumbY, scrollbarW - 2, thumbH);
            display->setColor(BLACK);
            display->fillRect(trackX + 1, thumbY, scrollbarW - 2, thumbH);
            display->setColor(WHITE);
        } else {
            display->fillRect(trackX + 1, thumbY, scrollbarW - 2, thumbH);
        }
    }
}

void HermesXMessageUiRenderer::drawComposerCandidates(OLEDDisplay *display,
                                                        int16_t width,
                                                        int16_t height,
                                                        const String &preview,
                                                        const std::vector<std::string> &candidates,
                                                        uint8_t candidateCursor)
{
    if (!display) {
        return;
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);
#if defined(USE_EINK)
    display->setColor(EINK_WHITE);
#else
    display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
    display->fillRect(0, 0, width, height);
    HermesXFastSetupUiRenderer::drawHeader(display, width, u8"注音候選");
#if defined(USE_EINK)
    display->setColor(EINK_BLACK);
#else
    display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
    display->setFont(FONT_SMALL);

    HermesXTextLayout::drawMixedSingleLineBounded(display, 2, 14 + 1, width - 4, preview.c_str(), FONT_HEIGHT_SMALL);

    const uint8_t totalRows = static_cast<uint8_t>(std::min<size_t>(candidates.size() + 1, 255));
    const int16_t listTop = 14 + FONT_HEIGHT_SMALL + 3;
    const int16_t rowHeight = 15;
    const uint8_t visibleRows = static_cast<uint8_t>(std::max<int16_t>(1, (height - listTop) / rowHeight));
    uint8_t start = 0;
    if (candidateCursor >= visibleRows) {
        start = candidateCursor - visibleRows + 1;
    }

    for (uint8_t row = 0; row < visibleRows; ++row) {
        const uint8_t item = start + row;
        if (item >= totalRows) {
            break;
        }
        const int16_t rowY = listTop + row * rowHeight;
        const bool selected = item == candidateCursor;
        if (selected) {
            display->fillRect(0, rowY, width, rowHeight);
#if defined(USE_EINK)
            display->setColor(EINK_WHITE);
#else
            display->setColor(OLEDDISPLAY_COLOR::BLACK);
#endif
        }

        String label;
        if (item == 0) {
            label = u8"返回鍵盤";
        } else {
            label = String(item) + ". " + candidates[item - 1].c_str();
        }
        HermesXTextLayout::drawMixedSingleLineBounded(display, 3, rowY + 1, width - 6, label.c_str(), rowHeight);
        if (selected) {
#if defined(USE_EINK)
            display->setColor(EINK_BLACK);
#else
            display->setColor(OLEDDISPLAY_COLOR::WHITE);
#endif
        }
    }
}

} // namespace graphics
