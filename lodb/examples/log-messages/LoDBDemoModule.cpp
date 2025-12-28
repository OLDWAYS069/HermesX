#include "LoDBDemoModule.h"
#include "gps/RTC.h"
#include "logging.h"
#include <algorithm>
#include <cstring>

LoDBDemoModule::LoDBDemoModule() : SinglePortModule("LoDBDemo", meshtastic_PortNum_TEXT_MESSAGE_APP), db(new LoDb("lodb_demo"))
{
    db->registerTable("messages", &meshtastic_LoDBDemoMessage_msg, sizeof(meshtastic_LoDBDemoMessage));
}

LoDBDemoModule::~LoDBDemoModule()
{
    delete db;
}

ProcessMessage LoDBDemoModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    meshtastic_LoDBDemoMessage record = meshtastic_LoDBDemoMessage_init_zero;
    record.timestamp = getTime();
    record.from_node = mp.from;
    record.to_node = mp.to;

    size_t copy_len = std::min(static_cast<size_t>(mp.decoded.payload.size), sizeof(record.text) - 1);
    if (copy_len > 0) {
        memcpy(record.text, mp.decoded.payload.bytes, copy_len);
    }
    record.text[copy_len] = '\0';

    lodb_uuid_t uuid = lodb_new_uuid(nullptr, record.timestamp ^ record.from_node);
    LoDbError err = db->insert("messages", uuid, &record);
    if (err != LODB_OK) {
        LOG_WARN("LoDBDemo: failed to log message err=%d", err);
    }

    return ProcessMessage::CONTINUE;
}
