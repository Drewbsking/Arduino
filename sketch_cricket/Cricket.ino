#include "arduino_secrets.h"

/*  UNO R4 WiFi + Adafruit 2.8" TFT Touch Shield (PID 1947) + Modulino Buzzer + Modulino Buttons
    Sporadic "smoke-alarm-like" cricket with three intensity profiles via Buttons:
      - A = LOTS (frequent, extra doubles/triples)
      - B = MEDIUM (default; previous behavior)
      - C = LESS (rare, mostly singles)

    Notes:
      - Pressing A/B/C updates the profile immediately and reschedules the next burst.
      - If your Modulino Buttons API differs, adjust serviceButtons() accordingly.
*/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Modulino.h>
#include <math.h>

// ---- Display pins (PID 1947 shield on UNO form factor) ----
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8   // set -1 if tied to RESET
Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

// ---- Modulino Buzzer (Qwiic/I2C) ----
ModulinoBuzzer buzzer;
bool buzzerOK = false;

// ---- Modulino Buttons (Qwiic/I2C) ----
// Assumes a class named ModulinoButtons with begin() and read() -> bitmask {A=bit0,B=bit1,C=bit2}
ModulinoButtons buttons;
bool buttonsOK = false;

// ---- Colors ----
const uint16_t COL_BG     = ILI9341_BLACK;
const uint16_t COL_GRASS  = ILI9341_DARKGREEN;
const uint16_t COL_BODY   = 0x2F6A;
const uint16_t COL_WING   = ILI9341_YELLOW;
const uint16_t COL_LEG    = 0xA145;
const uint16_t COL_EYE    = ILI9341_WHITE;
const uint16_t COL_PUPIL  = ILI9341_BLACK;
const uint16_t COL_TEXT   = ILI9341_CYAN;

// ---- Cricket placement & size ----
int cx, cy;
int scaleFactor = 2; // 1..3 to resize

// ---- Cricket timbre ----
int   CENTER_FREQ_HZ         = 4700; // 4.7 kHz-ish
int   FM_DEVIATION_HZ        = 320;  // +/- within a syllable
float FM_CYCLES_PER_SYLLABLE = 2.4f; // FM wobble per syllable

int   SYLLABLE_MS            = 38;   // one crisp "syllable"
int   SUBFRAME_MS            = 6;    // micro-pulse size
int   SUBFRAME_OFF_MS        = 1;    // brief off inside syllable

int   INTER_SYLLABLE_GAP_MS  = 55;   // within-burst gap (for multi-chirp)
int   RAND_FREQ_JITTER       = 120;
int   RAND_TIME_JITTER_MS    = 5;

// ---- SPORADIC scheduling (annoyance controls) ----
// Converted to VARIABLES so the profile can change at runtime.
unsigned long MEAN_IDLE_MS     = 45000UL;  // average wait between bursts
unsigned long MIN_IDLE_MS      = 12000UL;  // never sooner than 12s
unsigned long MAX_IDLE_MS      = 120000UL; // never later than 120s

float DOUBLE_PROB = 0.33f; // 33% chance of a double chirp
float TRIPLE_PROB = 0.07f; // 7% chance of a triple (rare = extra annoying)
const int   INTER_CHIRP_BURST_MS_MIN = 350;  // gap between chirps in a burst
const int   INTER_CHIRP_BURST_MS_MAX = 700;

// ---- Idle screen twitch so it doesnât look frozen ----
const unsigned long IDLE_WING_TWITCH_EVERY_MS_MIN = 1800;
const unsigned long IDLE_WING_TWITCH_EVERY_MS_MAX = 4200;

// ---- Profiles ----
enum ChirpProfile { LOTS, MEDIUM, LESS };
ChirpProfile currentProfile = MEDIUM;

// -------------------------------------------------------------------
// Drawing helpers
// -------------------------------------------------------------------
void drawGrass() {
  tft.fillRect(0, tft.height() - 30, tft.width(), 30, COL_GRASS);
}

void drawCricketBody() {
  int bodyW = 60 * scaleFactor;
  int bodyH = 28 * scaleFactor;
  int headR = 10 * scaleFactor;

  for (int i = 0; i < bodyH / 2; i++) {
    int w = bodyW - (i * bodyW) / (bodyH / 2);
    tft.drawFastHLine(cx - w / 2, cy - i, w, COL_BODY);
    tft.drawFastHLine(cx - w / 2, cy + i, w, COL_BODY);
  }

  tft.fillCircle(cx - (bodyW / 2) + headR + 4 * scaleFactor, cy - bodyH / 6, headR, COL_BODY);

  tft.fillCircle(cx - (bodyW / 2) + headR + 2 * scaleFactor, cy - bodyH / 6 - 2 * scaleFactor, 3 * scaleFactor, COL_EYE);
  tft.fillCircle(cx - (bodyW / 2) + headR + 2 * scaleFactor, cy - bodyH / 6 - 2 * scaleFactor, 1 * scaleFactor, COL_PUPIL);

  int ax = cx - (bodyW / 2) + headR;
  int ay = cy - bodyH / 6;
  tft.drawLine(ax, ay, ax - 18 * scaleFactor, ay - 16 * scaleFactor, COL_LEG);
  tft.drawLine(ax + 3 * scaleFactor, ay, ax + 18 * scaleFactor, ay - 18 * scaleFactor, COL_LEG);

  int legY = cy + bodyH / 2 - 2 * scaleFactor;
  tft.drawLine(cx - 10 * scaleFactor, legY, cx - 28 * scaleFactor, legY + 16 * scaleFactor, COL_LEG);
  tft.drawLine(cx + 6 * scaleFactor,  legY, cx + 28 * scaleFactor, legY + 16 * scaleFactor, COL_LEG);

  tft.drawLine(cx - 6 * scaleFactor, cy + bodyH / 3, cx - 22 * scaleFactor, cy + bodyH / 3 - 16 * scaleFactor, COL_LEG);
  tft.drawLine(cx + 10 * scaleFactor, cy + bodyH / 3, cx + 28 * scaleFactor, cy + bodyH / 3 - 18 * scaleFactor, COL_LEG);
}

void drawWings(bool frameA) {
  int bodyW = 60 * scaleFactor;
  int bodyH = 28 * scaleFactor;
  int wx0 = cx - bodyW / 6;
  int wy0 = cy - bodyH / 3;

  tft.fillRect(cx - bodyW / 3, cy - bodyH / 2, (bodyW * 2) / 3, bodyH, COL_BODY);

  if (frameA) {
    tft.fillTriangle(wx0, wy0,
                     wx0 - 24 * scaleFactor, wy0 + 10 * scaleFactor,
                     wx0 + 6 * scaleFactor,  wy0 + 22 * scaleFactor,
                     COL_WING);
    tft.fillTriangle(wx0 + 8 * scaleFactor,  wy0 + 2 * scaleFactor,
                     wx0 + 34 * scaleFactor, wy0 + 12 * scaleFactor,
                     wx0 + 6 * scaleFactor,  wy0 + 24 * scaleFactor,
                     COL_WING);
  } else {
    tft.fillTriangle(wx0, wy0 - 2 * scaleFactor,
                     wx0 - 20 * scaleFactor, wy0 + 12 * scaleFactor,
                     wx0 + 8 * scaleFactor,  wy0 + 20 * scaleFactor,
                     COL_WING);
    tft.fillTriangle(wx0 + 10 * scaleFactor, wy0,
                     wx0 + 32 * scaleFactor, wy0 + 8 * scaleFactor,
                     wx0 + 6 * scaleFactor,  wy0 + 22 * scaleFactor,
                     COL_WING);
  }

  tft.drawLine(wx0 - 8 * scaleFactor, wy0 + 8 * scaleFactor,
               wx0 + 2 * scaleFactor,  wy0 + 18 * scaleFactor, COL_BODY);
  tft.drawLine(wx0 + 16 * scaleFactor, wy0 + 8 * scaleFactor,
               wx0 + 2 * scaleFactor,  wy0 + 18 * scaleFactor, COL_BODY);
}

void drawChirpMarks(bool on) {
  int x = cx + 44 * scaleFactor;
  int y = cy - 6 * scaleFactor;
  uint16_t c = on ? COL_TEXT : COL_BG;
  tft.drawLine(x, y, x + 10 * scaleFactor, y - 8 * scaleFactor, c);
  tft.drawLine(x - 4 * scaleFactor, y + 6 * scaleFactor, x + 8 * scaleFactor, y + 2 * scaleFactor, c);
}

void drawTitle() {
  tft.setTextSize(2);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setCursor(8, 8);
  tft.print("Cricket!");
}

void drawModeBanner() {
  // Clear area under title for mode + help
  tft.fillRect(0, 24, tft.width(), 26, COL_BG);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_YELLOW, COL_BG);
  tft.setCursor(8, 28);
  tft.print("Mode: ");
  switch (currentProfile) {
    case LOTS:   tft.print("LOTS");   break;
    case MEDIUM: tft.print("MEDIUM"); break;
    case LESS:   tft.print("LESS");   break;
  }
  tft.setCursor(160, 28);
  tft.setTextColor(ILI9341_CYAN, COL_BG);
  tft.print("A=Lots  B=Med  C=Less");
}

void drawScene() {
  tft.fillScreen(COL_BG);
  drawTitle();
  drawModeBanner();
  drawGrass();
  drawCricketBody();
  drawWings(true);
}

// -------------------------------------------------------------------
// SOUND: one syllable (micro-pulsed, light FM)  ~40 ms total
// -------------------------------------------------------------------
void chirpSyllable(int centerHz, int durationMs, int fmDevHz, float fmCycles, int subMs) {
  drawWings(true);
  drawChirpMarks(true);

  int frames = max(1, durationMs / subMs);
  for (int i = 0; i < frames; i++) {
    float p = (frames <= 1) ? 0.0f : (float)i / (frames - 1);
    float fm  = sinf(6.2831853f * fmCycles * p);
    int   f   = centerHz + (int)(fmDevHz * fm);

    int onMs  = subMs - SUBFRAME_OFF_MS;
    if (onMs < 1) onMs = subMs;

    buzzer.tone(f, onMs);
    delay(onMs);
    if (SUBFRAME_OFF_MS > 0) delay(SUBFRAME_OFF_MS);

    if (i % 2 == 0) drawWings((i / 2) % 2 == 0);
  }

  drawChirpMarks(false);
  drawWings(false);
}

// -------------------------------------------------------------------
// Random helpers (exponential wait feels more "randomly occurring")
// -------------------------------------------------------------------
float urand01() {
  long r = random(1, 10001); // 1..10000
  return (float)r / 10000.0f;
}

unsigned long expWaitMs(float meanMs) {
  float u = urand01();
  float w = -meanMs * logf(u);
  return (unsigned long)w;
}

// -------------------------------------------------------------------
// Emit one burst: single/double/triple short chirps
// -------------------------------------------------------------------
void emitBurst() {
  float u = urand01();
  int chirps = 1;
  if (u < TRIPLE_PROB) chirps = 3;
  else if (u < TRIPLE_PROB + DOUBLE_PROB) chirps = 2;

  for (int c = 0; c < chirps; c++) {
    int fCenter = CENTER_FREQ_HZ + random(-RAND_FREQ_JITTER, RAND_FREQ_JITTER + 1);
    int durMs   = SYLLABLE_MS + random(-RAND_TIME_JITTER_MS, RAND_TIME_JITTER_MS + 1);

    chirpSyllable(fCenter, durMs, FM_DEVIATION_HZ, FM_CYCLES_PER_SYLLABLE, SUBFRAME_MS);

    if (c < chirps - 1) {
      int gap = random(INTER_CHIRP_BURST_MS_MIN, INTER_CHIRP_BURST_MS_MAX + 1);
      delay(gap);
    }
  }
}

// -------------------------------------------------------------------
// Profiles: apply + UI + reschedule
// -------------------------------------------------------------------
unsigned long nextBurstAt = 0;
unsigned long nextIdleWingTwitchAt = 0;

void scheduleNextBurst() {
  unsigned long w = expWaitMs((float)MEAN_IDLE_MS);
  if (w < MIN_IDLE_MS) w = MIN_IDLE_MS;
  if (w > MAX_IDLE_MS) w = MAX_IDLE_MS;
  nextBurstAt = millis() + w;
}

void scheduleNextTwitch() {
  nextIdleWingTwitchAt = millis() + random(IDLE_WING_TWITCH_EVERY_MS_MIN,
                                           IDLE_WING_TWITCH_EVERY_MS_MAX + 1);
}

void applyProfile(ChirpProfile p) {
  currentProfile = p;
  switch (p) {
    case LOTS:
      MEAN_IDLE_MS = 10000UL;   // avg ~10s
      MIN_IDLE_MS  = 3000UL;    // never sooner than 3s
      MAX_IDLE_MS  = 30000UL;   // cap at 30s
      DOUBLE_PROB  = 0.55f;
      TRIPLE_PROB  = 0.20f;
      break;

    case MEDIUM:
      MEAN_IDLE_MS = 45000UL;   // ~45s avg
      MIN_IDLE_MS  = 12000UL;
      MAX_IDLE_MS  = 120000UL;
      DOUBLE_PROB  = 0.33f;
      TRIPLE_PROB  = 0.07f;
      break;

    case LESS:
      MEAN_IDLE_MS = 120000UL;  // ~2min avg
      MIN_IDLE_MS  = 30000UL;
      MAX_IDLE_MS  = 300000UL;  // up to 5min
      DOUBLE_PROB  = 0.18f;
      TRIPLE_PROB  = 0.03f;
      break;
  }
  drawModeBanner();
  // Nudge the schedule so the new profile is felt quickly
  nextBurstAt = millis() + 500; // ~immediate next check
}

// Poll Buttons and switch chirp profile on press.
// Lights the matching button LED for feedback.
void serviceButtons() {
  if (!buttonsOK) return;

  // Only true when the module has a state transition to report (debounced)
  if (!buttons.update()) return;

  bool a = buttons.isPressed(0);  // Button A
  bool b = buttons.isPressed(1);  // Button B
  bool c = buttons.isPressed(2);  // Button C

  // LED feedback on the Buttons module
  buttons.setLeds(a, b, c);

  if (a)      applyProfile(LOTS);
  else if (b) applyProfile(MEDIUM);
  else if (c) applyProfile(LESS);
}



// -------------------------------------------------------------------
// SETUP / LOOP
// -------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));

  Wire.begin();
  Modulino.begin();

  // Buzzer
  buzzer.begin();
  buzzerOK = true;

  // Buttons
  buttons.begin();
  buttonsOK = true; // If your lib exposes isConnected(), you can check it here.

  // TFT
  tft.begin();
  tft.setRotation(1);
  cx = tft.width() / 2;
  cy = tft.height() / 2 + 10;
  drawScene();

  // Start in MEDIUM
  applyProfile(MEDIUM);

  scheduleNextBurst();
  scheduleNextTwitch();
}

void loop() {
  unsigned long now = millis();

  // Poll buttons every loop (non-blocking)
  serviceButtons();

  // Idle wing micro-twitch (tiny visual, ~40 ms, non-intrusive)
  if ((long)(now - nextIdleWingTwitchAt) >= 0) {
    drawWings(true);
    delay(40);
    drawWings(false);
    scheduleNextTwitch();
  }

  // Time to emit a burst?
  if ((long)(now - nextBurstAt) >= 0) {
    emitBurst();
    scheduleNextBurst();
  }
}
