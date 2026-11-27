#ifndef HIDDEN_GAME_H
#define HIDDEN_GAME_H

#include <stdint.h>

// Skywave, the hidden game: keep a radio wave bouncing between your
// antenna and the ionosphere until every layer is broken through.

void gameStart();
void gameStop();
// Advance the game. Returns TRUE when a new frame is due.
bool gameTick();
void gameRotate(int16_t enc);
void gameClick();
void gameDraw();

#endif // HIDDEN_GAME_H
