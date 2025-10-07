#pragma once

#include "pb.h"
#include "pb_encode.h"
#include "meshtastic/mesh.pb.h"
#include "MeshService.h"
#include "MeshTypes.h"      // 提供 NodeNum, PacketPool
#include "MeshService.h"    // 提供 sendToMesh()



#pragma once

#include "pb.h"
#include "pb_encode.h"
#include "meshtastic/mesh.pb.h"
#include "MeshService.h"
#include "MeshTypes.h"

class HermesXPacketUtils {
public:
    static meshtastic_MeshPacket* makeFromData(const meshtastic_Data* data,
                                               uint32_t to = NODENUM_BROADCAST,
                                               bool want_ack = false);
};