#pragma once
#include <Arduino.h>

// =====================================================================
//  7-Segment WS2812 Clock — hardware configuration
//  Everything that depends on how the string is physically wired lives
//  here. The values below are our best model of the build; the segment
//  ordering gets confirmed with the on-bench calibration chase, then
//  corrected in SEG_RUN[] / COLON_LEDS[].
// =====================================================================

// ---------- Strip ----------
#define LED_PIN            18     // WS2812 data line -> ESP32 GPIO18
#define LED_COUNT          298    // total pixels in the chain
#define LEDS_PER_SEGMENT   10     // each of the 7 segments is 10 LEDs
#define NUM_DIGITS         4      // HH:MM

// Start dim. 298 pixels at full white is ~18 A @ 5 V; keep first
// power-ons gentle until the PSU / power-injection is verified.
#define DEFAULT_BRIGHTNESS 40     // 0-255

// Global current cap, enforced by FastLED at show() time (it scales brightness
// down only on frames that would exceed the budget).
//
// This is NOT about PSU capacity — the LRS-320-5 has 60 A and is injected at the
// string start and midpoint. It's about the ESP32 browning out when the LED
// string sags the shared rail. Measured on this build:
//     solid amber @100  ~1.6 A  -> stable
//     rainbow     @149  ~2.4 A  -> stable
//     solid white @149  ~4.9 A  -> brown-out, and unrecoverable: the WS2812s
//                                  latch the white frame, hold the rail down,
//                                  and it can never boot to clear them.
//     solid white @60   ~2.0 A  -> stable
//     solid white @100  ~3.3 A  -> BROWN-OUT (rst=BROWNOUT), auto-recovers
//
// HONEST CAVEAT: this cap does not actually prevent the brown-out. White @100
// should have been scaled to 2.5 A and been as safe as white @60, and it still
// browned out — so the trigger is most likely the *transient* step in draw when
// a frame jumps brighter, which capacitance absorbs and a current cap cannot.
// Kept as cheap insurance against the most extreme frames, not as a fix.
//
// The real fix (and the way to raise the ceiling) is hardware: bulk capacitance
// across the ESP32's 5 V/GND, and/or feeding the board from the PSU terminals
// rather than downstream of the LED current path.
#define MAX_MILLIAMPS 2500        // 2.5 A @ 5 V

// ---------- Chain layout ----------
// Chain blocks, walking from the data-input end:
//   blkA [0..69]  connHH [70..71]  blkB [72..141]  cross [142..155]
//   blkC [156..225]  connMM [226..227]  blkD [228..297]
// The data line enters at the RIGHT of the display and runs leftward, so
// chain block 0 is the rightmost digit. DIGIT_BASE is therefore indexed by
// on-screen position (0 = leftmost) and reversed relative to chain order,
// so digit 0 = hours-tens sits on the left. (Verified 2026-07-14.)
const uint16_t DIGIT_BASE[NUM_DIGITS] = { 228, 156, 72, 0 };

// SEG_RUN[d][s] = which run (0..6) inside digit d's block holds logical
// segment s. Logical segment order is standard 7-seg:
//        0=a (top)      1=b (top-right)   2=c (bottom-right)
//        3=d (bottom)   4=e (bottom-left) 5=f (top-left)   6=g (middle)
// Because every segment is lit as a whole, direction within a run does
// not matter — only which run maps to which segment. Default is identity
// and is corrected after calibration.
// Calibrated 2026-07-14 from the color-map photo. All four digits wired
// identically:  a=green(1) b=red(0) c=magenta(5) d=cyan(4) e=yellow(3)
//               f=blue(2)  g=white(6)
//   index order here is s = a,b,c,d,e,f,g
const uint8_t SEG_RUN[NUM_DIGITS][7] = {
  { 1, 0, 5, 4, 3, 2, 6 },
  { 1, 0, 5, 4, 3, 2, 6 },
  { 1, 0, 5, 4, 3, 2, 6 },
  { 1, 0, 5, 4, 3, 2, 6 },
};

// Colon dots. Cross block is 142..155 = [2 horiz][10 vertical][2 horiz],
// so the vertical bar is 144..153; the top 3 and bottom 3 of it are the
// two flashing dots. Confirm/adjust with calibration.
const uint16_t COLON_LEDS[] = { 144, 145, 146, 151, 152, 153 };
const uint8_t  COLON_COUNT  = sizeof(COLON_LEDS) / sizeof(COLON_LEDS[0]);

// ---------- Time ----------
#define NTP_SERVER               "192.168.0.156"
#define TZ_STRING                "CST6CDT,M3.2.0,M11.1.0"  // US Central, auto DST
#define BLANK_LEADING_ZERO_HOUR  false   // false => 24h shows 00-23 with leading zero

// ---------- Font ----------
// 7-seg bitmask, bit0=a .. bit6=g
const uint8_t SEG_FONT[10] = {
  0x3F, // 0  a b c d e f
  0x06, // 1  b c
  0x5B, // 2  a b d e g
  0x4F, // 3  a b c d g
  0x66, // 4  b c f g
  0x6D, // 5  a c d f g
  0x7D, // 6  a c d e f g
  0x07, // 7  a b c
  0x7F, // 8  all
  0x6F, // 9  a b c d f g
};
