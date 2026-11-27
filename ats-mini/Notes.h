#ifndef NOTES_H
#define NOTES_H

#include <stdint.h>

// Notes of the release found by the updater, shown before installing it.
// Rotate to read, then pick Install or Cancel at the end.

void notesOpen();
void notesRotate(int16_t enc);
void notesClick();
void notesDraw();

#endif // NOTES_H
