#pragma once

// Global EM state used by EMACT gating.
extern bool gHermesEmergencyAwaitingSafe;
void HermesSetEmergencyAwaitingSafe(bool on, bool persist = true);
void HermesClearEmergencyAwaitingSafe(bool persist = true);
bool HermesIsEmergencyAwaitingSafe();
void HermesLoadEmergencyAwaitingSafe();
