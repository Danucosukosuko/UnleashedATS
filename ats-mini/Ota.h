#ifndef OTA_H
#define OTA_H

#include <Arduino.h>

enum OtaPhase { OTA_IDLE, OTA_CHECK_QUEUED, OTA_QUEUED, OTA_CONNECTING, OTA_WRITING, OTA_COMPLETE, OTA_REBOOT_PENDING, OTA_FAILED, OTA_CURRENT, OTA_AVAILABLE };

struct OtaStatus
{
  OtaPhase phase;
  String message;
};

// Begin returns false only when busy. Accepted uploads register otaEndUpload
// on disconnect and remain busy through errors until that callback runs.
// imageSize is the exact firmware file size, excluding multipart overhead.
bool otaBegin(size_t imageSize);
bool otaWrite(uint8_t *data, size_t size);
// Validate the completed image and select it for the next boot.
bool otaFinish();
// On disconnect, schedule reboot after success or abort an unfinished upload.
// Failed uploads release the busy flag here so another update can start.
void otaEndUpload();
OtaStatus otaStatus();

bool otaRequestLatest(bool install);
// Install the latest release even when it is the version already running
bool otaRequestReinstall();
// Check for a newer release and open its notes on the receiver, where it
// can be installed
bool otaRequestReview();
void otaTick();

// Releases looked at by the updater: stable ones only, or betas as well
enum OtaChannel : uint8_t { OTA_STABLE = 0, OTA_BETA = 1 };
extern uint8_t otaChannel;
// Tag and notes (Markdown) of the release found by the last check
const char *otaReleaseTag();
const char *otaReleaseNotes();

// The firmware kept in the other slot, normally the one installed before
// the last update. Returns FALSE when there is none that can be started.
bool otaPreviousFirmware(char *label, size_t size);
// Start the firmware in the other slot. Returns an error message on failure.
const char *otaBootPrevious();
// Confirm that this firmware starts. Until then, a reset brings the
// previous firmware back, so that a broken update cannot lock the receiver.
void otaConfirmRunning();

#endif
