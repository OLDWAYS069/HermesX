#include "configuration.h"
#include "HermesFace.h"

#if MESHTASTIC_EXCLUDE_HERMESX
#include "HermesXLog.h"

bool HermesX_IsUiEnabled()
{
    return false;
}

bool HermesX_TryGetFaceRenderContext(HermesFaceRenderContext &ctx)
{
    (void)ctx;
    HERMESX_LOG_INFO("Fallback HermesX_TryGetFaceRenderContext invoked");
    return false;
}

void HermesX_DrawFace(OLEDDisplay * /*display*/, int16_t /*x*/, int16_t /*y*/, HermesFaceMode /*mode*/)
{
    HERMESX_LOG_INFO("Fallback HermesX_DrawFace invoked");
}
#else
// When HermesX support is compiled in, the real implementations live in HermesFaceAdapter.cpp.
// Provide empty translation unit here so builds without HermesX can still link via the fallback above.
#endif
