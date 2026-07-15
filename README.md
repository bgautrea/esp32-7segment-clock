# 7-Segment WS2812 Clock (ESP32)

24-hour HH:MM clock driving a 3D-printed, hand-wired 7-segment display of
WS2812 (5V) LEDs, synced to a local NTP server.

## Hardware model

- 4 digits (HH:MM), **10 LEDs per segment** → 70 LEDs/digit.
- One continuous data chain of **298 LEDs**, GPIO18 data:

  | Section | Physical indices | Count |
  |---|---|---|
  | Digit 1 (H) | 0–69 | 70 |
  | Connector H↔H | 70–71 | 2 |
  | Digit 2 (H) | 72–141 | 70 |
  | Center cross / colon | 142–155 | 14 |
  | Digit 3 (M) | 156–225 | 70 |
  | Connector M↔M | 226–227 | 2 |
  | Digit 4 (M) | 228–297 | 70 |

- Colon = top 3 + bottom 3 of the cross's vertical bar (`COLON_LEDS` in `include/config.h`).
- ⚡ 298 px at full white ≈ 18 A @ 5 V. Runs dim by default (`DEFAULT_BRIGHTNESS`).
  Confirm PSU rating and inject 5V at multiple points so far digits don't sag red.

## Toolchain

PlatformIO Core is installed in a project-local venv (`.venv/`). All commands
use it directly:

```sh
# Build
.venv/Scripts/python.exe -m platformio run -e esp32dev

# Flash over USB (COM7)
.venv/Scripts/python.exe -m platformio run -e esp32dev -t upload

# Serial monitor @ 115200
.venv/Scripts/python.exe -m platformio device monitor
```

### Flash over WiFi (OTA, no USB)

Firmware updates are HTTP-push (outbound to the device, so no PC-firewall
issues). Two ways:

- **Browser:** the "Firmware update" box in the web UI — choose the `.bin`,
  Upload, enter the OTA password (user `admin`, password = `OTA_PASSWORD` in
  `include/secrets.h`).
- **CLI:**
  ```sh
  .venv/Scripts/python.exe -m platformio run -e esp32dev            # build
  curl.exe -u admin:segclock -F "firmware=@.pio/build/esp32dev/firmware.bin" \
           http://192.168.0.51/update
  ```

A failed OTA never bricks the board — USB re-flashing always recovers it. A
DHCP reservation (or `clock.local`) keeps the address stable.

## First-time setup

1. Copy `include/secrets.h.example` to `include/secrets.h` and fill in your
   WiFi SSID and password (this file is gitignored, so it stays local).
2. Build + upload (commands above).
3. Open the serial monitor. On boot it flashes **88:88** (self-test), joins WiFi,
   and queries NTP at `192.168.0.156`. Once synced it shows the time.

## Serial console

| Key | Action |
|---|---|
| `c` | clock mode |
| `k` | calibrate mode |
| `t` | self-test (88:88) |
| `+` / `-` | brightness |
| `h` | help |
| **calibrate:** `n`/`p` | next / prev digit (0–3) |
| **calibrate:** `]`/`[` | next / prev run (0–6) |
| **calibrate:** `o` | colon test |

## Calibration (mapping runs → segments)

The one thing we can't know from a photo is which physical 10-LED run is which
logical segment. In calibrate mode (`k`), one run lights **green** at a time.
For each digit, step through runs 0–6 with `]`, note which segment (a–g) each
run actually is, and record it. Then fix `SEG_RUN[digit][segment]` in
`include/config.h` and reflash. Segment order:

```
     a
   f   b
     g
   e   c
     d
```

`SEG_RUN[d][s]` = the run number (0–6) that lights logical segment `s`
(0=a,1=b,2=c,3=d,4=e,5=f,6=g) on digit `d`.

## Roadmap

- [x] NTP clock, 24h Central
- [x] Calibrated segment map + digit order (right-to-left chain)
- [x] Web UI (brightness, colors, effects, 12/24h) at http://clock.local — settings persist to NVS
- [x] OTA firmware updates (HTTP push, browser or curl)
- [ ] Stopwatch + timer modes
