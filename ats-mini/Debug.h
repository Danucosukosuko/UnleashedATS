#ifndef DEBUG_MODE_H
#define DEBUG_MODE_H

#include <stdint.h>

// Debug mode, enabled from the About screen (see docs/source/development.md).
// It unlocks a Debug settings menu that edits the band limits beyond what
// the receiver is known to support, and a few diagnostics.

extern bool debugMode;
extern bool debugUsbLog;

void debugSetMode(bool on);
// Apply the saved band limits, at startup after the bands are loaded
void debugApplyBandLimits();
// Change the limits of a band, or put back the original ones
void debugSetBandLimits(uint8_t band, uint32_t minimum, uint32_t maximum);
void debugResetBandLimits(uint8_t band);
bool debugBandLimitsChanged(uint8_t band);

#endif // DEBUG_MODE_H
