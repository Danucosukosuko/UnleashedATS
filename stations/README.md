# FM station lists

One JSON file per country, downloaded by the receiver from *Settings → Load Stations*. Downloading a
country replaces the one stored before. The receiver then lists its stations under *Menu → Stations* and
names the station on the frequency you tune when there is no RDS, the way EiBi does on shortwave.

## Files

* `index.json` - the countries, grouped by region, with the number of stations of each.
* `<CC>.json` - the stations of one country, `CC` being its ISO 3166 code.

```json
{
  "format": 1, "code": "ES", "country": "Spain", "region": "Europe", "generated": "2026-11-26",
  "sources": ["REMER vademecum (DGPCE, Spain)", "Wikidata"],
  "stations": [
    {"f": 98.1, "n": "LOS40", "c": "Madrid", "p": "LOS40"},
    {"f": 104.6, "n": "Radio Cornella", "c": "Cornella de Llobreg", "name": "Ràdio Cornellà", "city": "Cornellà de Llobregat"}
  ]
}
```

| Field | Meaning |
|---|---|
| `f` | Frequency in MHz |
| `n`, `c` | Station name and municipality, in ASCII as the receiver shows them |
| `p` | RDS name (PS) the station is expected to send, when known |
| `name`, `city` | Full spelling, when it differs from the ASCII one. Municipalities keep their local name |

Country and region names are in English. Stations are sorted by frequency.

## Building the lists

```shell
pip install unidecode   # optional, better ASCII for non-Latin scripts
python tools/stations/build_stations.py            # every country
python tools/stations/build_stations.py --only ES  # one country
```

## Sources and licenses

| Source | Coverage | License |
|---|---|---|
| [Wikidata](https://www.wikidata.org/) | Worldwide, items with an FM frequency | CC0 |
| [Wikipedia](https://www.wikipedia.org/) station tables | Lists of radio stations by country, network articles | CC BY-SA 4.0 |
| [FCC FM Query](https://www.fcc.gov/media/radio/fm-query) | United States, licensed FM and LPFM stations | Public domain |
| REMER vademecum of the Spanish Directorate-General for Civil Protection and Emergencies | Spain, every province | Public sector information |
| `tools/stations/manual/<CC>.json` | Additions and corrections by contributors | GPLv3 |

The lists as a whole are distributed under the [GNU GPLv3](../LICENSE), like the rest of UnleashedATS.
Material adapted from Wikipedia is used under CC BY-SA 4.0, which Creative Commons declares compatible with
the GPLv3 for adaptations. Each file names its sources.

## Contributing stations

Add or correct stations in `tools/stations/manual/<CC>.json`, with the same `stations` format plus the
optional `ps` field, and rebuild. Manual entries take priority over every other source.
