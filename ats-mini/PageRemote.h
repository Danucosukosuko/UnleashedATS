#ifndef PAGE_REMOTE_H
#define PAGE_REMOTE_H

// Generated from web/remote.html by tools/embed_web.py, do not edit.
// The same page is served by the PC companion, which adds the audio.
static const char pageRemote[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>UnleashedATS Remote</title>
<style>
:root{--bg:#0f0c08;--panel:#1c1710;--panel2:#262014;--line:#3a3020;--text:#f2e6cf;--muted:#a8946f;--accent:#d9a441;--accent2:#f0c46a;--good:#59c36a;--bad:#e35d4f}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg);color:var(--text);font:14px/1.35 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
.wrap{max-width:1180px;margin:0 auto;padding:14px 18px 70px}
.panel{background:var(--panel);border-radius:14px;padding:12px 14px;position:relative}
.label{font-size:20px;color:var(--accent);text-align:center;letter-spacing:.02em}
.top{display:flex;align-items:center;gap:22px}
.top .name{font-size:22px;color:var(--accent)}
.tbtn{display:flex;flex-direction:column;align-items:center;gap:2px;font-size:11px;color:var(--text);background:none;border:0;cursor:pointer;padding:2px 8px}
.tbtn svg{width:20px;height:20px;fill:var(--accent)}
.tbtn.on{color:var(--accent2)}
.top .right{margin-left:auto;display:flex;gap:14px;align-items:center;color:var(--muted)}
.top .users{color:var(--text)}
canvas#graph{width:100%;height:130px;display:block;margin:10px 0}
.grid{display:grid;grid-template-columns:80px 1.25fr 1fr 1fr 110px;grid-auto-rows:auto;gap:12px}
.af{grid-column:5;grid-row:1/5;text-align:center}
.af .label{margin:6px 0 10px}
.af button{display:block;width:100%;background:none;border:0;color:var(--text);font:15px "Consolas","DejaVu Sans Mono",monospace;padding:3px 0;cursor:pointer}
.af button.cur{color:var(--accent2);font-weight:700}
.play{grid-column:1;background:var(--accent);border:0;border-radius:14px;display:grid;place-items:center;cursor:pointer;min-height:90px}
.play svg{width:26px;fill:#1a1206}
.play[disabled]{opacity:.35;cursor:default}
.ps{grid-column:2;text-align:center;padding-top:26px}
.flags{position:absolute;top:9px;left:0;right:0;display:flex;justify-content:center;gap:12px;font-size:12px;font-weight:700;color:#5a4c34}
.flags .on{color:var(--text)}
.ps .name{font:700 40px/1.05 "Segoe UI",system-ui,sans-serif;letter-spacing:.02em;white-space:pre}
.ps .pty{color:var(--accent);font-size:17px;min-height:22px}
.audio{grid-column:3/5;display:grid;place-items:center;text-align:center}
.audio b{font-weight:500;font-size:17px}
.audio small{display:block;color:var(--muted)}
.audio input{width:180px;accent-color:var(--accent);margin-top:6px}
.pi{grid-column:1/3;display:grid;grid-template-columns:1fr;text-align:center;align-content:center}
.pi .val{font:300 52px/1.1 "Segoe UI",system-ui,sans-serif}
.freq{grid-column:3;display:grid;grid-template-columns:44px 1fr;gap:10px;align-items:center}
.bands{display:flex;flex-direction:column;gap:5px}
.bands button,.chip{background:var(--panel2);border:1px solid var(--line);color:var(--muted);border-radius:6px;font-size:11px;font-weight:700;padding:3px 0;cursor:pointer}
.bands button.on{background:var(--accent);color:#1a1206;border-color:var(--accent)}
.freq .val{font:600 54px/1 "Consolas","DejaVu Sans Mono",monospace;text-align:center;letter-spacing:-.02em;cursor:text}
.freq .sub{text-align:center;color:var(--muted);font-size:12px}
.meter{grid-column:4;text-align:center}
.scale{position:relative;height:42px;margin:8px 6px 0}
.scale .bar{position:absolute;left:0;right:0;top:12px;height:6px;background:#3a3020;border-radius:3px;overflow:hidden}
.scale .bar i{position:absolute;inset:0 auto 0 0;background:linear-gradient(90deg,var(--good) 0 52%,var(--bad) 52%);transition:width .3s}
.scale .peak{position:absolute;top:9px;width:3px;height:12px;background:var(--accent2);transition:left .3s}
.scale .ticks{position:absolute;left:0;right:0;top:0;display:flex;justify-content:space-between;font-size:10px;color:var(--text)}
.scale .ticks2{position:absolute;left:0;right:0;top:24px;display:flex;justify-content:space-between;font-size:11px;color:var(--text)}
.meter .num{font:600 22px "Consolas",monospace;margin-top:4px}
.meter .num small{font-size:12px;color:var(--muted);font-weight:400}
.bar2{grid-column:1/5;display:flex;gap:10px;align-items:center;background:var(--accent);border-radius:14px;padding:8px 12px;flex-wrap:wrap}
.bar2 button{background:none;border:0;color:#1a1206;font-weight:700;font-size:13px;padding:6px 10px;border-radius:8px;cursor:pointer}
.bar2 button.on{background:#1a1206;color:var(--accent2)}
.bar2 input[type=text]{background:#1a1206;color:var(--text);border:0;border-radius:8px;padding:6px 10px;width:110px;font-family:"Consolas",monospace}
.bar2 .vol{margin-left:auto;display:flex;align-items:center;gap:6px;color:#1a1206;font-weight:700;font-size:12px}
.bar2 .vol input{accent-color:#1a1206}
.rt{grid-column:1/3;text-align:center}
.rt .txt{font:15px "Consolas",monospace;min-height:40px;margin-top:6px;white-space:pre-wrap}
.rt .np{color:var(--accent,#d9a441);font-weight:600;margin-top:6px}.rt .np:empty{display:none}
.ident{grid-column:3/5;text-align:center}
.ident .city{margin-top:6px}
.ident .more{color:var(--muted);font-size:12px;margin-top:2px}
.eon{grid-column:1/6}
.eon table{width:100%;border-collapse:collapse;margin-top:6px}
.eon td{padding:5px 6px;border-top:1px solid var(--line)}
.eon td.n{font-weight:700;width:110px}
.eon td.p{color:var(--muted);font-family:"Consolas",monospace;width:60px}
.eon button,.presets button{background:var(--panel2);border:1px solid var(--line);color:var(--text);border-radius:8px;cursor:pointer}
.eon button{font:13px "Consolas",monospace;padding:3px 8px;margin-right:4px}
.presets{display:flex;gap:10px;margin-top:12px;align-items:stretch}
.presets select{background:var(--panel);color:var(--accent);border:1px solid var(--line);border-radius:10px;padding:0 8px;font-weight:700}
.presets button{flex:1;padding:6px 2px;font:600 13px/1.25 "Consolas",monospace;min-width:0}
.presets button.sel{border-color:var(--accent);box-shadow:0 0 0 1px var(--accent) inset}
.presets button.empty{color:#5a4c34}
.logger{margin-top:12px;display:none}
.logger.show{display:block}
.logger table{width:100%;border-collapse:collapse;font:12px "Consolas",monospace}
.logger td,.logger th{padding:3px 6px;border-top:1px solid var(--line);text-align:left}
.logger .tools{display:flex;justify-content:space-between;align-items:center}
.logger .tools button{background:var(--panel2);border:1px solid var(--line);color:var(--text);border-radius:8px;padding:4px 10px;cursor:pointer}
.empty{color:var(--muted);font-style:italic}
.help{position:fixed;right:18px;bottom:18px;width:30px;height:30px;border-radius:50%;background:#fff;color:#000;border:0;font-weight:700;cursor:pointer}
dialog{background:var(--panel2);color:var(--text);border:1px solid var(--line);border-radius:12px;max-width:520px;padding:18px 22px}
dialog::backdrop{background:#0008}
dialog h3{margin:0 0 10px;color:var(--accent);font-weight:500}
dialog p{margin:6px 0;text-align:center}
@media(max-width:900px){.grid{grid-template-columns:70px 1fr 1fr}.af{grid-column:1/4;grid-row:auto}.af button{display:inline-block;width:auto;padding:3px 8px}
 .ps{grid-column:2/4}.audio,.pi,.freq,.meter,.rt,.ident,.bar2,.eon{grid-column:1/4}.presets{flex-wrap:wrap}.presets button{flex:0 0 18%}}
</style>
</head>
<body>
<div class="wrap">
  <div class="panel top">
    <span class="name" id="host">UnleashedATS</span>
    <button class="tbtn" id="recBtn" hidden><svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="8"/></svg>Record</button>
    <button class="tbtn" id="scanBtn"><svg viewBox="0 0 24 24"><path d="M3 20h18v2H3zM5 17h2v-6H5zm4 0h2V7H9zm4 0h2v-9h-2zm4 0h2V4h-2z"/></svg>Seek band</button>
    <button class="tbtn" id="logBtn"><svg viewBox="0 0 24 24"><path d="M3 4h18v16H3zm2 2v3h6V6zm8 0v3h6V6zM5 11v3h6v-3zm8 0v3h6v-3zM5 16v2h6v-2zm8 0v2h6v-2z"/></svg>RDS Logger</button>
    <div class="right">
      <span id="fw"></span>
      <span class="users" id="users" hidden></span>
      <span id="conn">Connecting...</span>
    </div>
  </div>

  <canvas id="graph" width="1180" height="130" aria-label="Signal history"></canvas>

  <div class="grid">
    <button class="play" id="play" disabled aria-label="Play"><svg viewBox="0 0 24 24" id="playIcon"><path d="M7 4l13 8-13 8z"/></svg></button>
    <div class="panel ps">
      <div class="flags"><span id="fTp">TP</span><span id="fTa">TA</span><span id="fSt">ST</span><span id="fDx">DX</span></div>
      <div class="name" id="ps">&nbsp;</div>
      <div class="pty" id="pty"></div>
    </div>
    <div class="panel audio" id="audio">
      <div><b id="audioState">Standby</b><small id="audioSub">No active audio stream</small>
      <input type="range" id="gain" min="0" max="100" value="80" aria-label="Listening volume" hidden></div>
    </div>
    <div class="panel af"><div class="label">AF</div><div id="af"></div></div>

    <div class="panel pi"><div class="label">PI CODE</div><div class="val" id="pi">&nbsp;</div></div>
    <div class="panel freq">
      <div class="bands" id="bands">
        <button data-b="FM">FM</button><button data-b="SW">SW</button><button data-b="MW">MW</button><button data-b="LW">LW</button>
      </div>
      <div><div class="label">FREQUENCY</div><div class="val" id="freq" title="Type a frequency to tune">--.---</div>
        <div class="sub" id="sub"></div></div>
    </div>
    <div class="panel meter">
      <div class="label">SIGNAL</div>
      <div class="scale">
        <div class="ticks"><span>1</span><span>3</span><span>5</span><span>7</span><span>9</span><span>+10</span><span>+20</span><span>+30</span></div>
        <div class="bar"><i id="bar"></i></div><div class="peak" id="peak"></div>
        <div class="ticks2"><span>0</span><span>20</span><span>40</span><span>60</span><span>80</span></div>
      </div>
      <div class="num"><span id="rssi">--</span><small> dB&micro;V &middot; </small><span id="snr">--</span><small> dB SNR</small></div>
    </div>

    <div class="bar2">
      <button data-c="seek" data-d="-1" title="Seek down (Page Down)">&laquo; Seek</button>
      <button data-c="step" data-d="-1" title="Step down (Left)">&lsaquo; Step</button>
      <button data-c="step" data-d="1" title="Step up (Right)">Step &rsaquo;</button>
      <button data-c="seek" data-d="1" title="Seek up (Page Up)">Seek &raquo;</button>
      <input type="text" id="direct" placeholder="98.1" aria-label="Frequency">
      <button id="dxBtn" data-c="dx" title="Weak signal settings">DX</button>
      <button id="monoBtn" data-c="mono" title="Force mono">Mono</button>
      <button data-c="mode" data-d="1">Mode</button>
      <span class="vol">Radio vol <input type="range" id="vol" min="0" max="63" aria-label="Receiver volume"></span>
    </div>

    <div class="panel rt"><div class="label">RADIOTEXT</div><div class="np" id="np"></div><div class="txt" id="rt"></div></div>
    <div class="panel ident"><div class="label" id="identName">&nbsp;</div><div class="city" id="identCity"></div><div class="more" id="identMore"></div></div>

    <div class="panel eon"><div class="label">OTHER NETWORKS (EON)</div><table id="eon"></table></div>
  </div>

  <div class="presets" id="presets"></div>

  <div class="panel logger" id="logger">
    <div class="tools"><span class="label">RDS LOGGER</span><span><button id="logCsv">Download CSV</button> <button id="logClear">Clear</button></span></div>
    <table><thead><tr><th>Time</th><th>Freq</th><th>PI</th><th>PS</th><th>PTY</th><th>Radiotext</th></tr></thead><tbody id="logBody"></tbody></table>
  </div>
</div>
<button class="help" id="helpBtn" aria-label="Help">?</button>
<dialog id="help">
  <h3>Preset buttons</h3>
  <p>Four banks of ten presets, kept in this browser only.</p>
  <p><i>Left click</i> or <i>Enter</i> recalls a preset.</p>
  <p><i>Right click</i>, <i>Ctrl+click</i> or <i>Shift+S</i> stores the current station.</p>
  <p><i>Middle click</i>, <i>Shift+click</i> or <i>Shift+R</i> clears a preset.</p>
  <p><i>[</i> and <i>]</i> move between presets, the menu picks the bank.</p>
  <h3>Tuning</h3>
  <p><i>Left</i>/<i>Right</i> step, <i>Page Up</i>/<i>Page Down</i> seek, type digits and <i>Enter</i> to tune.</p>
</dialog>
<script>
"use strict";
const $ = id => document.getElementById(id);
const demo = location.protocol === "file:" || location.search.includes("demo");
let state = null, peak = 0, peakTime = 0, volTouched = 0, history = [], lastLog = "";

const DEMO = {fw:"v1.00-2.46", freq:89900000, band:"VHF", mode:"FM", step:"100k", bw:"Auto", agc:0, vol:42,
  rssi:56, snr:24, stereo:true, dx:true, tp:true, ta:false, mono:false, name:"HIT FM", ps:"HIT FM",
  listed:"Hit FM, Alcorcon", pi:"E2F1", pty:"Pop Music", rt:"HIT FM - Los exitos de hoy",
  af:[89900000, 92800000, 98500000, 102200000, 104600000, 106600000],
  eon:[{pi:"E211", ps:"SER", f:[105400000]}, {pi:"E215", ps:"LOS40", f:[93900000]}],
  audio:true, listening:false, users:1, host:"MADRID 2"};

const fmt = (hz, mode) => mode === "FM" ? (hz / 1e6).toFixed(3) : (hz / 1e3).toFixed(hz % 1000 ? 3 : 0);
const flag = (id, on) => $(id).classList.toggle("on", !!on);
const bandOf = s => s.mode === "FM" ? "FM" : s.freq < 520000 ? "LW" : s.freq < 1710000 ? "MW" : "SW";
const esc = t => String(t).replace(/[&<>"]/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]));

function render(s) {
  state = s;
  $("host").textContent = s.host ? "UnleashedATS | " + s.host : "UnleashedATS";
  $("fw").textContent = s.fw.replace(/^.*F\/W: /, "");
  $("ps").textContent = (s.ps || s.name || "").padEnd(8).slice(0, 8);
  $("pty").textContent = s.pty || "";
  flag("fTp", s.tp); flag("fTa", s.ta); flag("fSt", s.stereo); flag("fDx", s.dx);
  $("pi").textContent = s.pi || " ";
  $("freq").textContent = fmt(s.freq, s.mode);
  $("sub").textContent = s.band + " · " + s.mode + " · " + s.step + " · BW " + s.bw;
  document.querySelectorAll("#bands button").forEach(b => b.classList.toggle("on", b.dataset.b === bandOf(s)));
  $("dxBtn").classList.toggle("on", s.dx);
  $("monoBtn").classList.toggle("on", s.mono);
  $("rt").textContent = s.rt || "";
  // RT+ tags: what is playing
  $("np").textContent = [s.artist, s.title].filter(Boolean).join(" – ");

  // Station identification from the downloaded lists
  const [lname, ...lcity] = (s.listed || "").split(", ");
  $("identName").textContent = lname || s.name || " ";
  $("identCity").textContent = lcity.join(", ");
  $("identMore").textContent = s.listed ? "From the station list" : (s.pi ? "RDS" : "");

  const now = Date.now();
  if (s.rssi >= peak || now - peakTime > 2500) { peak = s.rssi; peakTime = now; }
  $("rssi").textContent = s.rssi; $("snr").textContent = s.snr;
  $("bar").style.width = Math.min(100, s.rssi / 80 * 100) + "%";
  $("peak").style.left = "calc(" + Math.min(100, peak / 80 * 100) + "% - 1px)";
  if (now - volTouched > 3000) $("vol").value = s.vol;

  $("af").innerHTML = s.af.length ? "" : '<span class="empty">none</span>';
  for (const f of s.af) {
    const b = document.createElement("button");
    b.textContent = (f / 1e6).toFixed(1);
    if (f === s.freq) b.className = "cur";
    b.onclick = () => send("tune", {f});
    $("af").appendChild(b);
  }

  $("eon").innerHTML = s.eon.length ? "" : '<tr><td class="empty">No other networks received</td></tr>';
  for (const n of s.eon) {
    const tr = $("eon").insertRow();
    tr.innerHTML = '<td class="n">' + esc(n.ps) + '</td><td class="p">' + esc(n.pi) + "</td><td></td>";
    for (const f of n.f) {
      const b = document.createElement("button");
      b.textContent = (f / 1e6).toFixed(1);
      b.onclick = () => send("tune", {f});
      tr.cells[2].appendChild(b);
    }
  }

  $("play").disabled = !s.audio;
  $("recBtn").hidden = !s.audio;
  $("recBtn").classList.toggle("on", !!s.recording);
  $("users").hidden = !s.audio;
  $("users").textContent = "\u{1F464} " + (s.users || 0);

  history.push(s.rssi);
  if (history.length > 240) history.shift();
  drawGraph();
  logRds(s);
}

function drawGraph() {
  const c = $("graph"), g = c.getContext("2d");
  const w = c.width = c.clientWidth, h = c.height;
  g.clearRect(0, 0, w, h);
  if (!history.length) return;
  let lo = Math.min(...history) - 2, hi = Math.max(...history) + 2;
  if (hi - lo < 6) { const m = (hi + lo) / 2; lo = m - 3; hi = m + 3; }
  g.font = "11px system-ui"; g.fillStyle = "#d9a441"; g.strokeStyle = "#2a2417";
  for (let i = 0; i <= 2; i++) {
    const v = lo + (hi - lo) * i / 2, y = h - 6 - (h - 12) * i / 2;
    g.beginPath(); g.moveTo(36, y); g.lineTo(w - 36, y); g.stroke();
    g.fillText(v.toFixed(1), 2, y + 4); g.fillText(v.toFixed(1), w - 32, y + 4);
  }
  g.strokeStyle = "#d9a441"; g.lineWidth = 2; g.beginPath();
  history.forEach((v, i) => {
    const x = 36 + (w - 72) * i / 239, y = h - 6 - (h - 12) * (v - lo) / (hi - lo);
    i ? g.lineTo(x, y) : g.moveTo(x, y);
  });
  g.stroke();
}

// RDS logger: one line per change of station data
const logRows = [];
function logRds(s) {
  if (!s.pi && !s.ps) return;
  const key = [s.freq, s.pi, s.ps, s.pty, s.rt].join("|");
  if (key === lastLog) return;
  lastLog = key;
  const row = [new Date().toLocaleTimeString(), fmt(s.freq, s.mode), s.pi, s.ps, s.pty, s.rt];
  logRows.unshift(row);
  if (logRows.length > 500) logRows.pop();
  const tr = $("logBody").insertRow(0);
  row.forEach(v => tr.insertCell().textContent = v);
  while ($("logBody").rows.length > 200) $("logBody").deleteRow(-1);
}
$("logBtn").onclick = () => { $("logger").classList.toggle("show"); $("logBtn").classList.toggle("on"); };
$("logClear").onclick = () => { logRows.length = 0; $("logBody").innerHTML = ""; };
$("logCsv").onclick = () => {
  const csv = "time,frequency,pi,ps,pty,radiotext\n" +
    logRows.slice().reverse().map(r => r.map(v => '"' + String(v).replace(/"/g, '""') + '"').join(",")).join("\n");
  const a = Object.assign(document.createElement("a"), {href: URL.createObjectURL(new Blob([csv], {type: "text/csv"})),
    download: "rds-log.csv"});
  a.click();
};

// Presets: four banks of ten, kept in this browser
let bank = localStorage.getItem("presetBank") || "A", sel = 0;
const loadBank = () => JSON.parse(localStorage.getItem("presets" + bank) || "[]");
const saveBank = p => localStorage.setItem("presets" + bank, JSON.stringify(p));
function drawPresets() {
  const p = loadBank(), box = $("presets");
  box.innerHTML = "";
  const menu = document.createElement("select");
  for (const b of "ABCD") menu.add(new Option("Bank " + b, b, false, b === bank));
  menu.onchange = () => { bank = menu.value; localStorage.setItem("presetBank", bank); drawPresets(); };
  box.appendChild(menu);
  for (let i = 0; i < 10; i++) {
    const b = document.createElement("button"), item = p[i];
    b.innerHTML = item ? esc(fmt(item.f, item.m)) + "<br>" + esc(item.ps || " ") : "—<br>&nbsp;";
    b.className = (item ? "" : "empty") + (i === sel ? " sel" : "");
    b.onclick = e => (e.ctrlKey ? store(i) : e.shiftKey ? clear(i) : recall(i));
    b.oncontextmenu = e => { e.preventDefault(); store(i); };
    b.onauxclick = e => { if (e.button === 1) clear(i); };
    box.appendChild(b);
  }
}
function recall(i) { sel = i; const it = loadBank()[i]; if (it) send("tune", {f: it.f}); drawPresets(); }
function store(i) { if (!state) return; const p = loadBank(); p[i] = {f: state.freq, m: state.mode, ps: (state.ps || state.name || "").trim()}; saveBank(p); sel = i; drawPresets(); }
function clear(i) { const p = loadBank(); p[i] = null; saveBank(p); drawPresets(); }
drawPresets();

async function poll() {
  if (demo) {
    if (!localStorage.getItem("presetsA")) {
      localStorage.setItem("presetsA", JSON.stringify([[89.9,"HIT FM"],[93.9,"LOS40"],[100.7,"MegaStar"],[99.5,"CAD-100"],
        [91.0,"EUROPAFM"],[92.4,"LOS40 DC"],[103.9,"LOS40 UR"],[102.7,"KISS FM"],[89.0,"LOS40 CL"],[105.4,"SER"]]
        .map(([f, ps]) => ({f: Math.round(f * 1e6), m: "FM", ps}))));
      drawPresets();
    }
    for (let i = 0; i < 240; i++) history.push(55.4 + Math.sin(i / 9) * .25 + (i > 160 && i < 170 ? .5 : 0));
    render(DEMO); $("conn").textContent = "Demo"; return;
  }
  try {
    const r = await fetch("/api/status", {cache: "no-store"});
    render(await r.json());
    $("conn").textContent = "";
  } catch (e) { $("conn").textContent = "Receiver not reachable"; }
  setTimeout(poll, 500);
}

async function send(c, args = {}) {
  if (demo) return;
  // A server open to the internet can ask for a control password
  const headers = {}, key = sessionStorage.getItem("controlKey");
  if (key) headers["X-Control-Key"] = key;
  const r = await fetch("/api/cmd?" + new URLSearchParams({c, ...args}), {method: "POST", headers}).catch(() => null);
  if (r && r.status === 401) {
    const typed = prompt("This receiver is in listen-only mode. Enter the control password to tune it:");
    if (typed) { sessionStorage.setItem("controlKey", typed); send(c, args); }
  }
}

document.querySelectorAll("[data-c]").forEach(b => b.onclick = () => send(b.dataset.c, {d: b.dataset.d || 0}));
document.querySelectorAll("#bands button").forEach(b => b.onclick = () => send("band", {name: b.dataset.b}));
$("scanBtn").onclick = () => send("seek", {d: 1});
$("helpBtn").onclick = () => $("help").showModal();
$("help").onclick = e => { if (e.target === $("help")) $("help").close(); };

function tuneDirect() {
  const v = parseFloat($("direct").value.replace(",", "."));
  if (v) send("tune", {f: Math.round(v < 300 ? v * 1e6 : v * 1e3)});
  $("direct").value = "";
}
$("direct").onkeydown = e => { if (e.key === "Enter") tuneDirect(); };
$("freq").onclick = () => $("direct").focus();
$("vol").oninput = e => { volTouched = Date.now(); send("vol", {v: e.target.value}); };

document.addEventListener("keydown", e => {
  if (e.target.tagName === "INPUT" || e.target.tagName === "SELECT") return;
  const k = {ArrowLeft: ["step", -1], ArrowRight: ["step", 1], PageDown: ["seek", -1], PageUp: ["seek", 1]}[e.key];
  if (k) { e.preventDefault(); send(k[0], {d: k[1]}); }
  else if (e.key === "[" ) { sel = (sel + 9) % 10; drawPresets(); }
  else if (e.key === "]") { sel = (sel + 1) % 10; drawPresets(); }
  else if (e.key === "Enter") recall(sel);
  else if (e.shiftKey && e.key === "S") store(sel);
  else if (e.shiftKey && e.key === "R") clear(sel);
  else if (/^[0-9.,]$/.test(e.key)) $("direct").focus();
});

// Audio, when the page comes from the PC companion
let audio = null;
$("play").onclick = () => audio ? stopAudio() : startAudio();
$("recBtn").onclick = () => send("record");
function setPlay(playing) {
  $("playIcon").innerHTML = playing ? '<path d="M6 4h4v16H6zm8 0h4v16h-4z"/>' : '<path d="M7 4l13 8-13 8z"/>';
  $("audioState").textContent = playing ? "Playing" : "Standby";
  $("audioSub").textContent = playing ? "Live from the line input" : "No active audio stream";
  $("gain").hidden = !playing;
}
function stopAudio() { audio.ws.close(); audio.ctx.close(); audio = null; setPlay(false); }
function startAudio() {
  const ctx = new AudioContext(), gain = ctx.createGain();
  gain.gain.value = $("gain").value / 100;
  gain.connect(ctx.destination);
  const ws = new WebSocket((location.protocol === "https:" ? "wss://" : "ws://") + location.host + "/audio");
  ws.binaryType = "arraybuffer";
  let rate = 24000, channels = 1, playAt = 0;
  ws.onmessage = ev => {
    if (typeof ev.data === "string") { const h = JSON.parse(ev.data); rate = h.rate; channels = h.channels; return; }
    const pcm = new Int16Array(ev.data), frames = pcm.length / channels;
    const buf = ctx.createBuffer(channels, frames, rate);
    for (let c = 0; c < channels; c++) {
      const out = buf.getChannelData(c);
      for (let i = 0; i < frames; i++) out[i] = pcm[i * channels + c] / 32768;
    }
    const src = ctx.createBufferSource();
    src.buffer = buf; src.connect(gain);
    // Keep a small cushion against network jitter
    const t = ctx.currentTime;
    if (playAt < t + 0.05 || playAt > t + 1.0) playAt = t + 0.3;
    src.start(playAt); playAt += buf.duration;
  };
  ws.onclose = () => { if (audio) stopAudio(); };
  $("gain").oninput = e => gain.gain.value = e.target.value / 100;
  audio = {ctx, ws};
  setPlay(true);
}

poll();
</script>
</body>
</html>
)HTML";

#endif // PAGE_REMOTE_H
