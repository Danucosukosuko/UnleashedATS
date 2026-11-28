#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Memories.h"
#include "Draw.h"
#include "Storage.h"
#include "Screenshot.h"
#include "RemoteAdhoc.h"
#include "ParserAdhoc.h"
#include "Backup.h"
#include "DxWatch.h"

static_assert(1 + AdhocCommand::THEME_WORDS * 5 <= FramerState::BUFFER_SIZE,
              "A theme must fit in the remote input buffer");

static bool remoteShowError(Stream& stream, const char *message)
{
  stream.printf("\r\nError: %s\r\n", message);
  return false;
}

static bool remoteSetFrequency(Stream& stream, uint32_t freqHz)
{
  const Band *band = getCurrentBand();
  if(freqHz < band->minimumFreq || freqHz > band->maximumFreq)
  {
    // Move to a band that covers the frequency, FM above 30MHz
    uint8_t mode = freqHz>=30000000? FM : currentMode==FM? AM : currentMode;
    if(tuneToFrequency(freqHz, mode)) return true;
    return remoteShowError(stream, "Frequency is out of range for every band");
  }
  if(!updateFrequency(freqHz, false)) return false;

  clearStationInfo();
  identifyFrequency(currentFrequency);

  return true;
}

static void remoteGetMemories(Stream& stream)
{
  for (uint8_t i = 0; i < getTotalMemories(); i++) {
    const Memory memory = getMemory(i);
    if (memory.freq) {
      stream.printf("#%02d,%s,%lu,%s%s%s\r\n", i + 1, bands[memory.band].bandName,
                     (unsigned long)memory.freq, bandModeDesc[memory.mode],
                     memory.name[0] ? "," : "", memory.name);
    }
  }
}

static bool remoteSetMemory(Stream& stream, const AdhocCommand& command)
{
  const uint32_t slot = command.memory_slot;
  if(slot < 1 || slot > (uint32_t)getTotalMemories())
    return remoteShowError(stream, "Invalid memory slot number");

  Memory mem = getMemory(slot - 1);
  mem.mode = 15;
  for (int i = 0; i < getTotalModes(); i++) {
    if (strcmp(bandModeDesc[i], command.mode) == 0) {
      mem.mode = i;
      break;
    }
  }
  if (mem.mode == 15)
    return remoteShowError(stream, "No such mode");

  mem.freq = command.frequency;
  mem.band = 0xFF;
  bool knownBand = false;
  for (int i = 0; i < getTotalBands(); i++) {
    if (strcmp(bands[i].bandName, command.band) != 0) continue;
    knownBand = true;
    // Match frequency as well as name for duplicate bands (15M).
    // A zero frequency clears the slot, so only its band name must match.
    if (!mem.freq || isMemoryInBand(&bands[i], &mem)) {
      mem.band = i;
      break;
    }
  }
  if (mem.band == 0xFF)
    return remoteShowError(stream, knownBand ? "Invalid frequency or mode" : "No such band");

  if(command.has_name)
    memcpy(mem.name, command.name, sizeof(mem.name));
  setMemory(slot-1, mem);
  return true;
}

//
// Set current color theme from the remote
//
static void remoteSetColorTheme(Stream& stream, const AdhocCommand& command)
{
  memcpy((uint8_t *)&TH + offsetof(ColorTheme, bg), command.theme_words, sizeof(command.theme_words));
  stream.println(" Ok");
  drawScreen();
}

//
// Print current color theme to the remote
//
static void remoteGetColorTheme(Stream& stream)
{
  stream.printf("Color theme %s: ", TH.name);
  const uint8_t *p = (uint8_t *)&(TH.bg);

  for(size_t i=0 ; i<sizeof(ColorTheme)-offsetof(ColorTheme, bg) ; i+=sizeof(uint16_t))
  {
    stream.printf("x%02X%02X", p[i+1], p[i]);
  }

  stream.println();
}

//
// Print current status to the remote
//
static void remotePrintStatus(Stream& stream, const AdhocState& state)
{
  // Prepare information ready to be sent
  float remoteVoltage = batteryMonitor();

  // S-Meter conditional on compile option
  rx.getCurrentReceivedSignalQuality();
  uint8_t remoteRssi = rx.getCurrentRSSI();
  uint8_t remoteSnr = rx.getCurrentSNR();

  // Use rx.getFrequency to force read of capacitor value from SI4732/5
  rx.getFrequency();
  uint16_t tuningCapacitor = rx.getAntennaTuningCapacitor();

  // Remote serial
  stream.printf("%u,%u,%d,%d,%s,%s,%s,%s,%hu,%hu,%hu,%hu,%hu,%.2f,%hu\r\n",
                VER_APP,
                getReceiverFrequency(currentFrequency),
                getCurrentBFO(),
                ((currentMode == USB) ? getCurrentBand()->usbCal :
                 (currentMode == LSB) ? getCurrentBand()->lsbCal : 0),
                getCurrentBand()->bandName,
                bandModeDesc[currentMode],
                getCurrentStep()->desc,
                getCurrentBandwidth()->desc,
                agcIdx,
                volume,
                remoteRssi,
                remoteSnr,
                tuningCapacitor,
                remoteVoltage,
                state.sequence
                );
}

//
// Execute a validated remote command
//
static bool remoteExecuteCommand(Stream& stream, AdhocState& state, const AdhocCommand& command)
{
  using Id = AdhocCommand::Id;
  switch(command.id)
  {
    case Id::RotateCw:   return handleRotation(1, 1);
    case Id::RotateCcw:  return handleRotation(-1, -1);
    case Id::Click:      return handleClick(false);
    case Id::ShortPress: return handleClick(true);
    case Id::BandUp:      doBand(1); break;
    case Id::BandDown:    doBand(-1); break;
    case Id::ModeUp:      doMode(1); break;
    case Id::ModeDown:    doMode(-1); break;
    case Id::StepUp:      doStep(1); break;
    case Id::StepDown:    doStep(-1); break;
    case Id::BandwidthUp:   doBandwidth(1); break;
    case Id::BandwidthDown: doBandwidth(-1); break;
    case Id::AgcUp:       doAgc(1); break;
    case Id::AgcDown:     doAgc(-1); break;
    case Id::VolumeUp:    doVolume(1); break;
    case Id::VolumeDown:  doVolume(-1); break;
    case Id::BacklightUp:   doBrt(1); break;
    case Id::BacklightDown: doBrt(-1); break;
    case Id::CalUp:       doCal(1); break;
    case Id::CalDown:     doCal(-1); break;
    case Id::SleepOn:     sleepOn(true); return true;
    case Id::SleepOff:    sleepOn(false); return true;
    case Id::SetMemory:
      if(!remoteSetMemory(stream, command)) return false;
      prefsRequestSave(SAVE_MEMORIES);
      break;
    case Id::SetFrequency:
      if(!remoteSetFrequency(stream, command.frequency)) return false;
      prefsRequestSave(SAVE_CUR_BAND);
      break;
    case Id::CaptureScreen:
    case Id::CaptureScreenBinary:
      state.logging = false;
      screenshotWrite(stream, command.id == Id::CaptureScreenBinary);
      return false;
    case Id::ToggleLog:
      state.logging = !state.logging;
      return false;
    case Id::ToggleJsonLog:
      state.jsonLogging = !state.jsonLogging;
      if(state.jsonLogging) statusJsonWrite(stream, true);
      return false;
    case Id::ToggleDx:
      toggleDxMode();
      return true;
    case Id::ToggleMono:
      doFmStereo(1);
      return true;
    case Id::SeekUp:
    case Id::SeekDown:
      seekFrequency(command.id == Id::SeekUp? 1 : -1);
      prefsRequestSave(SAVE_CUR_BAND);
      return true;
    case Id::GetMemories:
      remoteGetMemories(stream);
      return false;
    case Id::Backup:
    case Id::BackupNoNetwork:
      // Framed, so that the web updater can save it as a file
      state.logging = false;
      stream.print("BACKUP BEGIN\r\n");
      backupWrite(stream, command.id == Id::Backup);
      stream.print("BACKUP END\r\n");
      return false;
    case Id::DxLog:
      state.logging = false;
      stream.print("DXLOG BEGIN\r\n");
      dxWatchWriteLog(stream);
      stream.print("DXLOG END\r\n");
      return false;
    case Id::ToggleThemeEditor:
      stream.println(switchThemeEditor(!switchThemeEditor()) ? "Theme editor enabled" : "Theme editor disabled");
      return true;
    case Id::SetTheme:
      if(switchThemeEditor()) remoteSetColorTheme(stream, command);
      return false;
    case Id::GetTheme:
      if(switchThemeEditor()) remoteGetColorTheme(stream);
      return false;
    default:
      return false;
  }
  return true;
}

void remoteAdhocTick(Stream& stream, AdhocState& state)
{
  if(!state.logging && !state.jsonLogging) return;
  const uint32_t now = millis();
  if(now - state.reportTimer < 500) return;
  state.reportTimer = now;
  state.sequence++;
  if(state.logging) remotePrintStatus(stream, state);
  // Everything the receiver knows, one JSON object per line
  if(state.jsonLogging) statusJsonWrite(stream, true);
}

// Echo and prompts are ad hoc terminal behavior, not framing rules.
void remoteAdhocEcho(Stream& stream, const FramerState& input, uint8_t byte)
{
  if(input.ignoring || input.discarding) return;
  const uint8_t first = input.length ? input.buffer[0] : byte;
  if(!input.length && byte == '^')
  {
    if(switchThemeEditor())
      stream.print("Enter a string of hex colors (x0001x0002...): ");
  }
  else if(first == '#' || first == 'F' || (first == '^' && switchThemeEditor()))
  {
    if(byte != '\r' && byte != '\n') stream.write(byte);
  }
}

// "&BEGIN", one "&<line>" per stored value, "&END": restore a backup,
// answering each line so that the sender can wait for it
static void remoteRestoreFrame(Stream& stream, ByteSpan frame)
{
  char line[FramerState::BUFFER_SIZE];
  size_t length = 0;
  for(size_t i=1 ; i<frame.size && frame.data[i] != '\r' && frame.data[i] != '\n' && length < sizeof(line) - 1 ; i++)
    line[length++] = frame.data[i];
  line[length] = '\0';

  if(!strcmp(line, "BEGIN"))
  {
    backupRestoreBegin();
    stream.print("&OK\r\n");
  }
  else if(!strcmp(line, "END"))
  {
    stream.printf("&DONE %d\r\n", backupRestoreEnd());
    stream.flush();
    // Start again with what was restored
    delay(500);
    ESP.restart();
  }
  else if(const char *error = backupRestoreLine(line))
    stream.printf("&ERR %s\r\n", error);
  else
    stream.print("&OK\r\n");
}

bool remoteAdhocFrame(Stream& stream, AdhocState& state, ByteSpan frame)
{
  if(frame.data[0] == '&')
  {
    remoteRestoreFrame(stream, frame);
    return false;
  }
  if(frame.data[0] == '^' && !switchThemeEditor()) return false;
  AdhocCommand command;
  const ParseResult result = parseAdhoc(frame, command);
  if(result != ParseResult::Ready)
  {
    if(frame.data[0] == '^') stream.println(" Err");
    else if(result == ParseResult::InvalidSyntax)
      remoteShowError(stream, "Invalid command syntax");
    else if(result == ParseResult::InvalidArgument)
      remoteShowError(stream, "Invalid command argument");
    return false;
  }
  if(command.id == AdhocCommand::Id::SetMemory || command.id == AdhocCommand::Id::SetFrequency)
    stream.println();
  return remoteExecuteCommand(stream, state, command);
}

void remoteAdhocTimeout(Stream& stream, const FramerState& input)
{
  if(!input.ignoring && (input.buffer[0] != '^' || switchThemeEditor()))
    remoteShowError(stream, "Command timed out; send CR or LF to resume");
}

void remoteAdhocOverflow(Stream& stream)
{
  remoteShowError(stream, "Command is too long");
}
