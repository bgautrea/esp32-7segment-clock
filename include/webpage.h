#pragma once
#include <Arduino.h>

// Single self-contained config page. Served from flash. Talks to:
//   GET /state           current settings + active-mode display
//   GET /set?...         change persisted settings (and mode)
//   GET /action?do=...   transient stopwatch/timer start|pause|reset
//   POST /update         firmware upload
const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>7-Seg Clock</title>
<style>
  :root { --bg:#0e0f13; --card:#191b22; --line:#2a2d38; --fg:#e8eaf0; --mut:#8a90a2; --accent:#ff7800; }
  * { box-sizing:border-box; -webkit-tap-highlight-color:transparent; }
  body { margin:0; min-height:100vh; background:var(--bg); color:var(--fg);
         font:16px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
         display:flex; justify-content:center; padding:24px 16px; }
  .wrap { width:100%; max-width:420px; }
  h1 { font-size:15px; font-weight:600; letter-spacing:.14em; text-transform:uppercase;
       color:var(--mut); margin:0 0 20px; text-align:center; }
  .clock { font-variant-numeric:tabular-nums; font-weight:700; text-align:center;
           font-size:64px; letter-spacing:.04em; margin:4px 0 24px; }
  .clock small { font-size:20px; color:var(--mut); font-weight:600; margin-left:6px; text-transform:uppercase; }
  .card { background:var(--card); border:1px solid var(--line); border-radius:16px;
          padding:20px; margin-bottom:16px; }
  .row { display:flex; align-items:center; justify-content:space-between; gap:16px; }
  .row + .row { margin-top:20px; }
  label { color:var(--mut); font-size:14px; }
  .val { font-variant-numeric:tabular-nums; color:var(--fg); font-size:14px; min-width:34px; text-align:right; }
  input[type=range] { -webkit-appearance:none; appearance:none; width:100%; height:6px;
    background:var(--line); border-radius:999px; outline:none; }
  input[type=range]::-webkit-slider-thumb { -webkit-appearance:none; width:22px; height:22px;
    border-radius:50%; background:var(--accent); cursor:pointer; border:3px solid var(--card); }
  input[type=range]::-moz-range-thumb { width:22px; height:22px; border-radius:50%;
    background:var(--accent); cursor:pointer; border:3px solid var(--card); }
  .brow { display:block; }
  .brow .top { display:flex; justify-content:space-between; margin-bottom:12px; }
  input[type=color] { -webkit-appearance:none; appearance:none; border:none; width:52px; height:34px;
    background:none; cursor:pointer; padding:0; }
  input[type=color]::-webkit-color-swatch-wrapper { padding:0; }
  input[type=color]::-webkit-color-swatch { border:1px solid var(--line); border-radius:8px; }
  input[type=time] { background:var(--bg); border:1px solid var(--line); color:var(--fg);
    font:inherit; font-size:15px; border-radius:8px; padding:6px 8px; }
  .seg { display:flex; background:var(--bg); border:1px solid var(--line); border-radius:10px; padding:3px; }
  .seg button { flex:1; border:none; background:none; color:var(--mut); font:inherit; font-size:14px;
    padding:7px 12px; border-radius:8px; cursor:pointer; }
  .seg button.on { background:var(--accent); color:#111; font-weight:600; }
  .swatches { display:flex; flex-wrap:wrap; gap:12px; margin-top:18px; }
  .swatches button { width:36px; height:36px; border-radius:50%; border:2px solid var(--line);
    cursor:pointer; padding:0; transition:transform .08s; }
  .swatches button:active { transform:scale(.9); }
  .swatches button.on { border-color:var(--fg); box-shadow:0 0 0 2px var(--card),0 0 0 4px var(--fg); }
  .ctl { margin-top:18px; display:flex; gap:10px; align-items:center; flex-wrap:wrap; }
  .act { flex:1; border:none; background:var(--accent); color:#111; font:inherit; font-weight:600;
    padding:13px; border-radius:11px; cursor:pointer; }
  .act.sec { background:var(--line); color:var(--fg); }
  .trow { display:flex; align-items:center; justify-content:center; gap:8px; width:100%;
    font-size:26px; font-weight:700; margin-bottom:14px; }
  .trow input { width:76px; background:var(--bg); border:1px solid var(--line); color:var(--fg);
    font:inherit; font-size:26px; font-weight:700; text-align:center; border-radius:9px; padding:6px; }
  .foot { text-align:center; color:var(--mut); font-size:12px; margin-top:8px; }
  .foot code { color:var(--fg); }
  .upd { display:flex; gap:10px; align-items:center; margin-top:14px; flex-wrap:wrap; }
  .upd input[type=file] { flex:1; min-width:0; color:var(--mut); font-size:13px; }
  .upd button { border:none; background:var(--accent); color:#111; font:inherit; font-weight:600;
    padding:9px 16px; border-radius:9px; cursor:pointer; }
  .upd button:disabled { opacity:.5; cursor:default; }
  .ustat { color:var(--mut); font-size:13px; margin-top:8px; min-height:18px; }
  .hide { display:none !important; }
</style>
</head>
<body>
  <div class="wrap">
    <h1>7-Segment Clock</h1>
    <div class="clock"><span id="disp">--:--</span><small id="sub"></small></div>

    <div class="card">
      <div class="row">
        <label>Mode</label>
        <div class="seg" id="mode">
          <button data-m="clock">Clock</button>
          <button data-m="stopwatch">Stopwatch</button>
          <button data-m="timer">Timer</button>
        </div>
      </div>
      <div class="ctl hide" id="swctl">
        <button class="act" id="sw_go">Start</button>
        <button class="act sec" id="sw_reset">Reset</button>
      </div>
      <div id="tmctl" class="hide">
        <div class="ctl" style="display:block">
          <div class="trow">
            <input type="number" id="tmin" min="0" max="99" inputmode="numeric"> <span>:</span>
            <input type="number" id="tsec" min="0" max="59" inputmode="numeric">
          </div>
          <div style="display:flex; gap:10px">
            <button class="act" id="tm_go">Start</button>
            <button class="act sec" id="tm_reset">Reset</button>
          </div>
        </div>
      </div>
    </div>

    <div class="card">
      <div class="brow">
        <div class="top"><label>Brightness</label><span class="val" id="bval">--</span></div>
        <input type="range" id="bright" min="3" max="255" step="1">
      </div>
    </div>

    <div class="card">
      <div class="row">
        <label>Digit color</label>
        <input type="color" id="color" value="#ff7800">
      </div>
      <div class="swatches" id="swatches"></div>
      <div class="row">
        <label>Colon color</label>
        <input type="color" id="colon" value="#ff7800">
      </div>
    </div>

    <div class="card">
      <div class="row">
        <label>Effect</label>
        <div class="seg" id="effect">
          <button data-e="0">Solid</button>
          <button data-e="1">Rainbow</button>
          <button data-e="2">Cycle</button>
        </div>
      </div>
      <div class="brow" style="margin-top:20px">
        <div class="top"><label>Effect speed</label><span class="val" id="sval">--</span></div>
        <input type="range" id="speed" min="1" max="255" step="1">
      </div>
    </div>

    <div class="card">
      <div class="row">
        <label>Night dimming<span id="nact"></span></label>
        <div class="seg" id="ndim">
          <button data-n="0">Off</button>
          <button data-n="1">On</button>
        </div>
      </div>
      <div class="row"><label>From</label><input type="time" id="nstart"></div>
      <div class="row"><label>To</label><input type="time" id="nend"></div>
      <div class="brow" style="margin-top:20px">
        <div class="top"><label>Night brightness</label><span class="val" id="nbval">--</span></div>
        <input type="range" id="nbright" min="1" max="255" step="1">
      </div>
    </div>

    <div class="card">
      <div class="row">
        <label>Time format</label>
        <div class="seg" id="fmt">
          <button data-f="24">24 h</button>
          <button data-f="12">12 h</button>
        </div>
      </div>
    </div>

    <div class="card">
      <label>Firmware update</label>
      <div class="upd">
        <input type="file" id="fw" accept=".bin">
        <button id="upbtn" type="button">Upload</button>
      </div>
      <div class="ustat" id="ustat"></div>
    </div>

    <div class="foot">Reachable at <code id="host"></code></div>
  </div>

<script>
const $ = s => document.querySelector(s);
let t, t2, t3;                            // debounce timers
let st = {};                              // last known state
const focused = el => document.activeElement === el;
const min2hhmm = m => String(Math.floor(m/60)).padStart(2,'0')+':'+String(m%60).padStart(2,'0');
const hhmm2min = v => { const [h,m] = v.split(':').map(Number); return h*60+m; };

// Preset quick-pick colors.
const PRESETS = ['ff7800','ff2d2d','ffd166','24d160','00d0ff','2d6cff','9b5cff','ff3ea5','ffffff'];
const sw = $('#swatches');
PRESETS.forEach(hex => {
  const b = document.createElement('button');
  b.style.background = '#' + hex;
  b.dataset.hex = hex;
  b.addEventListener('click', () => send({color: hex}));
  sw.appendChild(b);
});

function paint(s) {
  st = s;
  $('#disp').textContent = s.disp;
  $('#sub').textContent = s.mode === 'clock' ? s.fmt + 'h' : s.mode;

  // mode selector + panels
  document.querySelectorAll('#mode button').forEach(b => b.classList.toggle('on', b.dataset.m === s.mode));
  $('#swctl').classList.toggle('hide', s.mode !== 'stopwatch');
  $('#tmctl').classList.toggle('hide', s.mode !== 'timer');
  $('#sw_go').textContent = s.running ? 'Pause' : 'Start';
  $('#tm_go').textContent = s.running ? 'Pause' : 'Start';
  if (!focused($('#tmin'))) $('#tmin').value = Math.floor(s.tdur / 60);
  if (!focused($('#tsec'))) $('#tsec').value = s.tdur % 60;

  // brightness / colors / effect
  $('#bval').textContent = s.brightness; if (!focused($('#bright'))) $('#bright').value = s.brightness;
  $('#color').value = '#' + s.color;
  $('#colon').value = '#' + s.colon;
  $('#sval').textContent = s.speed; if (!focused($('#speed'))) $('#speed').value = s.speed;
  const cur = s.color.toLowerCase();
  document.querySelectorAll('#effect button').forEach(b => b.classList.toggle('on', +b.dataset.e === s.effect));
  document.querySelectorAll('#fmt button').forEach(b => b.classList.toggle('on', +b.dataset.f === s.fmt));
  document.querySelectorAll('#swatches button').forEach(b => b.classList.toggle('on', b.dataset.hex.toLowerCase() === cur));

  // night dimming
  document.querySelectorAll('#ndim button').forEach(b => b.classList.toggle('on', +b.dataset.n === (s.ndim ? 1 : 0)));
  $('#nact').textContent = s.nactive ? '  🌙 active' : '';
  if (!focused($('#nstart'))) $('#nstart').value = min2hhmm(s.nstart);
  if (!focused($('#nend')))   $('#nend').value   = min2hhmm(s.nend);
  $('#nbval').textContent = s.nbright; if (!focused($('#nbright'))) $('#nbright').value = s.nbright;
}

async function send(params)  { paint(await (await fetch('/set?' + new URLSearchParams(params))).json()); }
async function act(doWhat)   { paint(await (await fetch('/action?do=' + doWhat)).json()); }
async function refresh()     { try { paint(await (await fetch('/state')).json()); } catch(e) {} }

// mode + transient controls
document.querySelectorAll('#mode button').forEach(b => b.addEventListener('click', () => send({mode: b.dataset.m})));
$('#sw_go').addEventListener('click', () => act(st.running ? 'pause' : 'start'));
$('#sw_reset').addEventListener('click', () => act('reset'));
$('#tm_go').addEventListener('click', () => act(st.running ? 'pause' : 'start'));
$('#tm_reset').addEventListener('click', () => act('reset'));
function sendTdur() {
  const m = Math.min(99, Math.max(0, +$('#tmin').value || 0));
  const s = Math.min(59, Math.max(0, +$('#tsec').value || 0));
  send({tdur: Math.max(1, m * 60 + s)});
}
$('#tmin').addEventListener('change', sendTdur);
$('#tsec').addEventListener('change', sendTdur);

// brightness / colors / effect / format
$('#bright').addEventListener('input', e => {
  $('#bval').textContent = e.target.value;
  clearTimeout(t); t = setTimeout(() => send({brightness: e.target.value}), 120);
});
['input','change'].forEach(ev => $('#color').addEventListener(ev, e => send({color: e.target.value.replace('#','')})));
['input','change'].forEach(ev => $('#colon').addEventListener(ev, e => send({colon: e.target.value.replace('#','')})));
document.querySelectorAll('#effect button').forEach(b => b.addEventListener('click', () => send({effect: b.dataset.e})));
document.querySelectorAll('#fmt button').forEach(b => b.addEventListener('click', () => send({fmt: b.dataset.f})));
$('#speed').addEventListener('input', e => {
  $('#sval').textContent = e.target.value;
  clearTimeout(t2); t2 = setTimeout(() => send({speed: e.target.value}), 120);
});

// night dimming
document.querySelectorAll('#ndim button').forEach(b => b.addEventListener('click', () => send({ndim: b.dataset.n})));
$('#nstart').addEventListener('change', e => send({nstart: hhmm2min(e.target.value)}));
$('#nend').addEventListener('change', e => send({nend: hhmm2min(e.target.value)}));
$('#nbright').addEventListener('input', e => {
  $('#nbval').textContent = e.target.value;
  clearTimeout(t3); t3 = setTimeout(() => send({nbright: e.target.value}), 120);
});

// firmware upload (multipart POST to /update with HTTP basic auth)
$('#upbtn').addEventListener('click', () => {
  const f = $('#fw').files[0];
  if (!f) { $('#ustat').textContent = 'Choose a .bin file first.'; return; }
  const pw = prompt('OTA password:');
  if (pw === null) return;
  const fd = new FormData();
  fd.append('firmware', f, f.name);
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/update');
  xhr.setRequestHeader('Authorization', 'Basic ' + btoa('admin:' + pw));
  $('#upbtn').disabled = true;
  xhr.upload.onprogress = e => {
    if (e.lengthComputable) $('#ustat').textContent = 'Uploading ' + Math.round(e.loaded / e.total * 100) + '%';
  };
  xhr.onload = () => {
    if (xhr.status === 200) { $('#ustat').textContent = 'Success — device rebooting…'; setTimeout(() => location.reload(), 7000); }
    else { $('#ustat').textContent = xhr.status === 401 ? 'Wrong password.' : 'Failed (' + xhr.status + ').'; $('#upbtn').disabled = false; }
  };
  xhr.onerror = () => { $('#ustat').textContent = 'Upload error.'; $('#upbtn').disabled = false; };
  xhr.send(fd);
});

$('#host').textContent = location.host;
refresh();
setInterval(refresh, 1000);               // keep counters/time live
</script>
</body>
</html>)HTML";
