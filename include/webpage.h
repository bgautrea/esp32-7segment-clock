#pragma once
#include <Arduino.h>

// Single self-contained config page. Served from flash. Talks to /state
// (GET current settings) and /set (GET with query args to change them).
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
           font-size:64px; letter-spacing:.04em; margin:4px 0 28px; }
  .clock small { font-size:22px; color:var(--mut); font-weight:600; margin-left:6px; }
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
  .seg { display:flex; background:var(--bg); border:1px solid var(--line); border-radius:10px; padding:3px; }
  .seg button { flex:1; border:none; background:none; color:var(--mut); font:inherit; font-size:14px;
    padding:7px 16px; border-radius:8px; cursor:pointer; }
  .seg button.on { background:var(--accent); color:#111; font-weight:600; }
  .swatches { display:flex; flex-wrap:wrap; gap:12px; margin-top:18px; }
  .swatches button { width:36px; height:36px; border-radius:50%; border:2px solid var(--line);
    cursor:pointer; padding:0; transition:transform .08s; }
  .swatches button:active { transform:scale(.9); }
  .swatches button.on { border-color:var(--fg); box-shadow:0 0 0 2px var(--card),0 0 0 4px var(--fg); }
  .foot { text-align:center; color:var(--mut); font-size:12px; margin-top:8px; }
  .foot code { color:var(--fg); }
  .upd { display:flex; gap:10px; align-items:center; margin-top:14px; flex-wrap:wrap; }
  .upd input[type=file] { flex:1; min-width:0; color:var(--mut); font-size:13px; }
  .upd button { border:none; background:var(--accent); color:#111; font:inherit; font-weight:600;
    padding:9px 16px; border-radius:9px; cursor:pointer; }
  .upd button:disabled { opacity:.5; cursor:default; }
  .ustat { color:var(--mut); font-size:13px; margin-top:8px; min-height:18px; }
</style>
</head>
<body>
  <div class="wrap">
    <h1>7-Segment Clock</h1>
    <div class="clock"><span id="time">--:--</span><small id="fmtlbl">24h</small></div>

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
let t, t2;                               // debounce timers (brightness, speed)

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
  $('#time').textContent = s.time;
  $('#fmtlbl').textContent = s.fmt + 'h';
  $('#bval').textContent = s.brightness;
  $('#bright').value = s.brightness;
  $('#color').value = '#' + s.color;
  $('#colon').value = '#' + s.colon;
  $('#sval').textContent = s.speed;
  $('#speed').value = s.speed;
  const cur = s.color.toLowerCase();
  document.querySelectorAll('#fmt button').forEach(b =>
    b.classList.toggle('on', +b.dataset.f === s.fmt));
  document.querySelectorAll('#effect button').forEach(b =>
    b.classList.toggle('on', +b.dataset.e === s.effect));
  document.querySelectorAll('#swatches button').forEach(b =>
    b.classList.toggle('on', b.dataset.hex.toLowerCase() === cur));
}

async function send(params) {
  const s = await (await fetch('/set?' + new URLSearchParams(params))).json();
  paint(s);
}
async function refresh() {
  try { paint(await (await fetch('/state')).json()); } catch(e) {}
}

$('#bright').addEventListener('input', e => {
  $('#bval').textContent = e.target.value;
  clearTimeout(t); t = setTimeout(() => send({brightness: e.target.value}), 120);
});
['input','change'].forEach(ev =>          // 'change' covers mobile pickers
  $('#color').addEventListener(ev, e => send({color: e.target.value.replace('#','')})));
['input','change'].forEach(ev =>
  $('#colon').addEventListener(ev, e => send({colon: e.target.value.replace('#','')})));
document.querySelectorAll('#fmt button').forEach(b =>
  b.addEventListener('click', () => send({fmt: b.dataset.f})));
document.querySelectorAll('#effect button').forEach(b =>
  b.addEventListener('click', () => send({effect: b.dataset.e})));
$('#speed').addEventListener('input', e => {
  $('#sval').textContent = e.target.value;
  clearTimeout(t2); t2 = setTimeout(() => send({speed: e.target.value}), 120);
});

// Firmware upload (multipart POST to /update with HTTP basic auth).
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
    if (e.lengthComputable)
      $('#ustat').textContent = 'Uploading ' + Math.round(e.loaded / e.total * 100) + '%';
  };
  xhr.onload = () => {
    if (xhr.status === 200) {
      $('#ustat').textContent = 'Success — device rebooting…';
      setTimeout(() => location.reload(), 7000);
    } else {
      $('#ustat').textContent = xhr.status === 401 ? 'Wrong password.' : 'Failed (' + xhr.status + ').';
      $('#upbtn').disabled = false;
    }
  };
  xhr.onerror = () => { $('#ustat').textContent = 'Upload error.'; $('#upbtn').disabled = false; };
  xhr.send(fd);
});

$('#host').textContent = location.host;
refresh();
setInterval(refresh, 2000);            // keep the time readout live
</script>
</body>
</html>)HTML";
