#ifndef PTY_SEARCH_H
#define PTY_SEARCH_H

#include <stdint.h>

// PTY Search: pick a programme type and go to the next FM station that
// broadcasts it, such as News, Rock or Sport.

enum PtySearchPhase { PTYS_OFF, PTYS_CHOOSE, PTYS_SEARCHING };

void ptySearchOpen();
void ptySearchRotate(int16_t enc);
void ptySearchClick();
// Check the next channel while searching. Returns TRUE when the screen needs a redraw.
bool ptySearchTick();

PtySearchPhase ptySearchPhase();
// Programme types to choose from, in the RDS or RBDS list in use
int ptySearchChoices();
int ptySearchChoice();
const char *ptySearchName(int choice);
uint32_t ptySearchFrequency();

#endif // PTY_SEARCH_H
