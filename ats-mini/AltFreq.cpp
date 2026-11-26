#include "Common.h"
#include "Utils.h"
#include "Menu.h"
#include "AltFreq.h"

// Alternative frequency codes, see IEC 62106 (EN 50067) section 3.2.1.6
#define AF_CODE_MIN         1   // First code that maps to a VHF frequency
#define AF_CODE_MAX       204   // Last code that maps to a VHF frequency
#define AF_CODE_FILLER    205   // Filler code, carries nothing
#define AF_CODE_LFMF      250   // The next code is an LF/MF frequency
#define AF_BASE_HZ   87500000   // Frequency of code zero (Hz)
#define AF_STEP_HZ     100000   // 100kHz per code

#define AF_GOOD_RSSI       24   // Signal good enough to stay put (dBuV)
#define AF_GOOD_SNR        12   // Quality good enough to stay put (dB)
#define AF_MARGIN           6   // Improvement expected from an alternative (dB)

#define AF_BAD_TIME     3000U   // Poor reception needed before hunting (msecs)
#define AF_RETRY_TIME   2000U   // Time between two attempts (msecs)
#define AF_SETTLE_TIME     60   // Time for the tuner to settle (msecs)
#define AF_PI_TIME       400U   // Time given to the decoder to report a PI (msecs)
#define AF_PI_POLL         20   // PI code polling interval (msecs)

static uint32_t afList[AF_MAX_COUNT]; // Alternatives in Hz
static uint8_t afTotal = 0;           // Alternatives collected so far
static uint8_t afNext  = 0;           // Next alternative to try

static EonNetwork eonList[EON_MAX_NETWORKS];
static uint8_t eonTotal = 0;

static uint16_t listPi   = 0;         // Station the lists belong to
static bool listShown    = false;     // Lists belong to the station on the air

static bool trafficProgram = false;   // TP flag of the station
static bool trafficAnnounce = false;  // TA flag of the station

static uint32_t afBadTime   = 0;      // Time the reception went bad, or zero
static uint32_t afTriedTime = 0;      // Time of the last attempt

static void clearLists()
{
  trafficProgram = trafficAnnounce = false;
  afTotal = afNext = eonTotal = 0;
  afBadTime = 0;
}

void rdsExtraReset()
{
  clearLists();
  listPi = 0;
  listShown = false;
}

void rdsExtraHide()
{
  listShown = false;
  afBadTime = 0;
}

static uint32_t afCodeToHz(uint8_t code)
{
  return(AF_BASE_HZ + (uint32_t)code * AF_STEP_HZ);
}

static void addFreq(uint32_t *list, uint8_t *count, uint8_t size, uint32_t freq)
{
  if(*count>=size) return;

  for(int i=0 ; i<*count ; i++)
    if(list[i]==freq) return;

  // Keep the list sorted, it reads better on screen
  int i = *count;
  while(i>0 && list[i-1]>freq)
  {
    list[i] = list[i-1];
    i--;
  }
  list[i] = freq;
  (*count)++;
}

//
// Add the frequencies of a pair of method A codes, skipping the list
// headers, the fillers and the LF/MF frequencies
//
static void addCodes(uint16_t block, uint32_t *list, uint8_t *count, uint8_t size)
{
  uint8_t code[2] = { uint8_t(block >> 8), uint8_t(block & 0xFF) };

  for(int i=0 ; i<2 ; i++)
  {
    if(code[i]==AF_CODE_LFMF) break;
    if(code[i]>=AF_CODE_MIN && code[i]<=AF_CODE_MAX)
      addFreq(list, count, size, afCodeToHz(code[i]));
  }
}

static EonNetwork *findNetwork(uint16_t pi)
{
  for(int i=0 ; i<eonTotal ; i++)
    if(eonList[i].pi==pi) return(&eonList[i]);

  if(eonTotal>=EON_MAX_NETWORKS) return(NULL);

  EonNetwork *network = &eonList[eonTotal++];
  memset(network, 0, sizeof(*network));
  network->pi = pi;
  memset(network->ps, ' ', 8);
  return(network);
}

//
// Decode the frequency related parts of the RDS group just read
//
void rdsExtraGroup()
{
  if(!rx.getNumRdsFifoUsed() || !rx.getRdsBlocksReliable()) return;

  uint16_t pi = rx.getRdsPI();
  if(!pi) return;

  // A different station drops the lists of the previous one, while
  // the same station brings them back
  if(pi!=listPi)
  {
    clearLists();
    listPi = pi;
  }
  listShown = true;

  uint16_t blockB = rx.getRdsBlock(1);
  uint16_t blockC = rx.getRdsBlock(2);
  uint16_t blockD = rx.getRdsBlock(3);
  uint8_t type    = blockB >> 12;
  bool versionB   = blockB & 0x0800;

  // Every group carries TP, groups 0 and 15B also carry TA
  trafficProgram = blockB & 0x0400;
  if(type==0 || (type==15 && versionB)) trafficAnnounce = blockB & 0x0010;

  // Group 0A carries two alternative frequency codes in block C
  if(type==0 && !versionB)
  {
    addCodes(blockC, afList, &afTotal, AF_MAX_COUNT);
    return;
  }

  // Group 14 carries the Enhanced Other Networks information, with
  // the program identification of the other network in block D
  if(type!=14 || !blockD || blockD==listPi) return;

  EonNetwork *network = findNetwork(blockD);
  if(!network || versionB) return;

  uint8_t variant = blockB & 0x0F;
  if(variant<=3)
  {
    // Station name, two characters per variant
    for(int i=0 ; i<2 ; i++)
    {
      char c = i? (blockC & 0xFF) : (blockC >> 8);
      network->ps[variant * 2 + i] = (c>=' ' && c<='~')? c : ' ';
    }
    network->psSegments |= 1 << variant;
  }
  else if(variant==4)
  {
    // Alternative frequencies of the other network, method A
    addCodes(blockC, network->freq, &network->count, EON_MAX_FREQS);
  }
  else if(variant<=8)
  {
    // Mapped frequency, the other network on the low byte
    uint8_t code = blockC & 0xFF;
    if(code>=AF_CODE_MIN && code<=AF_CODE_MAX)
      addFreq(network->freq, &network->count, EON_MAX_FREQS, afCodeToHz(code));
  }
}

//
// The menus keep showing the lists while they are open, so that moving
// between the alternatives does not make them blink
//
static bool listsVisible()
{
  return(currentMode==FM && (listShown || currentCmd==CMD_AFLIST || currentCmd==CMD_EONLIST));
}

uint8_t afCount()       { return(listsVisible()? afTotal : 0); }
uint16_t afProgramId()  { return(listsVisible()? listPi : 0); }
uint8_t eonCount()      { return(listsVisible()? eonTotal : 0); }
bool afPresent()        { return(currentMode==FM && listShown && afTotal); }
bool rdsTrafficProgram()      { return(currentMode==FM && listShown && trafficProgram); }
bool rdsTrafficAnnouncement() { return(currentMode==FM && listShown && trafficAnnounce); }
bool eonPresent()       { return(currentMode==FM && listShown && eonTotal); }

uint32_t afFrequency(uint8_t idx)
{
  return(idx<afTotal? afList[idx] : 0);
}

bool eonNetwork(uint8_t idx, EonNetwork *network)
{
  if(idx>=eonTotal || !network) return(false);

  *network = eonList[idx];
  network->ps[8] = '\0';

  // Only a complete name is worth showing, otherwise use the PI code
  if(network->psSegments!=0x0F)
    sprintf(network->ps, "PI %04X", network->pi);

  // Drop the padding the broadcasters add
  for(int i=strlen(network->ps) ; i>0 && network->ps[i-1]==' ' ; i--)
    network->ps[i-1] = '\0';

  return(true);
}

bool afTune(uint32_t freq)
{
  if(currentMode!=FM || !freq) return(false);
  if(freq==currentFrequency) return(true);

  // Same program, so the station name and the lists stay
  softVolume(true);
  bool tuned = updateFrequency(freq, false);
  softVolume(false);
  afBadTime = 0;
  return(tuned);
}

//
// Wait for the decoder to report the program identification of the
// frequency the receiver is on right now
//
static uint16_t afReadPi()
{
  uint32_t startTime = millis();

  // Drop whatever the decoder still holds from the previous frequency
  rx.getRdsStatus(0, 1, 0);

  while((millis() - startTime) < AF_PI_TIME)
  {
    rx.getRdsStatus();

    if(rx.getNumRdsFifoUsed() && rx.getRdsSync())
    {
      uint16_t pi = rx.getRdsPI();
      if(pi) return(pi);
    }

    delay(AF_PI_POLL);
  }

  return(0);
}

//
// Have a look at the next alternative frequency. Returns TRUE when the
// receiver has moved over to it.
//
static bool afTryNext()
{
  uint32_t curFreq = currentFrequency;
  uint32_t freq = afList[afNext];
  uint8_t newRssi, newSnr;

  afNext = (afNext + 1) % afTotal;

  // Nothing to gain from where we already are
  if(freq==curFreq || !isFreqInBand(getCurrentBand(), freq)) return(false);

  // Keep quiet while looking around, without thumping the speaker
  softVolume(true);

  rx.setFrequencyConfirmed(freqFromHz(freq, FM));
  delay(AF_SETTLE_TIME);

  rx.getCurrentReceivedSignalQuality();
  newRssi = rx.getCurrentRSSI();
  newSnr  = rx.getCurrentSNR();

  // Only worth a move if it is a clear improvement carrying the same
  // program identification
  if((newRssi>=rssi+AF_MARGIN || newSnr>=snr+AF_MARGIN) && afReadPi()==listPi)
  {
    currentFrequency = freq;
    getCurrentBand()->currentFreq = freq;
    rssi = newRssi;
    snr  = newSnr;
    softVolume(false);
    afBadTime = 0;
    return(true);
  }

  // Back to the frequency we came from
  rx.setFrequencyConfirmed(freqFromHz(curFreq, FM));
  delay(AF_SETTLE_TIME);
  softVolume(false);

  return(false);
}

bool afTickTime()
{
  uint32_t currentTime = millis();

  // Only an identified FM station can have alternatives
  if(!(getRDSMode() & RDS_AF) || currentMode!=FM || !listShown || !afTotal || !listPi)
  {
    afBadTime = 0;
    return(false);
  }

  // Leave the scanners and the frequency lists alone
  if(currentCmd==CMD_SCAN || currentCmd==CMD_SEEK || currentCmd==CMD_AFLIST ||
     currentCmd==CMD_EONLIST || memScanRunning())
    return(false);

  // Reception is fine, stay where we are
  if(rssi>=AF_GOOD_RSSI && snr>=AF_GOOD_SNR)
  {
    afBadTime = 0;
    return(false);
  }

  // Note when the reception started to go bad
  if(!afBadTime) afBadTime = currentTime;

  // Give the signal a chance to come back, and space out the attempts
  if((currentTime - afBadTime) < AF_BAD_TIME) return(false);
  if((currentTime - afTriedTime) < AF_RETRY_TIME) return(false);

  afTriedTime = currentTime;
  return(afTryNext());
}
