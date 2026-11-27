#ifndef UPDATE_SCREEN_H
#define UPDATE_SCREEN_H

#include <stdint.h>

// Full screen shown while the firmware is checked for, downloaded or
// installed. A task of its own keeps the animation moving while the main
// loop waits on the network.

enum UpdateIcon : uint8_t
{
  UPDATE_ICON_SEARCH,    // Radio waves, while checking or connecting
  UPDATE_ICON_DOWNLOAD,  // Arrow falling into the receiver, while writing
  UPDATE_ICON_VERIFY,    // Spinning ring, while the image is checked
  UPDATE_ICON_OK,        // Check mark
  UPDATE_ICON_NEW,       // A newer release exists
  UPDATE_ICON_FAIL,      // Cross
};

// Start the animated screen, or change what it shows. A negative percent
// shows no progress bar.
void updateScreenShow(UpdateIcon icon, const char *title, const char *line1, const char *line2,
                      int percent, const char *footer);
// Stop the animation, leaving the last frame on screen
void updateScreenHide();
// Show a final result and wait until a click or the time runs out
void updateScreenResult(UpdateIcon icon, const char *title, const char *line1, const char *line2,
                        uint32_t holdMs);

#endif // UPDATE_SCREEN_H
