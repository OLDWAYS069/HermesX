#include "HermesXMessageUiRenderer.h"

#include "HermesXMessageUiModel.h"
#include "HermesXFastSetupUiRenderer.h"
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
    const int16_t rowH = FONT_HEIGHT_SMALL + 3;
    const int16_t listTop = y + FONT_HEIGHT_SMALL + 2;
    int8_t visibleRows = (display->getHeight() - listTop - 1) / rowH;
    visibleRows = std::max<int8_t>(1, std::min<int8_t>(visibleRows, 3));

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

        char preview[40];
        HermesXTextLayout::copyUtf8Snippet(reinterpret_cast<const char *>(packet->decoded.payload.bytes),
                                           packet->decoded.payload.size, preview, sizeof(preview), 14);
        const char *channelName = context.channelName ? context.channelName(packet->channel) : "";
        char channelBuf[20];
        std::snprintf(channelBuf, sizeof(channelBuf), "#%s", channelName ? channelName : "");
        const int16_t channelW = display->getStringWidth(channelBuf);
        const int16_t channelX = x + width - channelW - 2;
        display->drawString(channelX, rowY, channelBuf);

        char lineBuf[96];
        std::snprintf(lineBuf, sizeof(lineBuf), "%s: %s", senderBuf, preview);
        const int16_t textWidth = channelX - x - 4;
        HermesX_zh::drawMixedBounded(*display, x + 2, rowY, textWidth > 0 ? textWidth : width - 4, lineBuf,
                                     HermesX_zh::GLYPH_WIDTH, rowH, nullptr);
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
    const int16_t headerLineHeight = FONT_HEIGHT_SMALL;
    const int16_t dividerY = y + headerLineHeight * 2 + 2;
    const int16_t bodyTop = dividerY + 2;
    const int16_t scrollbarW = 4;
    const int16_t bodyW = std::max<int16_t>(width - scrollbarW - 3, 12);
    const int16_t bodyH = std::max<int16_t>(height - bodyTop - 1, headerLineHeight);
    const uint8_t hanziSize = bodyHanziPixelSize();
    const int lineHeight = std::max<int>(FONT_HEIGHT_MEDIUM, hanziSize) + 2;

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

    HermesX_zh::drawMixedBounded(*display, x, y, width - 1, senderBuf, HermesX_zh::GLYPH_WIDTH, headerLineHeight, nullptr);
    HermesX_zh::drawMixedBounded(*display, x, y + headerLineHeight, width - 1, timeLabel ? timeLabel : "",
                                 HermesX_zh::GLYPH_WIDTH, headerLineHeight, nullptr);
    display->drawLine(x, dividerY, x + width - 1, dividerY);
    display->setColor(WHITE);

    static char payload[237];
    const size_t payloadLen = std::min<size_t>(packet->decoded.payload.size, sizeof(payload) - 1);
    std::memcpy(payload, packet->decoded.payload.bytes, payloadLen);
    payload[payloadLen] = 0;
    display->setFont(FONT_MEDIUM);
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

    graphics::HermesX_zh::drawMixedBounded(*display, 2, 14 + 1, width - 4, preview.c_str(),
                                           graphics::HermesX_zh::GLYPH_WIDTH, FONT_HEIGHT_SMALL, nullptr);

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
        graphics::HermesX_zh::drawMixedBounded(*display, 3, rowY + 1, width - 6, label.c_str(),
                                               graphics::HermesX_zh::GLYPH_WIDTH, rowHeight, nullptr);
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
