#ifndef DEBUG_MODE_H
#define DEBUG_MODE_H

#include <stdint.h>

// Debug mode, enabled from the About screen (see docs/source/development.md).
// It unlocks a Debug settings menu that edits the band limits beyond what
// the receiver is known to support, tunes receiver and system internals,
// and shows live diagnostics.

#define DEBUG_AUTO -1  // Leave a setting to the firmware

extern bool debugMode;
extern bool debugUsbLog;
extern bool debugInfoView;      // Live information screen on
extern uint32_t debugLoopUs;    // Time of the last main loop pass

// Overrides, DEBUG_AUTO when unused
extern int16_t debugFmSoftMute;
extern int16_t debugFmSeekRssi;
extern int16_t debugFmSeekSnr;
extern int16_t debugAmSeekRssi;
extern int16_t debugAmSeekSnr;
extern int16_t debugAntCap;
extern uint16_t debugCpuMhz;
extern uint16_t debugI2cKhz;

void debugSetMode(bool on);
// Load the saved state and band limits, at startup after the bands are loaded
void debugApplyBandLimits();
// Change the limits of a band, or put back the original ones
void debugSetBandLimits(uint8_t band, uint32_t minimum, uint32_t maximum);
void debugResetBandLimits(uint8_t band);
bool debugBandLimitsChanged(uint8_t band);

void debugSaveSettings();
// Apply the receiver overrides, after every band change
void debugApplyRadio();
// Apply the CPU and I2C clocks
void debugApplySystem();

void debugDrawInfo();
void debugRebootToDownload();

#endif // DEBUG_MODE_H
