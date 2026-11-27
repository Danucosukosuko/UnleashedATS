#ifndef SAFE_MODE_H
#define SAFE_MODE_H

// Safe mode, entered by holding the encoder button while switching on.
// It offers to erase parts of the stored data, to go back to the previous
// firmware, and to recover over Wi-Fi or USB.

// Runs the safe mode menu, returns when the receiver should start
void safeModeRun(bool afterCrashes = false);
// Recovery Wi-Fi was requested for this session
bool safeModeWifi();
// Called from the main loop, starts a requested reinstall once online
void safeModeTick();

#endif // SAFE_MODE_H
