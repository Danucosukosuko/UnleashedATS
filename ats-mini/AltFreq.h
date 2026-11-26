#ifndef ALTFREQ_H
#define ALTFREQ_H

#include <stdint.h>

#define AF_MAX_COUNT      25 // Alternative frequencies kept per station
#define EON_MAX_NETWORKS   8 // Other networks kept per station
#define EON_MAX_FREQS     12 // Frequencies kept per other network

typedef struct
{
  uint16_t pi;                    // Program identification of the network
  char ps[9];                     // Station name, as far as received
  uint8_t psSegments;             // Bit mask of the name segments received
  uint8_t count;                  // Frequencies collected
  uint32_t freq[EON_MAX_FREQS];   // Frequencies in Hz
} EonNetwork;

// Called for every RDS group read from the receiver
void rdsExtraGroup();
// Called when the receiver moves to another station. The lists are hidden
// until the same station shows up again, so that tuning to one of its own
// alternative frequencies keeps them.
void rdsExtraHide();
// Called on band changes, drops everything
void rdsExtraReset();

// Alternative frequencies (Hz) of the station being received
uint8_t afCount();
uint32_t afFrequency(uint8_t idx);
uint16_t afProgramId();
// Other networks referenced by the station being received (EON)
uint8_t eonCount();
bool eonNetwork(uint8_t idx, EonNetwork *network);
// TRUE when the station carries these services, for the indicators
bool afPresent();
bool eonPresent();
// Traffic programme and traffic announcement flags
bool rdsTrafficProgram();
bool rdsTrafficAnnouncement();

// Tune to one of the alternative frequencies, keeping the station info
bool afTune(uint32_t freq);
// Follow the station to an alternative frequency once the reception
// fades. Returns TRUE when the screen needs a redraw.
bool afTickTime();

#endif // ALTFREQ_H
