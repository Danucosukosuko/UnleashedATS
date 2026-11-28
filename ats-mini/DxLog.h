#ifndef DXLOG_H
#define DXLOG_H

#include <stdint.h>

// The DX Watch log on the receiver: the catches, newest first. Click one
// to tune it; Clear log at the end empties it after a second click.

void dxLogOpen();
void dxLogRotate(int16_t enc);
void dxLogClick();
void dxLogDraw();

#endif // DXLOG_H
