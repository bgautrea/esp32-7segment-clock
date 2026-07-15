// =====================================================================
//  7-Segment WS2812 Clock  —  firmware v1
//  Goal: come up, join WiFi, sync to the local NTP server, show 24h
//  HH:MM in US Central time. Includes a serial calibration console so we
//  can map physical LED runs to logical segments on the bench.
// =====================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <Preferences.h>
#include <time.h>
#include <FastLED.h>

#include "config.h"
#include "secrets.h"
#include "webpage.h"

CRGB leds[LED_COUNT];

enum Mode { MODE_CLOCK, MODE_CALIBRATE, MODE_MAP };
Mode    mode        = MODE_CLOCK;
bool    timeSynced  = false;

// Digit color effects.
enum Effect { FX_SOLID = 0, FX_RAINBOW = 1, FX_CYCLE = 2 };

// How far the rainbow hue advances per LED (position spread). ~2 gives a bit
// over two rainbows across the 298-LED chain.
#define RAINBOW_SPREAD 2

// User settings, persisted to NVS flash.
struct Settings {
  uint8_t  brightness = DEFAULT_BRIGHTNESS;
  uint32_t color      = 0xFF7800;   // 0xRRGGBB digit color, warm amber
  uint32_t colonColor = 0xFF7800;   // 0xRRGGBB colon color
  uint8_t  effect     = FX_SOLID;
  uint8_t  speed      = 128;         // effect speed, 1..255
  bool     use12h     = false;       // false = 24h
};
Settings settings;

uint8_t gHue = 0;                     // animated hue base for effects

// Derived display colors, refreshed by applySettings().
CRGB colorOn    = CRGB(255, 120, 0);
CRGB colorColon = CRGB(255, 120, 0);

Preferences prefs;
WebServer   server(80);

// Calibration cursor.
int  calDigit = 0;
int  calRun   = 0;

// Distinct color per run index, for the color-map calibration photo.
//   run0=red run1=green run2=blue run3=yellow run4=cyan run5=magenta run6=white
const CRGB RUN_COLORS[7] = {
  CRGB(255,0,0), CRGB(0,255,0), CRGB(0,0,255), CRGB(255,255,0),
  CRGB(0,255,255), CRGB(255,0,255), CRGB(255,255,255)
};

// ---------------------------------------------------------------------
//  Settings (persisted to NVS)
// ---------------------------------------------------------------------
void applySettings() {
  FastLED.setBrightness(settings.brightness);
  colorOn    = CRGB((settings.color >> 16) & 0xFF,
                    (settings.color >> 8)  & 0xFF,
                    (settings.color)       & 0xFF);
  colorColon = CRGB((settings.colonColor >> 16) & 0xFF,
                    (settings.colonColor >> 8)  & 0xFF,
                    (settings.colonColor)       & 0xFF);
}

void loadSettings() {
  prefs.begin("clock", true);
  settings.brightness = prefs.getUChar("bright", settings.brightness);
  settings.color      = prefs.getULong("color",  settings.color);
  settings.colonColor = prefs.getULong("colon",  settings.colonColor);
  settings.effect     = prefs.getUChar("effect", settings.effect);
  settings.speed      = prefs.getUChar("speed",  settings.speed);
  settings.use12h     = prefs.getBool ("use12h", settings.use12h);
  prefs.end();
}

void saveSettings() {
  prefs.begin("clock", false);
  prefs.putUChar("bright", settings.brightness);
  prefs.putULong("color",  settings.color);
  prefs.putULong("colon",  settings.colonColor);
  prefs.putUChar("effect", settings.effect);
  prefs.putUChar("speed",  settings.speed);
  prefs.putBool ("use12h", settings.use12h);
  prefs.end();
}

// ---------------------------------------------------------------------
//  Low-level drawing
// ---------------------------------------------------------------------
void clearAll() { fill_solid(leds, LED_COUNT, CRGB::Black); }

// Light one physical run (10 LEDs) of a digit.
void lightRun(int digit, int run, CRGB c) {
  uint16_t base = DIGIT_BASE[digit] + run * LEDS_PER_SEGMENT;
  for (int i = 0; i < LEDS_PER_SEGMENT; i++) leds[base + i] = c;
}

// Light a logical segment (a..g) of a digit, via the mapping table.
void lightSegment(int digit, int seg, CRGB c) {
  lightRun(digit, SEG_RUN[digit][seg], c);
}

void drawDigit(int digit, int value, CRGB c) {
  uint8_t mask = SEG_FONT[value];
  for (int s = 0; s < 7; s++)
    if (mask & (1 << s)) lightSegment(digit, s, c);
}

// Effect color for a single physical LED index.
CRGB fxColor(uint16_t idx) {
  switch (settings.effect) {
    case FX_RAINBOW: return CHSV(gHue + idx * RAINBOW_SPREAD, 255, 255);
    case FX_CYCLE:   return CHSV(gHue, 255, 255);
    default:         return colorOn;
  }
}

// Draw a digit using the active effect for its color (per-LED).
void drawDigitFX(int digit, int value) {
  uint8_t mask = SEG_FONT[value];
  for (int s = 0; s < 7; s++) {
    if (!(mask & (1 << s))) continue;
    uint16_t base = DIGIT_BASE[digit] + SEG_RUN[digit][s] * LEDS_PER_SEGMENT;
    for (int i = 0; i < LEDS_PER_SEGMENT; i++) leds[base + i] = fxColor(base + i);
  }
}

void drawColon(bool on) {
  CRGB c = on ? colorColon : CRGB::Black;
  for (int i = 0; i < COLON_COUNT; i++) leds[COLON_LEDS[i]] = c;
}

// ---------------------------------------------------------------------
//  Renderers
// ---------------------------------------------------------------------
// Break a time into the four display digits, honoring the 12/24h setting.
// blankLeading = true means the tens-of-hours digit should be blanked.
void clockDigits(const struct tm& t, int& d0, int& d1, int& d2, int& d3, bool& blankLeading) {
  int hh = t.tm_hour;                          // 0..23
  if (settings.use12h) { hh %= 12; if (hh == 0) hh = 12; }
  d0 = hh / 10;  d1 = hh % 10;
  d2 = t.tm_min / 10;  d3 = t.tm_min % 10;
  blankLeading = settings.use12h ? (d0 == 0) : (BLANK_LEADING_ZERO_HOUR && d0 == 0);
}

void renderClock() {
  struct tm t;
  if (!getLocalTime(&t, 5)) {         // non-blocking-ish poll
    // No time yet: dim red heartbeat on the first pixel.
    clearAll();
    leds[0] = CRGB(16, 0, 0);
    FastLED.show();
    return;
  }
  timeSynced = true;

  int d0, d1, d2, d3; bool blankLeading;
  clockDigits(t, d0, d1, d2, d3, blankLeading);

  // Advance the effect hue (frame-rate independent).
  gHue = (uint8_t)((millis() * (settings.speed + 1)) >> 10);

  clearAll();
  if (!blankLeading) drawDigitFX(0, d0);
  drawDigitFX(1, d1);
  drawDigitFX(2, d2);
  drawDigitFX(3, d3);
  drawColon((t.tm_sec % 2) == 0);     // colon keeps its own color, 1 Hz blink

  FastLED.show();

  static int lastSec = -1;            // serial heartbeat for verification
  if (t.tm_sec != lastSec) {
    lastSec = t.tm_sec;
    Serial.printf("time %02d:%02d:%02d\n", t.tm_hour, t.tm_min, t.tm_sec);
  }
}

void renderCalibrate() {
  clearAll();
  lightRun(calDigit, calRun, CRGB::Green);
  FastLED.show();
}

// Light every run of every digit at once, each run in its own color, so a
// single photo reveals the physical run->segment layout for all 4 digits.
void renderColorMap() {
  clearAll();
  for (int d = 0; d < NUM_DIGITS; d++)
    for (int r = 0; r < 7; r++)
      lightRun(d, r, RUN_COLORS[r]);
  FastLED.show();
}

// Brief power-on self-test: show "88:88" so every segment + the colon
// lights. Good sanity check on wiring and PSU headroom.
void selfTest() {
  clearAll();
  for (int d = 0; d < NUM_DIGITS; d++) drawDigit(d, 8, colorOn);
  drawColon(true);
  FastLED.show();
}

// ---------------------------------------------------------------------
//  Serial console
// ---------------------------------------------------------------------
void printHelp() {
  Serial.println();
  Serial.println(F("=== 7-seg clock console ==="));
  Serial.println(F("  c  clock mode"));
  Serial.println(F("  k  calibrate mode"));
  Serial.println(F("  t  self-test (88:88)"));
  Serial.println(F("  +  brighter    -  dimmer"));
  Serial.println(F("Calibrate mode:"));
  Serial.println(F("  n / p   next / prev digit (0..3)"));
  Serial.println(F("  ] / [   next / prev run   (0..6)"));
  Serial.println(F("  o       colon test"));
  Serial.println(F("  h  help"));
}

void reportCal() {
  Serial.printf("[cal] digit=%d  run=%d  (physical LEDs %d..%d)\n",
                calDigit, calRun,
                DIGIT_BASE[calDigit] + calRun * LEDS_PER_SEGMENT,
                DIGIT_BASE[calDigit] + calRun * LEDS_PER_SEGMENT + LEDS_PER_SEGMENT - 1);
}

void handleSerial() {
  while (Serial.available()) {
    char ch = Serial.read();
    switch (ch) {
      case 'c': mode = MODE_CLOCK;     Serial.println(F("-> clock"));     break;
      case 'k': mode = MODE_CALIBRATE; Serial.println(F("-> calibrate")); reportCal(); renderCalibrate(); break;
      case 'm': mode = MODE_MAP;       Serial.println(F("-> color-map")); renderColorMap(); break;
      case 't': selfTest();            Serial.println(F("-> self-test")); break;
      case '+': settings.brightness = qadd8(settings.brightness, 15); applySettings(); saveSettings(); Serial.printf("brightness=%u\n", settings.brightness); break;
      case '-': settings.brightness = qsub8(settings.brightness, 15); applySettings(); saveSettings(); Serial.printf("brightness=%u\n", settings.brightness); break;
      case 'h': printHelp(); break;
      // calibrate navigation
      case 'n': if (mode == MODE_CALIBRATE) { calDigit = (calDigit + 1) % NUM_DIGITS; reportCal(); renderCalibrate(); } break;
      case 'p': if (mode == MODE_CALIBRATE) { calDigit = (calDigit + NUM_DIGITS - 1) % NUM_DIGITS; reportCal(); renderCalibrate(); } break;
      case ']': if (mode == MODE_CALIBRATE) { calRun = (calRun + 1) % 7; reportCal(); renderCalibrate(); } break;
      case '[': if (mode == MODE_CALIBRATE) { calRun = (calRun + 6) % 7; reportCal(); renderCalibrate(); } break;
      case 'o': if (mode == MODE_CALIBRATE) { clearAll(); drawColon(true); FastLED.show(); Serial.println(F("[cal] colon")); } break;
      default: break;
    }
  }
}

// ---------------------------------------------------------------------
//  Web interface
// ---------------------------------------------------------------------
void sendState() {
  // Current time formatted per the 12/24h setting (leading zero blanked in 12h).
  char timeStr[8] = "--:--";
  struct tm t;
  if (getLocalTime(&t, 5)) {
    int d0, d1, d2, d3; bool blankLeading;
    clockDigits(t, d0, d1, d2, d3, blankLeading);
    if (blankLeading) snprintf(timeStr, sizeof(timeStr), "%d:%d%d", d1, d2, d3);
    else              snprintf(timeStr, sizeof(timeStr), "%d%d:%d%d", d0, d1, d2, d3);
  }
  char buf[220];
  snprintf(buf, sizeof(buf),
           "{\"time\":\"%s\",\"brightness\":%u,\"color\":\"%06X\",\"colon\":\"%06X\","
           "\"effect\":%u,\"speed\":%u,\"fmt\":%u}",
           timeStr, settings.brightness, settings.color & 0xFFFFFF,
           settings.colonColor & 0xFFFFFF, settings.effect, settings.speed,
           settings.use12h ? 12 : 24);
  server.send(200, "application/json", buf);
}

void handleSet() {
  if (server.hasArg("brightness"))
    settings.brightness = constrain(server.arg("brightness").toInt(), 0, 255);
  if (server.hasArg("color"))
    settings.color = strtoul(server.arg("color").c_str(), nullptr, 16) & 0xFFFFFF;
  if (server.hasArg("colon"))
    settings.colonColor = strtoul(server.arg("colon").c_str(), nullptr, 16) & 0xFFFFFF;
  if (server.hasArg("effect"))
    settings.effect = constrain(server.arg("effect").toInt(), 0, 2);
  if (server.hasArg("speed"))
    settings.speed = constrain(server.arg("speed").toInt(), 1, 255);
  if (server.hasArg("fmt"))
    settings.use12h = (server.arg("fmt").toInt() == 12);

  applySettings();
  saveSettings();
  sendState();
}

// ---------------------------------------------------------------------
//  OTA — HTTP firmware push to /update (multipart POST, HTTP basic auth).
//  Outbound from the uploader's side, so no PC firewall involvement.
// ---------------------------------------------------------------------
bool otaAuthed = false;

// Receives the uploaded .bin in chunks and streams it to flash.
void handleUpdateUpload() {
  HTTPUpload& up = server.upload();
  switch (up.status) {
    case UPLOAD_FILE_START:
      otaAuthed = server.authenticate("admin", OTA_PASSWORD);
      if (!otaAuthed) return;                 // gate before touching flash
      mode = MODE_CLOCK;
      Serial.printf("OTA(http): start %s\n", up.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      break;

    case UPLOAD_FILE_WRITE:
      if (!otaAuthed) return;
      if (Update.write(up.buf, up.currentSize) != up.currentSize) {
        Update.printError(Serial);
      } else {
        // Throttled blue progress bar (~total firmware is ~0.85 MB).
        static uint32_t lastShown = 0;
        if (up.totalSize - lastShown >= 24000) {
          lastShown = up.totalSize;
          uint16_t n = min((uint32_t)LED_COUNT, up.totalSize / 3000);
          FastLED.clear();
          for (uint16_t i = 0; i < n; i++) leds[i] = CRGB(0, 0, 48);
          FastLED.show();
        }
      }
      break;

    case UPLOAD_FILE_END:
      if (!otaAuthed) return;
      if (Update.end(true)) Serial.printf("OTA(http): done, %u bytes\n", up.totalSize);
      else                  Update.printError(Serial);
      break;

    default: break;
  }
}

// Runs after the upload completes: reports result and reboots on success.
void handleUpdateDone() {
  if (!otaAuthed) { server.requestAuthentication(); return; }
  bool ok = !Update.hasError();
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", ok ? "OK - rebooting" : "FAIL");
  if (ok) {
    fill_solid(leds, LED_COUNT, CRGB(0, 40, 0));   // green = success
    FastLED.show();
    delay(700);
    ESP.restart();
  }
}

void startWebServer() {
  server.on("/", []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/state", sendState);
  server.on("/set", handleSet);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.begin();
  Serial.printf("Web UI: http://%s/\n", WiFi.localIP().toString().c_str());
}

// ---------------------------------------------------------------------
//  WiFi + time
// ---------------------------------------------------------------------
void connectWiFi() {
  Serial.printf("WiFi: connecting to \"%s\"", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(300);
    Serial.print('.');
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi: connected, IP %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println(F("\nWiFi: FAILED (will keep retrying in background)"));
  }
}

void startTime() {
  // SNTP against the local server; TZ string handles Central + DST.
  configTzTime(TZ_STRING, NTP_SERVER);
  Serial.printf("NTP: querying %s (TZ %s)\n", NTP_SERVER, TZ_STRING);
}

// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("\n\n7-seg WS2812 clock booting..."));

  loadSettings();

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
  applySettings();     // sets brightness + colors from saved settings
  clearAll();
  FastLED.show();

  selfTest();          // flash 88:88 so we can eyeball the wiring
  delay(1500);

  connectWiFi();
  startTime();

  if (WiFi.status() == WL_CONNECTED) {
    if (MDNS.begin("clock")) {
      MDNS.addService("http", "tcp", 80);
      Serial.println(F("mDNS: http://clock.local/"));
    }
    startWebServer();
  }

  printHelp();

  if (mode == MODE_MAP) renderColorMap();   // show calibration colors immediately
}

void loop() {
  handleSerial();
  server.handleClient();

  // keep WiFi alive
  static uint32_t lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 5000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
  }

  if (mode == MODE_CLOCK) {
    // Animate at ~30 fps for effects; a lazy 250 ms is plenty when solid.
    uint32_t interval = (settings.effect == FX_SOLID) ? 250 : 33;
    static uint32_t lastDraw = 0;
    if (millis() - lastDraw >= interval) {
      lastDraw = millis();
      renderClock();
    }
  }
  // calibrate mode only redraws on command
}
