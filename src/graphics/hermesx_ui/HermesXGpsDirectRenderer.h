#pragma once

#include "HermesXGpsStateCollector.h"

class TFTDisplay;

namespace graphics
{

using HermesXGpsWorkspaceEnsure = bool (*)();

class HermesXGpsDirectRenderer
{
  public:
    static bool render(TFTDisplay *display,
                       const HermesXGpsStateSnapshot &snapshot,
                       const HermesXGpsPosterLayers &layers,
                       HermesXGpsWorkspaceEnsure ensureWorkspace);
};

} // namespace graphics
