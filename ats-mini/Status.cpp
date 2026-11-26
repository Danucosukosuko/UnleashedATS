#include "Common.h"
#include "Utils.h"
#include "Menu.h"
#include "AltFreq.h"

#include <ArduinoJson.h>

// Strings received over the air can carry any byte, keep them printable
static String printable(const char *text)
{
  String out;
  for( ; text && *text ; text++)
    out += (*text>=' ' && *text<='~')? *text : ' ';
  out.trim();
  return(out);
}

static String hex4(uint16_t value)
{
  char text[5];
  sprintf(text, "%04X", value);
  return(String(text));
}

//
// Write the receiver status as a single line of JSON. Live readings
// query the receiver chip, so they may only be taken on the main task;
// other tasks get the values last read by the main loop.
//
void statusJsonWrite(Print &out, bool live)
{
  static uint8_t sequence = 0;
  JsonDocument doc;

  uint8_t curRssi = rssi, curSnr = snr;
  if(live)
  {
    rx.getCurrentReceivedSignalQuality();
    curRssi = rx.getCurrentRSSI();
    curSnr  = rx.getCurrentSNR();
  }

  doc["fw"]     = getVersion();
  doc["seq"]    = sequence++;
  doc["freq"]   = currentFrequency;
  doc["band"]   = getCurrentBand()->bandName;
  doc["mode"]   = bandModeDesc[currentMode];
  doc["step"]   = getCurrentStep()->desc;
  doc["bw"]     = getCurrentBandwidth()->desc;
  doc["agc"]    = agcIdx;
  doc["vol"]    = volume;
  doc["rssi"]   = curRssi;
  doc["snr"]    = curSnr;
  doc["stereo"] = currentMode==FM && fmStereoIdx!=FM_STEREO_MONO && rx.getCurrentPilot();
  doc["dx"]     = dxMode;
  doc["mono"]   = fmStereoIdx==FM_STEREO_MONO;
  // Broadcast range, as the remote page band buttons name it
  doc["range"]  = currentMode==FM? "FM" : currentFrequency<520000? "LW" :
                  currentFrequency<1710000? "MW" : "SW";

  // Station name: RDS, a memory name, or a station list entry
  const char *name = getStationName();
  bool listed = *name == (char)0xFF;
  doc["name"]   = printable(listed? name + 1 : name);
  doc["ps"]     = printable(getRdsStationName());
  doc["listed"] = listed? printable(name + 1) : String("");
  uint16_t pi = afProgramId();
  doc["pi"]     = pi? hex4(pi) : String("");
  doc["pty"]    = printable(getProgramInfo());
  doc["rt"]     = printable(getRadioText());
  doc["tp"]     = rdsTrafficProgram();
  doc["ta"]     = rdsTrafficAnnouncement();

  JsonArray af = doc["af"].to<JsonArray>();
  for(int i=0 ; i<afCount() ; i++) af.add(afFrequency(i));

  JsonArray eon = doc["eon"].to<JsonArray>();
  EonNetwork network;
  for(int i=0 ; i<eonCount() ; i++)
    if(eonNetwork(i, &network))
    {
      JsonObject item = eon.add<JsonObject>();
      item["pi"] = hex4(network.pi);
      item["ps"] = printable(network.ps);
      JsonArray freqs = item["f"].to<JsonArray>();
      for(int j=0 ; j<network.count ; j++) freqs.add(network.freq[j]);
    }

  serializeJson(doc, out);
  out.print("\r\n");
}
