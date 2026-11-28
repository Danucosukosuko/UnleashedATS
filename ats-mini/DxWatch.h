#ifndef DXWATCH_H
#define DXWATCH_H

#include <stdint.h>

// DX Watch sweeps the FM band while nobody is listening, learns the
// stations that are always there, and stops on any new one, which is
// how tropospheric and sporadic E openings show up.

enum DxWatchPhase { DXW_OFF, DXW_LEARNING, DXW_WATCHING, DXW_CATCH };

typedef struct
{
  uint32_t freq;      // Hz
  uint16_t pi;
  char ps[9];
  uint8_t rssi;
  uint8_t snr;
} DxCatch;

void dxWatchStart();
// Stop watching. Staying keeps the receiver on the station it is on.
void dxWatchStop(bool stay);
DxWatchPhase dxWatchPhase();
// Advance the sweep by one channel. Returns TRUE when the screen needs a redraw.
bool dxWatchTick();

uint32_t dxWatchFrequency();
uint16_t dxWatchSweeps();
uint16_t dxWatchCatches();
const DxCatch *dxWatchLastCatch();

#define DXWATCH_LOG "/dxlog.csv"

// The log of catches: time_utc,frequency_mhz,pi,ps,rssi_dbuv,snr_db
typedef struct
{
  char time[24];      // UTC, ISO 8601, empty when the clock was not set
  uint32_t freq;      // Hz
  uint16_t pi;
  char ps[9];
  uint8_t rssi;
  uint8_t snr;
} DxLogEntry;

class Print;
void dxWatchWriteLog(Print &out);
// Fill entries with the newest catches first, returns how many
int dxWatchLoadLog(DxLogEntry *entries, int max);
bool dxWatchClearLog();

#endif // DXWATCH_H
