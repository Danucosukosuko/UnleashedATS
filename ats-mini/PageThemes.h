#ifndef PAGE_THEMES_H
#define PAGE_THEMES_H

#include <Arduino.h>

// Theme editor. The page reads /api/themes and posts its changes there;
// the receiver carries them out on its main loop.

static const char pageThemesTitle[] PROGMEM = "ATS-Mini Themes";

static const char pageThemes[] PROGMEM = R"HTML(<H1>{{title}}</H1>
{{{navigation}}}
<STYLE>
  #msg { min-height: 1.4em; }
  #list TD, #list TH { padding: 0.3em 0.6em; }
  #list BUTTON { margin: 0.1em; }
  .swatch { display: inline-block; width: 1.1em; height: 1.1em; border: 1px solid #888; vertical-align: middle; margin-right: 1px; }
  .groups { display: flex; flex-wrap: wrap; gap: 1em; justify-content: center; }
  FIELDSET { min-width: 15em; }
  FIELDSET LABEL { display: flex; justify-content: space-between; align-items: center; gap: 0.5em; margin: 0.15em 0; }
  FIELDSET INPUT[type=color] { width: 3em; height: 1.6em; padding: 0; border: none; }
  .muted { opacity: 0.6; }
</STYLE>
<TABLE COLUMNS="1">
  <TR><TD CLASS="CENTER" ID="msg"></TD></TR>
  <TR><TD CLASS="CENTER"><TABLE ID="list"></TABLE></TD></TR>
  <TR><TH CLASS="HEADING">
    <BUTTON TYPE="BUTTON" ID="new" STYLE="padding: 0.5em 2em;">New theme</BUTTON>
  </TH></TR>
</TABLE>

<DIV ID="editor" HIDDEN>
  <H2 ID="edtitle" ALIGN="CENTER"></H2>
  <P ALIGN="CENTER">
    <LABEL>Name <INPUT ID="name" MAXLENGTH="15" SIZE="16"></LABEL>
    &nbsp;<LABEL><INPUT TYPE="CHECKBOX" ID="live" CHECKED> Live preview on the receiver</LABEL>
  </P>
  <DIV CLASS="groups" ID="groups"></DIV>
  <P ALIGN="CENTER">
    <BUTTON TYPE="BUTTON" ID="save" STYLE="padding: 0.5em 2em;">Save</BUTTON>
    <BUTTON TYPE="BUTTON" ID="saveas" STYLE="padding: 0.5em 2em;">Save as new</BUTTON>
    <BUTTON TYPE="BUTTON" ID="close" STYLE="padding: 0.5em 2em;">Close</BUTTON>
  </P>
</DIV>

<SCRIPT>
(function () {
  const $ = (id) => document.getElementById(id);
  const words = { bg: 'background', smeter: 'S-meter', rf: 'RF', batt: 'battery', hl: 'highlight',
                  hdr: 'header', funit: 'frequency unit', freq: 'frequency', snr: 'SNR', rssi: 'RSSI',
                  sel: 'selected', conn: 'connected', rds: 'RDS', off: 'off', plus: 'plus' };
  const groups = [
    ['Screen', ['bg', 'text']], ['S-meter', ['smeter']], ['Icons', ['save', 'stereo', 'rf']],
    ['Battery', ['batt']], ['Band and mode', ['band', 'mode']], ['Boxes', ['box']], ['Menus', ['menu']],
    ['Frequency', ['freq', 'funit']], ['RDS and scale', ['rds', 'scale']], ['Scan', ['scan']]
  ];
  let state = null, editing = null, timer = null;

  function label(field) {
    const text = field.split('_').map((w) => words[w] || w).join(' ');
    return text.charAt(0).toUpperCase() + text.slice(1);
  }
  function to565(hex) {
    const v = parseInt(hex.slice(1), 16);
    const r = (v >> 16) & 255, g = (v >> 8) & 255, b = v & 255;
    return (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)).toString(16).padStart(4, '0');
  }
  function message(text) { $('msg').textContent = text || ''; }

  async function post(fields) {
    const body = new URLSearchParams(fields);
    for (let i = 0; i < 20; i++) {
      const r = await fetch('/api/themes', { method: 'POST', body });
      if (r.status !== 409) return r.ok;
      await new Promise((res) => setTimeout(res, 150));
    }
    return false;
  }
  async function load(note) {
    const r = await fetch('/api/themes', { cache: 'no-store' });
    state = await r.json();
    render();
    message(note !== undefined ? note : state.message);
  }
  async function act(fields) {
    if (!(await post(fields))) { message('The receiver did not take the change.'); return; }
    // Changes are carried out on the receiver's main loop
    setTimeout(() => load(), 350);
  }

  function swatches(colors) {
    // A few colors that tell themes apart: background, text, menu, frequency, S-meter
    return [0, 1, 26, 30, 33, 5].map((i) => '<SPAN CLASS="swatch" STYLE="background:' + colors[i] + '"></SPAN>').join('');
  }
  function button(text, handler) {
    const b = document.createElement('BUTTON');
    b.type = 'button';
    b.textContent = text;
    b.onclick = handler;
    return b;
  }
  function render() {
    const list = $('list');
    list.textContent = '';
    const head = list.insertRow();
    ['', 'Theme', 'Kind', ''].forEach((t) => { const th = document.createElement('TH'); th.textContent = t; head.appendChild(th); });
    state.themes.forEach((t) => {
      const row = list.insertRow();
      if (t.hidden) row.className = 'muted';
      row.insertCell().innerHTML = swatches(t.colors);
      const name = row.insertCell();
      name.textContent = t.name + (t.id === state.current && !state.preview ? '  (in use)' : '');
      row.insertCell().textContent = t.builtin ? (t.hidden ? 'Built-in, hidden' : 'Built-in') : 'Custom';
      const cell = row.insertCell();
      if (!t.hidden) cell.appendChild(button('Use', () => act({ op: 'select', id: t.id })));
      if (t.builtin) {
        cell.appendChild(button('Copy', () => edit(t, true)));
        cell.appendChild(button(t.hidden ? 'Show' : 'Hide', () => act({ op: t.hidden ? 'show' : 'hide', id: t.id })));
      } else {
        cell.appendChild(button('Edit', () => edit(t, false)));
        cell.appendChild(button('Copy', () => edit(t, true)));
        cell.appendChild(button('Delete', () => {
          if (confirm('Delete the theme "' + t.name + '"?')) act({ op: 'delete', id: t.id });
        }));
      }
    });
  }

  function colors() {
    return state.fields.map((f) => to565($('c_' + f).value)).join(',');
  }
  function schedulePreview() {
    if (!$('live').checked) return;
    clearTimeout(timer);
    timer = setTimeout(() => post({ op: 'preview', c: colors() }), 250);
  }
  function edit(theme, copy) {
    editing = copy ? null : theme.id;
    $('edtitle').textContent = copy ? 'New theme, from ' + theme.name : 'Editing ' + theme.name;
    $('name').value = copy ? (theme.name + ' copy').slice(0, 15) : theme.name;
    $('save').hidden = copy;
    const box = $('groups');
    box.textContent = '';
    groups.forEach(([title, prefixes]) => {
      const set = document.createElement('FIELDSET');
      const legend = document.createElement('LEGEND');
      legend.textContent = title;
      set.appendChild(legend);
      state.fields.forEach((f, i) => {
        if (!prefixes.includes(f.split('_')[0])) return;
        const row = document.createElement('LABEL');
        row.textContent = label(f);
        const input = document.createElement('INPUT');
        input.type = 'color';
        input.id = 'c_' + f;
        input.value = theme.colors[i];
        input.oninput = schedulePreview;
        row.appendChild(input);
        set.appendChild(row);
      });
      box.appendChild(set);
    });
    $('editor').hidden = false;
    $('editor').scrollIntoView({ behavior: 'smooth' });
    schedulePreview();
  }
  async function close(note) {
    clearTimeout(timer);
    $('editor').hidden = true;
    editing = null;
    if (state && state.preview) await post({ op: 'cancel' });
    setTimeout(() => load(note), 350);
  }

  $('live').onchange = () => { if ($('live').checked) schedulePreview(); else post({ op: 'cancel' }); };
  $('new').onclick = () => {
    const current = state.themes.find((t) => t.id === state.current) || state.themes[0];
    edit(current, true);
  };
  $('save').onclick = () => { clearTimeout(timer); act({ op: 'save', id: editing, name: $('name').value, c: colors() }); $('editor').hidden = true; };
  $('saveas').onclick = () => { clearTimeout(timer); act({ op: 'save', id: -1, name: $('name').value, c: colors() }); $('editor').hidden = true; };
  $('close').onclick = () => close('');
  load();
})();
</SCRIPT>
)HTML";

#endif // PAGE_THEMES_H
