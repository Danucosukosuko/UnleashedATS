#ifndef RECOVERY_H
#define RECOVERY_H

// The recovery app, in a partition of its own that updates never touch.
// The UnleashedATS bootloader starts it when the encoder is held for two
// seconds at power on, or when no firmware can start.

// TRUE when the recovery app is installed and can be started from here
bool recoveryAvailable();
// Restart into the recovery app. Returns only when it is not available.
void recoveryRestart();

// Called first thing at startup: TRUE after three crashes in a row, before
// the firmware got to run for a while
bool recoveryCrashLoop();
// Called once the firmware has been running for a while
void recoveryStable();

#endif // RECOVERY_H
