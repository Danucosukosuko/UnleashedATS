#!/usr/bin/env python3
"""Build the UnleashedATS FM (VHF) station lists, one JSON file per country.

Sources, merged and de-duplicated by frequency and municipality:

* Wikidata (CC0): items with an FM-band frequency (P2144) and a country (P17).
* Wikipedia (CC BY-SA 4.0): station tables of the "List of radio stations
  in ..." articles and a few local-language lists (see EXTRA_WIKI_PAGES).
* FCC FM Query (public domain): every licensed US FM and LPFM station.
* Hand-maintained files in tools/stations/manual/<CC>.json, which take
  priority over everything else.

Country and region names are in English. Municipalities keep their local
name (in the "city" field), with an ASCII rendering for the receiver display
(in the "c" field). Station names keep their own spelling ("name"), again
with an ASCII rendering ("n").

Usage: python tools/stations/build_stations.py [--only ES,FR] [--skip-fcc]
Requires Python 3.9+. The optional "unidecode" package gives better ASCII
renderings of non-Latin scripts.
"""
import argparse
import datetime
import html
import html.parser
import json
import os
import re
import sys
import time
import unicodedata
import urllib.error
import urllib.parse
import urllib.request

try:
    from unidecode import unidecode
except ImportError:  # pragma: no cover
    unidecode = None

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT_DIR = os.path.join(ROOT, "stations")
MANUAL_DIR = os.path.join(HERE, "manual")
CACHE_DIR = os.path.join(HERE, ".cache")

USER_AGENT = os.environ.get("STATIONS_USER_AGENT", "stations-list-builder/1.0")
REQUEST_DELAY = float(os.environ.get("STATIONS_REQUEST_DELAY", "1.0"))  # Seconds between requests
FORMAT_VERSION = 1
PS_LEN = 8                      # RDS Programme Service name
REGION_LEN = 23                 # Longest province or state name the receiver keeps

FM_MIN, FM_MAX = 64.0, 108.0   # OIRT, Japanese and CCIR FM bands, in MHz
NAME_LEN, CITY_LEN = 23, 19     # Longest names the receiver keeps

# Wikidata continents mapped to the regions offered by the receiver
CONTINENTS = {
    "Q46": "Europe", "Q48": "Asia", "Q15": "Africa", "Q49": "North America",
    "Q18": "South America", "Q538": "Oceania", "Q55643": "Oceania",
    "Q3960": "Oceania", "Q828": "North America", "Q27611": "North America",
}
REGION_ORDER = ["Europe", "North America", "South America", "Asia", "Africa", "Oceania"]
# Countries that span continents, placed where listeners expect them
REGION_OVERRIDE = {
    "RU": "Europe", "TR": "Europe", "CY": "Europe", "GE": "Asia", "AM": "Asia",
    "AZ": "Asia", "KZ": "Asia", "EG": "Africa", "PA": "North America",
    "ID": "Asia", "PG": "Oceania", "TT": "North America", "GL": "North America",
}

# Languages for names where the official languages in Wikidata mislead
LANG_OVERRIDE = {"US": ["en"]}

# Local-language Wikipedia lists that the English category does not cover
EXTRA_WIKI_PAGES = [
    ("es", "Anexo:Emisoras de radio de España", "ES"),
    ("es", "Anexo:Emisoras de radio de Argentina", "AR"),
    ("es", "Anexo:Emisoras de radio de Chile", "CL"),
    ("es", "Anexo:Emisoras de radio de Colombia", "CO"),
    ("es", "Anexo:Emisoras de radio de México", "MX"),
    ("es", "Anexo:Emisoras de radio de Perú", "PE"),
    ("es", "Anexo:Emisoras de radio de Uruguay", "UY"),
    ("es", "Anexo:Emisoras de radio de Venezuela", "VE"),
    ("fr", "Liste des radios en France", "FR"),
    ("de", "Liste der Hörfunksender in Deutschland", "DE"),
    ("it", "Radio in Italia", "IT"),
    ("pt", "Lista de emissoras de rádio de Portugal", "PT"),
    ("nl", "Lijst van Nederlandse radiozenders", "NL"),
    ("pl", "Stacje radiowe w Polsce", "PL"),
]
WIKI_CATEGORY = "Category:Lists of radio stations by country"

# Categories of station and network articles, whose frequency tables list
# every transmitter of the network with its municipality
STATION_CATEGORIES = [
    ("fr", "Catégorie:Station de radio en France", "FR"),
    ("fr", "Catégorie:Station de radio en Belgique", "BE"),
    ("fr", "Catégorie:Station de radio en Suisse", "CH"),
    ("de", "Kategorie:Hörfunksender (Deutschland)", "DE"),
    ("de", "Kategorie:Hörfunksender (Österreich)", "AT"),
    ("it", "Categoria:Emittenti radiofoniche italiane", "IT"),
    ("pt", "Categoria:Estações de rádio de Portugal", "PT"),
    ("pt", "Categoria:Estações de rádio do Brasil", "BR"),
    ("nl", "Categorie:Nederlandse radiozender", "NL"),
    ("pl", "Kategoria:Rozgłośnie radiowe w Polsce", "PL"),
]


def log(*args):
    print(*args, file=sys.stderr, flush=True)


#
# HTTP helpers with a small on-disk cache, so that a rerun is cheap
#

def fetch(url, data=None, accept=None, cache=True, tries=4):
    key = None
    if cache:
        import hashlib
        key = hashlib.sha1((url + "|" + (data or "")).encode()).hexdigest()
        path = os.path.join(CACHE_DIR, key)
        if os.path.exists(path):
            with open(path, "rb") as f:
                return f.read()
    headers = {"User-Agent": USER_AGENT}
    if accept:
        headers["Accept"] = accept
    body = data.encode() if data else None
    for attempt in range(tries + 4):
        # Stay well under the request rates the public APIs tolerate
        time.sleep(REQUEST_DELAY)
        try:
            req = urllib.request.Request(url, data=body, headers=headers)
            with urllib.request.urlopen(req, timeout=180) as r:
                content = r.read()
            break
        except (urllib.error.URLError, TimeoutError) as e:
            code = getattr(e, "code", None)
            if code in (400, 403, 404) or attempt >= tries + 3:
                raise
            wait = 10 * (attempt + 1)
            if code in (429, 503):
                retry = getattr(e, "headers", {}) and e.headers.get("Retry-After")
                wait = max(wait, int(retry) if retry and retry.isdigit() else 30)
            log(f"  retrying in {wait}s ({e})")
            time.sleep(wait)
    # Never keep a huge answer, the disk may be short of space
    if key and len(content) <= 2 * 1024 * 1024:
        os.makedirs(CACHE_DIR, exist_ok=True)
        with open(os.path.join(CACHE_DIR, key), "wb") as f:
            f.write(content)
    return content


def sparql(query):
    data = urllib.parse.urlencode({"query": query})
    raw = fetch("https://query.wikidata.org/sparql", data=data,
                accept="application/sparql-results+json")
    rows = []
    for b in json.loads(raw)["results"]["bindings"]:
        rows.append({k: v["value"].rsplit("/", 1)[-1] if v["type"] == "uri" else v["value"]
                     for k, v in b.items()})
    return rows


def mwapi(host, **params):
    params.update(format="json", formatversion=2)
    url = f"https://{host}/w/api.php?" + urllib.parse.urlencode(params)
    return json.loads(fetch(url))


#
# Text helpers
#

LATIN_FIXES = str.maketrans({"ß": "ss", "æ": "ae", "Æ": "AE", "ø": "o", "Ø": "O",
                             "ł": "l", "Ł": "L", "đ": "d", "Đ": "D", "þ": "th",
                             "œ": "oe", "Œ": "OE", "ı": "i", "’": "'", "‘": "'",
                             "–": "-", "—": "-", "“": '"', "”": '"'})


def is_latin(text):
    letters = [c for c in text if c.isalpha()]
    if not letters:
        return True
    latin = sum(1 for c in letters if "LATIN" in unicodedata.name(c, ""))
    return latin >= 0.8 * len(letters)


def to_ascii(text, fallback=""):
    """ASCII rendering for the receiver display. Non-Latin scripts use the
    fallback (an English or romanized name) when one is available."""
    if not text:
        return ""
    if not is_latin(text) and fallback and is_latin(fallback):
        text = fallback
    text = text.translate(LATIN_FIXES)
    if is_latin(text) or not unidecode:
        text = unicodedata.normalize("NFKD", text)
        text = "".join(c for c in text if not unicodedata.combining(c))
    if unidecode:
        text = unidecode(text)
    text = "".join(c for c in text if " " <= c <= "~")
    return re.sub(r"\s+", " ", text).strip()


def clean(text):
    text = html.unescape(text or "")
    text = re.sub(r"\[[^\]]{1,12}\]", "", text)       # footnote marks
    text = re.sub(r"\s+", " ", text)
    return text.strip(" ,;:-*†‡")


def norm_key(text):
    return re.sub(r"[^a-z0-9]", "", to_ascii(text).lower())


FREQ_RE = re.compile(r"(?<![\d.,])(\d{2,3})(?:[.,](\d{1,3}))?(?![\d])")


def parse_freqs(text):
    """FM frequencies in MHz found in a table cell."""
    out = []
    text = text.replace(" ", " ")
    if re.search(r"khz|\bam\b|\bmw\b|\blw\b", text, re.I) and not re.search(r"mhz|\bfm\b", text, re.I):
        return out
    for whole, frac in FREQ_RE.findall(text):
        value = float(f"{whole}.{frac or 0}")
        if FM_MIN <= value <= FM_MAX:
            # FM channels sit on a 10kHz grid at most
            out.append(round(value, 2))
    return out


#
# Countries, from Wikidata
#

def load_countries():
    log("Countries (Wikidata)...")
    rows = sparql("""
SELECT ?c ?iso ?name ?cont ?lang WHERE {
  ?c wdt:P297 ?iso .
  FILTER NOT EXISTS { ?c wdt:P576 ?dissolved }
  OPTIONAL { ?c rdfs:label ?name . FILTER(LANG(?name) = "en") }
  OPTIONAL { ?c wdt:P30 ?cont }
  OPTIONAL { ?c wdt:P37 ?l . ?l wdt:P218 ?lang }
}""")
    countries = {}
    for r in rows:
        iso = r["iso"].upper()
        if not re.fullmatch(r"[A-Z]{2}", iso):
            continue
        c = countries.setdefault(iso, {"code": iso, "qid": r["c"], "name": "",
                                       "continents": [], "langs": []})
        if r.get("name") and not c["name"]:
            c["name"] = r["name"]
        if r.get("cont") and r["cont"] not in c["continents"]:
            c["continents"].append(r["cont"])
        if r.get("lang") and r["lang"] not in c["langs"]:
            c["langs"].append(r["lang"])
    for c in countries.values():
        # Wikidata lists the languages of the territories but not English
        if c["code"] in LANG_OVERRIDE:
            c["langs"] = LANG_OVERRIDE[c["code"]] + [l for l in c["langs"] if l not in LANG_OVERRIDE[c["code"]]]
        if "en" in c["langs"]:
            c["langs"] = ["en"] + [l for l in c["langs"] if l != "en"]
        region = REGION_OVERRIDE.get(c["code"])
        for q in c["continents"]:
            region = region or CONTINENTS.get(q)
        c["region"] = region or "Other"
        # Simpler English names than the formal ones
        c["name"] = {"People's Republic of China": "China", "Kingdom of the Netherlands": "Netherlands",
                     "United States of America": "United States", "State of Palestine": "Palestine"
                     }.get(c["name"], c["name"])
    log(f"  {len(countries)} countries")
    return countries


#
# Wikidata labels, fetched in batches
#

def wikidata_labels(qids, langs):
    """Return {qid: {lang: label}} for the requested languages (plus en, mul)."""
    langs = sorted(set(langs) - {"en", "mul"})
    # The API takes at most 50 languages per request
    chunks = [["en", "mul"] + langs[i:i + 40] for i in range(0, max(len(langs), 1), 40)]
    out = {}
    qids = sorted(set(q for q in qids if q and q.startswith("Q")))
    for i in range(0, len(qids), 50):
        batch = qids[i:i + 50]
        for chunk in chunks:
            # Labels only: the claims of a large city run to megabytes
            try:
                d = mwapi("www.wikidata.org", action="wbgetentities", ids="|".join(batch),
                          props="labels", languages="|".join(chunk))
            except Exception as e:  # a lost batch only costs a few names
                log(f"  labels batch skipped: {e}")
                continue
            if "error" in d:
                log(f"  wbgetentities: {d['error'].get('info')}")
            for qid, ent in d.get("entities", {}).items():
                labels = out.setdefault(qid, {})
                labels.update({k: v["value"] for k, v in ent.get("labels", {}).items()})
        if i and not i % 1000:
            log(f"  labels {i}/{len(qids)}")
    return out


def pick_label(labels, langs, prefer_native=False):
    if not labels:
        return ""
    if prefer_native and labels.get("native"):
        return labels["native"]
    for lang in list(langs) + ["mul", "en"]:
        if labels.get(lang):
            return labels[lang]
    return next(iter(labels.values()), "")


#
# Provinces, states and regions of the places, through Wikidata
#

def regions_of(places, countries):
    """{place qid: region qid}: the most local division above each place
    that has an ISO 3166-2 code (provinces in Spain and Italy, states in the
    United States, departments in France...)."""
    qids = [q for q in places if q and q.startswith("Q")]
    candidates, parents = {}, {}
    # DISTINCT matters: the query service repeats a row for every path
    # between a place and its region, which can run to millions of rows
    for i in range(0, len(qids), 50):
        values = " ".join("wd:" + q for q in qids[i:i + 50])
        try:
            rows = sparql(f"""SELECT DISTINCT ?p ?r ?up WHERE {{ VALUES ?p {{ {values} }}
  ?p wdt:P131* ?r . ?r wdt:P300 ?iso . OPTIONAL {{ ?r wdt:P131 ?up }} }}""")
        except Exception as e:
            log(f"  regions skipped: {e}")
            continue
        for r in rows:
            candidates.setdefault(r["p"], set()).add(r["r"])
            if r.get("up"):
                parents.setdefault(r["r"], set()).add(r["up"])
    out = {}
    for place, regions in candidates.items():
        # The lowest one is not the parent of any other candidate
        higher = {up for r in regions for up in parents.get(r, ())}
        lowest = sorted(regions - higher) or sorted(regions)
        out[place] = lowest[0]
    return out


REGION_PREFIX = re.compile(r"^(province of|provincia de|provincia di|province de|prov[ií]ncia de|"
                           r"prov[ií]ncia d'|state of|regi[oó]n de|region of)\s+", re.I)
REGION_ALIASES = {"ES": {"Gerona": "Girona", "Lérida": "Lleida", "Vizcaya": "Bizkaia",
                         "Guipúzcoa": "Gipuzkoa", "Orense": "Ourense", "La Coruña": "A Coruña",
                         "Baleares": "Illes Balears", "Islas Baleares": "Illes Balears",
                         "Álava": "Álava", "Araba": "Álava", "Castellón de la Plana": "Castellón"},
                  "US": {"Arisona": "Arizona"}}


def clean_region(name, cc):
    """One spelling per region: no "Province of", the official local name."""
    name = REGION_PREFIX.sub("", name or "").strip()
    name = name[:1].upper() + name[1:]
    return REGION_ALIASES.get(cc, {}).get(name, name)


def add_regions(entries, countries):
    """Fill "region" from the place QIDs kept in "place_qid"."""
    places = {e["place_qid"]: e["cc"] for e in entries if e.get("place_qid") and not e.get("region")}
    if not places:
        return
    log(f"  Regions of {len(places)} places...")
    region_qid = regions_of(places, countries)
    langs = {l for e in entries for l in countries[e["cc"]]["langs"]}
    labels = wikidata_labels(set(region_qid.values()), langs)
    for e in entries:
        q = region_qid.get(e.get("place_qid"))
        if q and not e.get("region"):
            e["region"] = pick_label(labels.get(q, {}), countries[e["cc"]]["langs"])
    for e in entries:
        if e.get("region"):
            e["region"] = clean_region(e["region"], e["cc"])


#
# Source 1: Wikidata
#

def from_wikidata(countries, only=None):
    log("Stations (Wikidata)...")
    rows = sparql(f"""
SELECT ?st ?f ?c ?loc ?lic ?city ?hq WHERE {{
  ?st p:P2144 ?fs ; wdt:P17 ?c .
  ?fs psv:P2144 ?v . ?v wikibase:quantityUnit wd:Q732707 ; wikibase:quantityAmount ?f .
  FILTER(?f >= {FM_MIN} && ?f <= {FM_MAX})
  FILTER NOT EXISTS {{ ?st wdt:P576 ?closed }}
  OPTIONAL {{ ?fs pq:P276|pq:P131|pq:P1001 ?loc }}
  OPTIONAL {{ ?st wdt:P1408 ?lic }}
  OPTIONAL {{ ?st wdt:P131 ?city }}
  OPTIONAL {{ ?st wdt:P159 ?hq }}
}}""")
    by_qid = {c["qid"]: c for c in countries.values()}
    entries = {}
    for r in rows:
        country = by_qid.get(r["c"])
        if not country or (only and country["code"] not in only):
            continue
        key = (r["st"], round(float(r["f"]), 2), r.get("loc", ""))
        e = entries.setdefault(key, {"cc": country["code"], "qid": r["st"],
                                     "f": round(float(r["f"]), 2), "place": None})
        # Transmitter location, then city of license, then the admin area
        e["place"] = e["place"] or r.get("loc") or r.get("lic") or r.get("city") or r.get("hq")
    log(f"  {len(entries)} frequencies")
    langs = {l for e in entries.values() for l in countries[e["cc"]]["langs"]}
    labels = wikidata_labels([e["qid"] for e in entries.values()] +
                             [e["place"] for e in entries.values()], langs)
    out = []
    for e in entries.values():
        country = countries[e["cc"]]
        st = labels.get(e["qid"], {})
        name = pick_label(st, country["langs"])
        if not name or re.fullmatch(r"Q\d+", name):
            continue
        place = labels.get(e["place"], {}) if e["place"] else {}
        city = pick_label(place, country["langs"], prefer_native=True)
        out.append({"cc": e["cc"], "f": e["f"], "name": name, "name_en": st.get("en", ""),
                    "city": city, "city_en": place.get("en", ""), "ps": "", "src": "Wikidata",
                    "place_qid": e["place"]})
    log(f"  {len(out)} named stations")
    return out


#
# Source 2: Wikipedia station tables
#

class TableParser(html.parser.HTMLParser):
    """Collect the cells of every wikitable, with the latest section heading
    and the first wiki link found in each cell."""

    def __init__(self, all_tables=False):
        super().__init__(convert_charrefs=True)
        self.all_tables = all_tables
        self.tables = []
        self.heading = ""
        self._stack = []        # nested tables
        self._cell = None
        self._in_heading = False
        self._heading_text = ""
        self._skip = 0          # inside <sup>, <style>, hidden spans

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag in ("sup", "style", "script"):
            self._skip += 1
            return
        if tag in ("h2", "h3", "h4"):
            self._in_heading, self._heading_text = True, ""
        elif tag == "table":
            self._stack.append({"rows": [], "heading": self.heading,
                                "wikitable": "wikitable" in a.get("class", "")})
        elif tag == "tr" and self._stack:
            self._stack[-1]["rows"].append([])
        elif tag in ("td", "th") and self._stack and self._stack[-1]["rows"] is not None:
            if not self._stack[-1]["rows"]:
                self._stack[-1]["rows"].append([])
            self._cell = {"text": "", "link": None, "th": tag == "th",
                          "rowspan": int(re.sub(r"\D", "", a.get("rowspan", "1")) or 1),
                          "colspan": int(re.sub(r"\D", "", a.get("colspan", "1")) or 1)}
            self._stack[-1]["rows"][-1].append(self._cell)
        elif tag == "a" and self._cell is not None and self._cell["link"] is None:
            href = a.get("href", "")
            if href.startswith("/wiki/") and ":" not in href[6:]:
                self._cell["link"] = urllib.parse.unquote(href[6:]).replace("_", " ")
        elif tag == "br" and self._cell is not None:
            self._cell["text"] += "\n"

    def handle_endtag(self, tag):
        if tag in ("sup", "style", "script"):
            self._skip = max(0, self._skip - 1)
            return
        if tag in ("h2", "h3", "h4") and self._in_heading:
            self._in_heading = False
            self.heading = clean(self._heading_text.replace("[edit]", ""))
        elif tag in ("td", "th"):
            self._cell = None
        elif tag == "table" and self._stack:
            t = self._stack.pop()
            if t["wikitable"] or self.all_tables:
                self.tables.append(t)

    def handle_data(self, data):
        if self._skip:
            return
        if self._in_heading:
            self._heading_text += data
        if self._cell is not None:
            self._cell["text"] += data


def expand_spans(rows):
    """Turn rows of cells with rowspan/colspan into a rectangular grid."""
    grid, pending = [], {}
    for row in rows:
        out, col = [], 0
        cells = list(row)
        while cells or col in pending:
            if col in pending:
                cell, left = pending[col]
                out.append(cell)
                if left > 1:
                    pending[col] = (cell, left - 1)
                else:
                    del pending[col]
                col += 1
                continue
            cell = cells.pop(0)
            for _ in range(cell["colspan"]):
                out.append(cell)
                if cell["rowspan"] > 1:
                    pending[col] = (cell, cell["rowspan"] - 1)
                col += 1
        grid.append(out)
    return grid


HEAD_FREQ = re.compile(r"frequen|freq|mhz|^fm$|frecuencia|fréquence|frequenz|częstotliwo|frequência|frequentie", re.I)
HEAD_NAME = re.compile(r"^name|station|branding|brand|nombre|emisora|nom\b|radio|sender|programm|nazwa|nome|zender|network|call", re.I)
HEAD_CALL = re.compile(r"call ?sign|indicativo|callsign", re.I)
HEAD_PS = re.compile(r"\brds\b|^ps$|\bps[ -]?name|programme service|program service", re.I)
HEAD_CITY = re.compile(r"city|town|locat|licen|communit|area|market|region|site|transmit|coverage|municip|served|ciudad|localidad|municipio|ville|commune|ort|standort|miasto|cidade|localidade|plaats|provincia|province|state|county|serving", re.I)


def classify(header):
    cols = {}
    for i, cell in enumerate(header):
        text = clean(cell["text"]).lower()
        if not text or re.search(r"web|url|homepage|internet|e-mail|email|twitter|stream", text):
            continue
        if "ps" not in cols and HEAD_PS.search(text) and not HEAD_FREQ.search(text):
            cols["ps"] = i
        elif "freq" not in cols and HEAD_FREQ.search(text):
            cols["freq"] = i
        elif "call" not in cols and HEAD_CALL.search(text):
            cols["call"] = i
        elif "name" not in cols and HEAD_NAME.search(text):
            cols["name"] = i
        elif "city" not in cols and HEAD_CITY.search(text):
            cols["city"] = i
    return cols


def parse_matrix(grid, header_idx, cols):
    """Tables of transmitters, one column per program, frequencies in the
    cells: the column header names the station, the row names the place."""
    header, rows = grid[header_idx], grid[header_idx + 1:]
    if not rows:
        return []
    station_cols = []
    for j, cell in enumerate(header):
        title = clean(cell["text"])
        if not title or HEAD_FREQ.search(title) or HEAD_CITY.search(title) or j == cols.get("city"):
            continue
        hits = sum(1 for r in rows if len(r) > j and parse_freqs(r[j]["text"]))
        if hits >= max(2, len(rows) // 2):
            station_cols.append((j, title))
    if len(station_cols) < 2:
        return []
    used = {j for j, _ in station_cols}
    site = cols.get("city", next((j for j in range(len(header)) if j not in used), None))
    out = []
    for r in rows:
        city, link = "", None
        if site is not None and len(r) > site:
            city, link = clean(r[site]["text"].split("\n")[0].split(",")[0]), r[site]["link"]
        for j, title in station_cols:
            if len(r) > j:
                for f in parse_freqs(r[j]["text"]):
                    out.append({"f": f, "name": title, "city": city, "link": link, "ps": ""})
    return out


def parse_wiki_page(host, title, default_name=None):
    d = mwapi(host, action="parse", page=title, prop="text", redirects=1)
    if "parse" not in d:
        return []
    p = TableParser()
    p.feed(d["parse"]["text"])
    out = []
    for table in p.tables:
        grid = expand_spans(table["rows"])
        if len(grid) < 2:
            continue
        header_idx = next((i for i, r in enumerate(grid[:3]) if r and all(c["th"] for c in r)), 0)
        cols = classify(grid[header_idx])
        if "freq" not in cols:
            out += parse_matrix(grid, header_idx, cols)
            continue
        for row in grid[header_idx + 1:]:
            if len(row) <= cols["freq"] or all(c["th"] for c in row):
                continue
            freqs = parse_freqs(row[cols["freq"]]["text"])
            if not freqs:
                continue
            name = ""
            if "name" in cols and len(row) > cols["name"]:
                name = clean(row[cols["name"]]["text"].split("\n")[0])
            if not name and "call" in cols and len(row) > cols["call"]:
                name = clean(row[cols["call"]]["text"].split("\n")[0])
            if not name:
                name = default_name or table["heading"]
            if not name or len(name) > 60 or parse_freqs(name):
                continue
            city, link = "", None
            if "city" in cols and len(row) > cols["city"]:
                cell = row[cols["city"]]
                city = clean(cell["text"].split("\n")[0].split(",")[0])
                link = cell["link"]
                if re.search(r"https?:|www\.|\.(com|org|net|es|fm)\b", city, re.I):
                    city, link = "", None
            ps = ""
            if "ps" in cols and len(row) > cols["ps"]:
                ps = clean(row[cols["ps"]]["text"].split("\n")[0]).strip('"\'')
                if len(ps) > PS_LEN or parse_freqs(ps):
                    ps = ""
            for f in freqs:
                out.append({"f": f, "name": name, "city": city, "link": link, "ps": ps})
    return out


def wiki_country(title, countries):
    """Country of a "List of radio stations in X" article."""
    m = re.search(r"\bin (?:the )?(.+)$", title)
    if not m:
        return None
    place = m.group(1).strip()
    names = {c["name"].lower(): c["code"] for c in countries.values()}
    names.update({"the united states": "US", "united states": "US", "the uk": "GB",
                  "united kingdom": "GB", "the united kingdom": "GB", "britain": "GB",
                  "south korea": "KR", "north korea": "KP", "russia": "RU",
                  "the czech republic": "CZ", "czech republic": "CZ", "ivory coast": "CI",
                  "the philippines": "PH", "the netherlands": "NL", "taiwan": "TW",
                  "vietnam": "VN", "laos": "LA", "syria": "SY", "iran": "IR",
                  "the democratic republic of the congo": "CD", "east timor": "TL",
                  "brunei": "BN", "moldova": "MD", "bolivia": "BO", "venezuela": "VE",
                  "tanzania": "TZ", "the gambia": "GM", "cape verde": "CV",
                  "macau": "MO", "hong kong": "HK", "palestine": "PS", "kosovo": "XK"})
    return names.get(place.lower())


def category_pages(host, category, depth=1, limit=300):
    """Articles of a category and of its direct subcategories."""
    pages, todo, seen = [], [(category, 0)], set()
    while todo and len(pages) < limit:
        cat, level = todo.pop(0)
        if cat in seen:
            continue
        seen.add(cat)
        cont = {}
        while True:
            try:
                d = mwapi(host, action="query", list="categorymembers", cmtitle=cat,
                          cmlimit=500, **cont)
            except Exception as e:
                log(f"  {host} {cat}: {e}")
                break
            for m in d.get("query", {}).get("categorymembers", []):
                if m["ns"] == 14 and level < depth:
                    todo.append((m["title"], level + 1))
                elif m["ns"] == 0:
                    pages.append(m["title"])
            if "continue" in d:
                cont = d["continue"]
            else:
                break
    return sorted(set(pages))[:limit]


def from_wikipedia(countries, only=None, categories=True):
    log("Stations (Wikipedia)...")
    pages = []
    todo, seen = [WIKI_CATEGORY], set()
    while todo:
        cat = todo.pop()
        if cat in seen:
            continue
        seen.add(cat)
        cont = {}
        while True:
            d = mwapi("en.wikipedia.org", action="query", list="categorymembers",
                      cmtitle=cat, cmlimit=500, **cont)
            for m in d["query"]["categorymembers"]:
                if m["ns"] == 14 and len(seen) < 60:
                    todo.append(m["title"])
                elif m["ns"] == 0:
                    pages.append(("en", m["title"], None))
            if "continue" in d:
                cont = d["continue"]
            else:
                break
    pages = [(h, t, wiki_country(t, countries), None) for h, t, _ in sorted(set(pages))]
    pages = [p for p in pages if p[2]]
    pages += [(lang, title, cc, None) for lang, title, cc in EXTRA_WIKI_PAGES]
    # Articles about the stations and networks themselves, whose own
    # tables list their transmitters, named after the article
    for lang, category, cc in STATION_CATEGORIES if categories else []:
        if only and cc not in only:
            continue
        for title in category_pages(f"{lang}.wikipedia.org", category):
            pages.append((lang, title, cc, re.sub(r"\s*\([^)]*\)$", "", title)))
    if only:
        pages = [p for p in pages if p[2] in only]
    out, links = [], {}
    for lang, title, cc, default_name in pages:
        host = f"{lang}.wikipedia.org" if "." not in lang else lang
        try:
            rows = parse_wiki_page(host, title, default_name)
        except Exception as e:  # a single broken page must not stop the build
            log(f"  {lang}:{title}: {e}")
            continue
        if rows:
            log(f"  {lang}:{title} -> {cc}: {len(rows)}")
        for r in rows:
            r.update(cc=cc, src=f"Wikipedia ({lang}): {title}", host=host)
            if r["link"]:
                links.setdefault(host, set()).add(r["link"])
            out.append(r)
    # Municipalities: follow the links to Wikidata for the local spelling
    log("  Local municipality names...")
    title_qid = {}
    for host, titles in links.items():
        titles = sorted(titles)
        for i in range(0, len(titles), 50):
            d = mwapi(host, action="query", prop="pageprops", ppprop="wikibase_item",
                      redirects=1, titles="|".join(titles[i:i + 50]))
            q = d.get("query", {})
            redirects = {r["from"]: r["to"] for r in q.get("redirects", [])}
            normalized = {n["from"]: n["to"] for n in q.get("normalized", [])}
            found = {p["title"]: p.get("pageprops", {}).get("wikibase_item")
                     for p in q.get("pages", [])}
            for t in titles[i:i + 50]:
                t2 = normalized.get(t, t)
                t2 = redirects.get(t2, t2)
                if found.get(t2):
                    title_qid[(host, t)] = found[t2]
    langs = {l for r in out for l in countries[r["cc"]]["langs"]}
    labels = wikidata_labels(set(title_qid.values()), langs)
    for r in out:
        qid = title_qid.get((r["host"], r["link"])) if r["link"] else None
        place = labels.get(qid, {}) if qid else {}
        local = pick_label(place, countries[r["cc"]]["langs"], prefer_native=True) if place else ""
        # Only trust the link when it points at the place named in the cell
        if local and place.get("en") and norm_key(place["en"])[:4] == norm_key(r["city"])[:4]:
            r["city_en"] = place.get("en", "")
            r["city"] = local
        elif local and norm_key(local)[:4] == norm_key(r["city"])[:4]:
            r["city"] = local
        r.setdefault("city_en", "")
        r["place_qid"] = qid
        r["name_en"] = r["name"] if r["host"].startswith("en.") else ""
        del r["link"], r["host"]
    log(f"  {len(out)} stations")
    return out


#
# Source 3: FCC (United States), public domain
#

FCC_URL = ("https://transition.fcc.gov/fcc-bin/fmq?state=&call=&city=&arn=&serv=&vac=3"
           "&freq=0.0&fre2=107.9&facid=&asrn=&class=&list=4&NextTab=Results+to+Next+Page%2FTab"
           "&dist=&dlat2=&mlat2=&slat2=&NS=N&dlon2=&mlon2=&slon2=&EW=W&size=9")


def from_fcc():
    log("Stations (FCC)...")
    raw = fetch(FCC_URL).decode("latin-1")
    out = []
    for line in raw.splitlines():
        f = [x.strip() for x in line.split("|")]
        if len(f) < 12:
            continue
        call, freq, service, status, city, state = f[1], f[2], f[3], f[9], f[10], f[11]
        if service not in ("FM", "FL") or status != "LIC":
            continue
        try:
            mhz = float(freq.split()[0])
        except ValueError:
            continue
        if not FM_MIN <= mhz <= FM_MAX:
            continue
        city = city.title()
        out.append({"cc": "US", "f": round(mhz, 2), "name": call, "name_en": call,
                    "city": f"{city} {state}".strip(), "city_en": "", "ps": "", "src": "FCC"})
    log(f"  {len(out)} stations")
    return out


#
# Source 4: REMER vademecum (Spain), from the Spanish Directorate-General
# for Civil Protection and Emergencies, one page per province
#

REMER_URL = "https://08pc10.es/08vdmr/vdmr08/vdm027fm{:02d}.htm"
REMER_PROVINCES = [
    "Álava", "Albacete", "Alicante", "Almería", "Ávila", "Badajoz", "Illes Balears", "Barcelona",
    "Burgos", "Cáceres", "Cádiz", "Castellón", "Ciudad Real", "Córdoba", "A Coruña", "Cuenca",
    "Girona", "Granada", "Guadalajara", "Gipuzkoa", "Huelva", "Huesca", "Jaén", "León", "Lleida",
    "La Rioja", "Lugo", "Madrid", "Málaga", "Murcia", "Navarra", "Ourense", "Asturias", "Palencia",
    "Las Palmas", "Pontevedra", "Salamanca", "Santa Cruz de Tenerife", "Cantabria", "Segovia",
    "Sevilla", "Soria", "Tarragona", "Teruel", "Toledo", "Valencia", "Valladolid", "Bizkaia",
    "Zamora", "Zaragoza", "Ceuta", "Melilla"]
REMER_NETWORKS = {"SER": "Cadena SER", "RNE": "RNE", "COPE": "COPE", "OC": "Onda Cero",
                  "CCAA": "Regional radio", "PRI": "Local station", "MUN": "Municipal radio",
                  "CAD100": "Cadena 100", "40P": "LOS40", "DIAL": "Cadena Dial"}
SMALL_WORDS = {"de", "del", "la", "las", "los", "el", "y", "i", "e", "d", "l", "da", "do", "das", "dos", "a"}


def smart_title(text):
    words = text.lower().split()
    out = []
    for i, w in enumerate(words):
        if i and w in SMALL_WORDS:
            out.append(w)
        else:
            out.append("-".join(part[:1].upper() + part[1:] for part in w.split("-")))
    return " ".join(out).replace("L'", "l'") if out else ""


def from_remer():
    log("Stations (REMER, Spain)...")
    out = []
    for n in range(1, 53):
        try:
            raw = fetch(REMER_URL.format(n))
        except Exception as e:
            log(f"  page {n}: {e}")
            continue
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            text = raw.decode("windows-1252", "replace")
        p = TableParser(all_tables=True)
        p.feed(text)
        for table in p.tables:
            grid = expand_spans(table["rows"])
            header = next((i for i, r in enumerate(grid[:3])
                           if any("frec" in clean(c["text"]).lower() for c in r)), None)
            if header is None:
                continue
            heads = [clean(c["text"]).lower() for c in grid[header]]
            col = lambda key: next((i for i, h in enumerate(heads) if key in h), None)
            cf, cl, cc, ci = col("frec"), col("localidad"), col("cadena"), col("identif")
            for row in grid[header + 1:]:
                if cf is None or len(row) <= cf:
                    continue
                freqs = parse_freqs(row[cf]["text"])
                cell = lambda c: clean(row[c]["text"]) if c is not None and len(row) > c else ""
                city, network, ident = smart_title(cell(cl)), cell(cc).upper(), cell(ci)
                ident = re.sub(r"^R\.\s*", "", ident).strip()
                if network == "RNE" and re.fullmatch(r"\d", ident):
                    name = f"RNE Radio {ident}"
                elif ident:
                    name = brand_title(ident)
                else:
                    name = REMER_NETWORKS.get(network, network.title() or "Local station")
                for f in freqs:
                    out.append({"cc": "ES", "f": f, "name": name, "name_en": "", "city": city,
                                "city_en": "", "ps": "", "region": REMER_PROVINCES[n - 1],
                                "src": "REMER vademecum (DGPCE, Spain)"})
    # The pages lost their accented letters, put the official spelling back
    official = spanish_municipalities()
    fixed = 0
    for e in out:
        good = official.get(fold(e["city"]))
        if not good and "�" in e["city"]:
            pattern = re.compile("^" + re.escape(fold(e["city"])).replace("\\?", ".") + "$")
            matches = [v for k, v in official.items() if pattern.match(k)]
            good = matches[0] if len(matches) == 1 else None
        if good:
            fixed += good != e["city"]
            e["city"] = good
        else:
            e["city"] = e["city"].replace("�", "")
    log(f"  {len(out)} stations, {fixed} municipality names corrected")
    return out


KEEP_UPPER = {"FM", "AM", "TV", "SER", "COPE", "RNE", "RAC", "CCAA", "RTVE", "EITB", "CRTVG",
              "IB3", "APR", "TPA", "CMM", "RTPA", "UPV", "UNED", "DAB", "CAD", "BBC", "ERT"}


def brand_title(text):
    """Station labels written in capitals, in title case, keeping acronyms."""
    if not text.isupper() or len(text) < 5:
        return text
    words = []
    for w in text.split():
        words.append(w if w in KEEP_UPPER or any(ch.isdigit() for ch in w) else smart_title(w))
    return " ".join(words)


def fold(text):
    """Accent-free lowercase key; the lost letters become '?'."""
    text = text.replace("�", "?").lower()
    text = unicodedata.normalize("NFKD", text)
    return "".join(c for c in text if not unicodedata.combining(c))


def spanish_municipalities():
    rows = sparql("""
SELECT ?name WHERE {
  ?m wdt:P31 wd:Q2074737 ; rdfs:label ?name .
  FILTER(LANG(?name) = "es")
}""")
    out = {}
    for r in rows:
        out.setdefault(fold(r["name"]), r["name"])
        # Bilingual names such as "Vitoria-Gasteiz" or "Donostia/San Sebastian"
        for part in re.split(r"[-/]", r["name"]):
            out.setdefault(fold(part.strip()), r["name"])
    return out


#
# Hand-maintained additions
#

def from_manual():
    out = []
    if not os.path.isdir(MANUAL_DIR):
        return out
    for name in sorted(os.listdir(MANUAL_DIR)):
        if not name.endswith(".json"):
            continue
        cc = name[:-5].upper()
        with open(os.path.join(MANUAL_DIR, name), encoding="utf-8") as f:
            data = json.load(f)
        for s in data.get("stations", []):
            out.append({"cc": cc, "f": round(float(s["f"]), 2), "name": s["name"],
                        "name_en": s.get("name_en", ""), "city": s.get("city", ""),
                        "city_en": s.get("city_en", ""), "ps": s.get("ps", ""),
                        "region": s.get("region", ""), "src": "Manual", "manual": True})
    log(f"Stations (manual): {len(out)}")
    return out


#
# Merge and write
#

def merge(entries):
    """De-duplicate by country, frequency and municipality. Manual entries win,
    then the entries carrying a municipality, then the first source seen."""
    best = {}
    for e in entries:
        city_key = norm_key(e.get("city_en") or e["city"])[:10]
        key = (e["cc"], e["f"], city_key)
        old = best.get(key)
        if old is None or (e.get("manual") and not old.get("manual")):
            best[key] = e
        elif not old.get("manual") and len(e["name"]) > 2 and len(old["name"]) <= 2:
            best[key] = e
        elif not old.get("ps") and e.get("ps"):
            # Keep the expected RDS name from whichever source carries it
            old["ps"] = e["ps"]
    # A national network listed without a municipality is dropped when the
    # same frequency appears with one
    with_city = {(e["cc"], e["f"], norm_key(e["name"])) for e in best.values() if e["city"]}
    return [e for e in best.values()
            if e["city"] or (e["cc"], e["f"], norm_key(e["name"])) not in with_city]


def write(countries, entries, partial=False):
    """Write one file per country and the index. A partial build keeps the
    countries already in the index, so the lists can be published bit by bit."""
    os.makedirs(OUT_DIR, exist_ok=True)
    today = datetime.date.today().isoformat()
    per_cc = {}
    for e in entries:
        per_cc.setdefault(e["cc"], []).append(e)
    regions = {}
    index_path = os.path.join(OUT_DIR, "index.json")
    if partial and os.path.exists(index_path):
        with open(index_path, encoding="utf-8") as f:
            for region in json.load(f).get("regions", []):
                for c in region["countries"]:
                    if c["code"] not in per_cc:
                        regions.setdefault(region["name"], []).append(c)
    for cc, items in sorted(per_cc.items()):
        country = countries.get(cc)
        if not country:
            continue
        stations = []
        for e in sorted(items, key=lambda e: (e["f"], norm_key(e["city"]), norm_key(e["name"]))):
            n = to_ascii(e["name"], e.get("name_en"))[:NAME_LEN].strip()
            c = to_ascii(e["city"], e.get("city_en"))[:CITY_LEN].strip()
            if not n:
                continue
            s = {"f": e["f"], "n": n, "c": c}
            ps = to_ascii(e.get("ps", ""))[:PS_LEN]
            if ps:
                s["p"] = ps
            region = e.get("region") or ""
            r = to_ascii(region)[:REGION_LEN].strip()
            if r:
                s["r"] = r
                if region != r:
                    s["region"] = region
            if e["name"] != n:
                s["name"] = e["name"]
            if e["city"] != c:
                s["city"] = e["city"]
            stations.append(s)
        sources = sorted({e["src"] for e in items})
        doc = {"format": FORMAT_VERSION, "code": cc, "country": country["name"],
               "region": country["region"], "generated": today, "sources": sources,
               "stations": stations}
        with open(os.path.join(OUT_DIR, f"{cc}.json"), "w", encoding="utf-8") as f:
            json.dump(doc, f, ensure_ascii=False, separators=(",", ":"))
            f.write("\n")
        regions.setdefault(country["region"], []).append(
            {"code": cc, "name": to_ascii(country["name"]), "count": len(stations)})
    index = {"format": FORMAT_VERSION, "generated": today, "regions": []}
    for region in REGION_ORDER + sorted(set(regions) - set(REGION_ORDER)):
        if region in regions:
            index["regions"].append({"name": region, "countries":
                                     sorted(regions[region], key=lambda c: c["name"])})
    with open(index_path, "w", encoding="utf-8") as f:
        json.dump(index, f, ensure_ascii=False, indent=1)
        f.write("\n")
    total = sum(len(v) for v in per_cc.values())
    log(f"Wrote {len(per_cc)} countries, {total} entries")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--only", help="comma separated ISO codes to rebuild")
    ap.add_argument("--skip-fcc", action="store_true")
    ap.add_argument("--skip-wikipedia", action="store_true")
    ap.add_argument("--skip-categories", action="store_true",
                    help="skip the station articles of the local-language Wikipedias (slow)")
    args = ap.parse_args()
    only = {c.strip().upper() for c in args.only.split(",")} if args.only else None

    countries = load_countries()
    entries = []
    entries += from_wikidata(countries, only)
    if not args.skip_wikipedia:
        entries += from_wikipedia(countries, only, not args.skip_categories)
    if not args.skip_fcc and (not only or "US" in only):
        try:
            entries += from_fcc()
        except Exception as e:
            log(f"  FCC unavailable: {e}")
    if not only or "ES" in only:
        try:
            entries += from_remer()
        except Exception as e:
            log(f"  REMER unavailable: {e}")
    entries += from_manual()
    if only:
        entries = [e for e in entries if e["cc"] in only]
    entries = merge(entries)
    add_regions(entries, countries)
    write(countries, entries, partial=bool(only))


if __name__ == "__main__":
    main()
