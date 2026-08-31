#include "HermesXDirectMessageComposer.h"

#include "input/InputBroker.h"
#include "mesh/generated/meshtastic/module_config.pb.h"
#include <cstring>

namespace graphics
{
namespace
{

using InputChar = meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar;

const char *kKeyRowsUpper[][10] = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L", "Aa"},
    {"Z", "X", "C", "V", "B", "N", "M", ".", "-", "_"},
    {"EXIT", "SP", "DEL", u8"中", "OK", nullptr, nullptr, nullptr, nullptr, nullptr},
};
const char *kKeyRowsLower[][10] = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
    {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
    {"a", "s", "d", "f", "g", "h", "j", "k", "l", "Aa"},
    {"z", "x", "c", "v", "b", "n", "m", ".", "-", "_"},
    {"EXIT", "SP", "DEL", u8"中", "OK", nullptr, nullptr, nullptr, nullptr, nullptr},
};
const char *kKeyRowsBopomofo[][10] = {
    {u8"ㄅ", u8"ㄉ", u8"ˇ", u8"ˋ", u8"ㄓ", u8"ˊ", u8"ㄚ", u8"ㄞ", u8"ㄢ", u8"ㄦ"},
    {u8"ㄆ", u8"ㄊ", u8"ㄍ", u8"ㄐ", u8"ㄔ", u8"ㄗ", u8"ㄧ", u8"ㄛ", u8"ㄟ", u8"ㄣ"},
    {u8"ㄇ", u8"ㄋ", u8"ㄎ", u8"ㄑ", u8"ㄕ", u8"ㄘ", u8"ㄨ", u8"ㄜ", u8"ㄠ", u8"ㄤ"},
    {u8"ㄈ", u8"ㄌ", u8"ㄏ", u8"ㄒ", u8"ㄖ", u8"ㄙ", u8"ㄩ", u8"ㄝ", u8"ㄡ", u8"ㄥ"},
    {"EXIT", "SP", "DEL", "Aa", u8"˙", "EN", "OK", nullptr, nullptr, nullptr},
};
const char kBopomofoScreenKeys[][10] = {
    {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'},
    {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p'},
    {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';'},
    {'z', 'x', 'c', 'v', 'b', 'n', 'm', '.', ',', '?'},
};
const uint8_t kRowLengthsEnglish[] = {10, 10, 10, 10, 5};
const uint8_t kRowLengthsBopomofo[] = {10, 10, 10, 10, 7};
constexpr uint8_t kRowCount = sizeof(kRowLengthsEnglish) / sizeof(kRowLengthsEnglish[0]);

bool matches(const HermesXComposerInput &event, InputChar input)
{
    return event.code == static_cast<char>(input);
}

bool isRotary(const HermesXComposerInput &event)
{
    return event.source && std::strncmp(event.source, "rotEnc", 6) == 0;
}

} // namespace

HermesXDirectMessageComposer &HermesXDirectMessageComposer::instance()
{
    static HermesXDirectMessageComposer composer;
    return composer;
}

void HermesXDirectMessageComposer::resetState()
{
    draft_ = "";
    toast_ = "";
    toastUntilMs_ = 0;
    keyRow_ = 0;
    keyCol_ = 0;
    lowercase_ = false;
    bopomofoMode_ = false;
    candidateMode_ = false;
    candidateCursor_ = 0;
    bpmf_ = hermesx_bpmf::HermesXBpmfEngine();
}

void HermesXDirectMessageComposer::start(uint32_t destination, bool fromGroupDetail)
{
    active_ = true;
    fromGroupDetail_ = fromGroupDetail;
    destination_ = destination;
    resetState();
    bpmf_.setMaxCandidates(8);
}

void HermesXDirectMessageComposer::stop()
{
    active_ = false;
    fromGroupDetail_ = false;
    destination_ = 0;
    resetState();
}

String HermesXDirectMessageComposer::compositionText() const
{
    if (!bopomofoMode_ || !bpmf_.composing()) {
        return String();
    }
    return String(bpmf_.composingText().c_str());
}

String HermesXDirectMessageComposer::previewText() const
{
    String preview = draft_;
    preview += compositionText();
    preview += "_";
    return preview;
}

const char *const (*HermesXDirectMessageComposer::keyRows() const)[10]
{
    if (bopomofoMode_) {
        return kKeyRowsBopomofo;
    }
    return lowercase_ ? kKeyRowsLower : kKeyRowsUpper;
}

const uint8_t *HermesXDirectMessageComposer::keyRowLengths() const
{
    return bopomofoMode_ ? kRowLengthsBopomofo : kRowLengthsEnglish;
}

uint8_t HermesXDirectMessageComposer::keyRowCount() const
{
    return kRowCount;
}

void HermesXDirectMessageComposer::removeLastDraftCharacter()
{
    if (draft_.length() == 0) {
        return;
    }
    int index = static_cast<int>(draft_.length()) - 1;
    while (index > 0 && (static_cast<uint8_t>(draft_[index]) & 0xC0u) == 0x80u) {
        --index;
    }
    draft_.remove(static_cast<unsigned int>(index));
}

bool HermesXDirectMessageComposer::openCandidates(uint32_t nowMs)
{
    if (!bpmf_.composing()) {
        return false;
    }
    bpmf_.refresh();
    if (!bpmf_.hasCandidates()) {
        toast_ = u8"沒有候選字";
        toastUntilMs_ = nowMs + 1200;
        return false;
    }
    candidateMode_ = true;
    candidateCursor_ = 1;
    return true;
}

HermesXComposerAction HermesXDirectMessageComposer::requestSend(uint32_t nowMs)
{
    if (draft_.length() == 0) {
        toast_ = u8"訊息是空的";
        toastUntilMs_ = nowMs + 1200;
        return HermesXComposerAction::Refresh;
    }
    return HermesXComposerAction::SendRequested;
}

void HermesXDirectMessageComposer::showSendFailure(uint32_t nowMs)
{
    toast_ = "SEND FAIL";
    toastUntilMs_ = nowMs + 1200;
}

int8_t HermesXDirectMessageComposer::navigationDirection(const HermesXComposerInput &event,
                                                          const HermesXComposerInputBindings &bindings) const
{
    const bool up = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_UP);
    const bool down = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_DOWN);
    const bool left = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool right = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool clockwise = bindings.clockwise != 0 && event.code == bindings.clockwise;
    const bool counterClockwise = bindings.counterClockwise != 0 && event.code == bindings.counterClockwise;
    if (isRotary(event)) {
        if (counterClockwise) {
            return -1;
        }
        if (clockwise) {
            return 1;
        }
        if (bindings.clockwise == 0 && bindings.counterClockwise == 0) {
            if (up || left) {
                return -1;
            }
            if (down || right) {
                return 1;
            }
        }
        return 0;
    }
    if (up || left || counterClockwise) {
        return -1;
    }
    if (down || right || clockwise) {
        return 1;
    }
    return 0;
}

HermesXComposerAction HermesXDirectMessageComposer::handleInput(const HermesXComposerInput &event,
                                                                 const HermesXComposerInputBindings &bindings,
                                                                 uint32_t nowMs,
                                                                 size_t payloadLimit)
{
    if (!active_) {
        return HermesXComposerAction::Consumed;
    }

    const bool left = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_LEFT);
    const bool right = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_RIGHT);
    const bool select = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_SELECT);
    const bool cancel = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_CANCEL);
    const bool back = matches(event, meshtastic_ModuleConfig_CannedMessageConfig_InputEventChar_BACK);
    const bool press = bindings.press != 0 && event.code == bindings.press;
    const bool rotary = isRotary(event);
    const int8_t navDir = navigationDirection(event, bindings);

    if (candidateMode_) {
        const int totalEntries = static_cast<int>(bpmf_.candidates().size()) + 1;
        if (cancel || back || (!rotary && (left || right))) {
            candidateMode_ = false;
            candidateCursor_ = 0;
            return HermesXComposerAction::Refresh;
        }
        if (navDir != 0 && totalEntries > 0) {
            int next = static_cast<int>(candidateCursor_) + navDir;
            candidateCursor_ = static_cast<uint8_t>((next + totalEntries) % totalEntries);
            return HermesXComposerAction::Refresh;
        }
        if (select || press || event.key == 0x0D || event.key == 0x0A) {
            if (candidateCursor_ == 0) {
                candidateMode_ = false;
                return HermesXComposerAction::Refresh;
            }
            const size_t candidateIndex = candidateCursor_ - 1;
            const auto &candidateList = bpmf_.candidates();
            if (candidateIndex < candidateList.size()) {
                if (draft_.length() + candidateList[candidateIndex].length() > payloadLimit) {
                    candidateMode_ = false;
                    toast_ = u8"訊息已達上限";
                    toastUntilMs_ = nowMs + 1200;
                    return HermesXComposerAction::Refresh;
                }
                draft_ += bpmf_.select(candidateIndex).c_str();
            }
            candidateMode_ = false;
            candidateCursor_ = 0;
            return HermesXComposerAction::Refresh;
        }
        return HermesXComposerAction::Consumed;
    }

    if (cancel || (!rotary && (left || right))) {
        return HermesXComposerAction::CloseRequested;
    }

    const uint8_t key = static_cast<uint8_t>(event.key);
    if (key >= 0x20 && key <= 0x7E && key != INPUT_BROKER_MSG_LEFT && key != INPUT_BROKER_MSG_RIGHT &&
        key != INPUT_BROKER_MSG_UP && key != INPUT_BROKER_MSG_DOWN) {
        if (bopomofoMode_) {
            char ascii = static_cast<char>(key);
            if (ascii == ' ') {
                if (bpmf_.composing()) {
                    bpmf_.addSpace();
                } else if (draft_.length() < payloadLimit) {
                    draft_ += ' ';
                }
                return HermesXComposerAction::Refresh;
            }
            if (ascii >= 'A' && ascii <= 'Z') {
                ascii = static_cast<char>(ascii - 'A' + 'a');
            }
            const hermesx_bpmf::HermesXBpmfSymbol *symbol = hermesx_bpmf::lookup_key(ascii);
            if (symbol) {
                bpmf_.addSymbol(*symbol);
            }
            return HermesXComposerAction::Refresh;
        }
        if (draft_.length() < payloadLimit) {
            draft_ += static_cast<char>(key);
        }
        return HermesXComposerAction::Refresh;
    }

    if (key == 0x08 || back) {
        if (bopomofoMode_ && bpmf_.composing()) {
            bpmf_.backspace();
            return HermesXComposerAction::Refresh;
        }
        if (draft_.length() > 0) {
            removeLastDraftCharacter();
            return HermesXComposerAction::Refresh;
        }
        return HermesXComposerAction::CloseRequested;
    }

    if (key == 0x0D || key == 0x0A) {
        if (bopomofoMode_ && bpmf_.composing()) {
            openCandidates(nowMs);
            return HermesXComposerAction::Refresh;
        }
        return requestSend(nowMs);
    }

    if (navDir != 0) {
        const uint8_t *rowLengths = keyRowLengths();
        int totalKeys = 0;
        int index = 0;
        for (uint8_t row = 0; row < kRowCount; ++row) {
            totalKeys += rowLengths[row];
            if (row < keyRow_) {
                index += rowLengths[row];
            }
        }
        index += keyCol_;
        index = (index + navDir + totalKeys) % totalKeys;
        for (uint8_t row = 0; row < kRowCount; ++row) {
            if (index < rowLengths[row]) {
                keyRow_ = row;
                keyCol_ = static_cast<uint8_t>(index);
                break;
            }
            index -= rowLengths[row];
        }
        return HermesXComposerAction::Refresh;
    }

    if (select || press) {
        const char *label = keyRows()[keyRow_][keyCol_];
        if (!label) {
            return HermesXComposerAction::Consumed;
        }
        if (std::strcmp(label, "OK") == 0) {
            if (bopomofoMode_ && bpmf_.composing()) {
                openCandidates(nowMs);
                return HermesXComposerAction::Refresh;
            }
            return requestSend(nowMs);
        }
        if (std::strcmp(label, "EXIT") == 0) {
            return HermesXComposerAction::CloseRequested;
        }
        if (std::strcmp(label, "DEL") == 0) {
            if (bopomofoMode_ && bpmf_.composing()) {
                bpmf_.backspace();
            } else {
                removeLastDraftCharacter();
            }
        } else if (std::strcmp(label, "Aa") == 0) {
            lowercase_ = !lowercase_;
        } else if (std::strcmp(label, u8"˙") == 0) {
            const hermesx_bpmf::HermesXBpmfSymbol *tone = hermesx_bpmf::tone5_symbol();
            if (tone) {
                bpmf_.addSymbol(*tone);
            }
        } else if (std::strcmp(label, u8"中") == 0) {
            bopomofoMode_ = true;
            bpmf_.reset();
        } else if (std::strcmp(label, "EN") == 0) {
            bopomofoMode_ = false;
            bpmf_.reset();
            if (keyRow_ == kRowCount - 1 && keyCol_ >= kRowLengthsEnglish[kRowCount - 1]) {
                keyCol_ = kRowLengthsEnglish[kRowCount - 1] - 1;
            }
        } else if (std::strcmp(label, "SP") == 0) {
            if (bopomofoMode_ && bpmf_.composing()) {
                bpmf_.addSpace();
            } else if (draft_.length() < payloadLimit) {
                draft_ += ' ';
            }
        } else if (bopomofoMode_ && keyRow_ < 4) {
            const hermesx_bpmf::HermesXBpmfSymbol *symbol = hermesx_bpmf::screen_symbol(kBopomofoScreenKeys[keyRow_][keyCol_]);
            if (symbol) {
                bpmf_.addSymbol(*symbol);
            }
        } else if (draft_.length() + std::strlen(label) <= payloadLimit) {
            draft_ += label;
        }
        return HermesXComposerAction::Refresh;
    }

    return HermesXComposerAction::Consumed;
}

} // namespace graphics
