/*
  Matchbox Traffic Light (UNO R4 WiFi)

  Hardware:
    - Arduino UNO R4 WiFi
    - 3 relays:
        RED    -> D2
        YELLOW -> D3
        GREEN  -> D4
      (Assumed active-LOW: LOW = relay ON)
    - Modulino Distance (VL53L4CD) via Qwiic / I2C
      using STM32duino VL53L4CD library
    - 3-Button Modulino:
        Button 1 output -> D8  (active-LOW)  = Pedestrian Call
        (Buttons 2 & 3 unused)
    - Onboard 12x8 LED Matrix:
        - "X"  = DON'T WALK
        - Walker icon = WALK
        - 9..0 countdown in PED phase
    - Wi-Fi Access Point:
        SSID: TrafficLight-UNO
        PASS: carGO2025
        Status page: http://192.168.4.1

  Behavior:
    States: RED, PED, GREEN, YELLOW, COOLDOWN

    RED:
      - Cars RED, matrix shows DON'T WALK.
      - If pedRequestPending -> go PED.
      - Else if carHere (distance sensor) -> go GREEN.
      - Else stay RED.

    PED:
      - Cars RED.
      - Matrix shows WALK briefly, then countdown 9..0.
      - After countdown:
          if carHere -> GREEN
          else       -> RED

    GREEN:
      - Cars GREEN, matrix DON'T WALK.
      - After GREEN_TIME -> YELLOW.

    YELLOW:
      - Cars YELLOW, DON'T WALK.
      - After YELLOW_TIME -> COOLDOWN.

    COOLDOWN:
      - Cars RED, DON'T WALK.
      - After COOLDOWN_MS -> RED.

    Ped Call (Button 1 on D8):
      - Pressed (in any state except PED) sets pedRequestPending = true.
      - Served at next RED as above.
*/

#include <Wire.h>
#include <WiFiS3.h>
#include <Arduino_LED_Matrix.h>
#include <Arduino_Modulino.h>

// ---------- Distance sensor ----------
VL53L4CD tof(&Wire, -1);      // -1: no XSHUT pin used
VL53L4CD_Result_t tofResult;

// ---------- LED Matrix ----------
ArduinoLEDMatrix matrix;
uint8_t frame[8][12];         // 8 rows x 12 columns: 0 = off, 1 = on

// ---------- Wi-Fi AP ----------
const char* AP_SSID = "TrafficLight-UNO";
const char* AP_PASS = "carGO2025";
WiFiServer server(80);

// ---------- Relays ----------
const bool RELAY_ACTIVE_LOW = true;  // LOW = ON on most relay boards
const uint8_t PIN_RED    = 2;
const uint8_t PIN_YELLOW = 3;
const uint8_t PIN_GREEN  = 4;

// ---------- Ped Button (BTN1) ----------
const uint8_t PIN_PED_BTN   = 8;     // active-LOW, INPUT_PULLUP
const unsigned long BTN_DEBOUNCE_MS = 40;
bool btnLastRaw = false;
unsigned long btnChangeTime = 0;

// ---------- Timings (ms) ----------
unsigned long GREEN_TIME      = 5000;
unsigned long YELLOW_TIME     = 3000;
unsigned long COOLDOWN_MS     = 2000;
unsigned long PED_COUNT_TOTAL = 9000;   // 9 seconds (9..0)

// ---------- Distance thresholds (mm) ----------
uint16_t TRIGGER_NEAR_MM = 120;   // car present if < this
uint16_t CLEAR_FAR_MM    = 200;   // car gone if >= this
unsigned long DEBOUNCE_MS = 60;   // presence debounce

// ---------- State machine ----------
enum Phase { RED, PED, GREEN, YELLOW, COOLDOWN };

Phase         phase             = RED;
unsigned long tPhase            = 0;
bool          pedRequestPending = false;
volatile unsigned long carCount = 0;

// ==========================================================
// Relay helpers
// ==========================================================
void relayWrite(uint8_t pin, bool on) {
  digitalWrite(
    pin,
    RELAY_ACTIVE_LOW
      ? (on ? LOW : HIGH)
      : (on ? HIGH : LOW)
  );
}

void carsRed()    { relayWrite(PIN_RED, true);  relayWrite(PIN_YELLOW, false); relayWrite(PIN_GREEN, false); }
void carsYellow() { relayWrite(PIN_RED, false); relayWrite(PIN_YELLOW, true);  relayWrite(PIN_GREEN, false); }
void carsGreen()  { relayWrite(PIN_RED, false); relayWrite(PIN_YELLOW, false); relayWrite(PIN_GREEN, true);  }

// ==========================================================
// Matrix helpers (using frame[8][12] + renderBitmap())
// ==========================================================
void clearFrame() {
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 12; c++) {
      frame[r][c] = 0;
    }
  }
}

void setPixel(int r, int c) {
  if (r >= 0 && r < 8 && c >= 0 && c < 12) {
    frame[r][c] = 1;
  }
}

void drawDontWalkX() {
  clearFrame();
  // Big X from corners across 8x12
  for (int r = 0; r < 8; r++) {
    int c1 = r;          // \
    int c2 = 11 - r;     // /
    if (c1 >= 0 && c1 < 12) setPixel(r, c1);
    if (c2 >= 0 && c2 < 12) setPixel(r, c2);
  }
  matrix.renderBitmap(frame, 8, 12);
}

void drawWalkMan() {
  clearFrame();
  // Simple stick figure near center
  // Head
  setPixel(1, 6);
  // Body
  setPixel(2, 6);
  setPixel(3, 6);
  setPixel(4, 6);
  // Arms
  setPixel(3, 5);
  setPixel(3, 7);
  // Legs
  setPixel(5, 5);
  setPixel(6, 4);
  setPixel(5, 7);
  setPixel(6, 8);

  matrix.renderBitmap(frame, 8, 12);
}

// Draw a crude big digit 0–9 centered-ish
void drawDigit(int d) {
  if (d < 0) d = 0;
  if (d > 9) d = 9;

  clearFrame();

  int left   = 3;
  int right  = 7;
  int top    = 0;
  int bottom = 6;

  switch (d) {
    case 0:
      for (int c = left; c <= right; c++) {
        setPixel(top, c);
        setPixel(bottom, c);
      }
      for (int r = top; r <= bottom; r++) {
        setPixel(r, left);
        setPixel(r, right);
      }
      break;

    case 1:
      for (int r = top; r <= bottom; r++) {
        setPixel(r, right - 1);
      }
      setPixel(top + 1, right - 2);
      break;

    case 2:
      for (int c = left; c <= right; c++) {
        setPixel(top, c);
        setPixel(bottom, c);
      }
      for (int r = top; r <= (top + bottom) / 2; r++) {
        setPixel(r, right);
      }
      for (int r = (top + bottom) / 2; r <= bottom; r++) {
        setPixel(r, left);
      }
      break;

    case 3:
      for (int c = left; c <= right; c++) {
        setPixel(top, c);
        setPixel(bottom, c);
      }
      for (int r = top; r <= bottom; r++) {
        setPixel(r, right);
      }
      setPixel((top + bottom) / 2, left + 1);
      break;

    case 4:
      for (int r = top; r <= (top + bottom) / 2; r++) {
        setPixel(r, left);
      }
      for (int r = top; r <= bottom; r++) {
        setPixel(r, right);
      }
      for (int c = left; c <= right; c++) {
        setPixel((top + bottom) / 2, c);
      }
      break;

    case 5:
      for (int c = left; c <= right; c++) {
        setPixel(top, c);
        setPixel(bottom, c);
      }
      for (int r = top; r <= (top + bottom) / 2; r++) {
        setPixel(r, left);
      }
      for (int r = (top + bottom) / 2; r <= bottom; r++) {
        setPixel(r, right);
      }
      break;

    case 6:
      for (int c = left; c <= right; c++) {
        setPixel(top, c);
        setPixel(bottom, c);
      }
      for (int r = top; r <= bottom; r++) {
        setPixel(r, left);
      }
      for (int r = (top + bottom) / 2; r <= bottom; r++) {
        setPixel(r, right);
      }
      break;

    case 7:
      for (int c = left; c <= right; c++) {
        setPixel(top, c);
      }
      for (int r = top; r <= bottom; r++) {
        setPixel(r, right);
      }
      break;

    case 8:
      for (int c = left; c <= right; c++) {
        setPixel(top, c);
        setPixel(bottom, c);
      }
      for (int r = top; r <= bottom; r++) {
        setPixel(r, left);
        setPixel(r, right);
      }
      for (int c = left; c <= right; c++) {
        setPixel((top + bottom) / 2, c);
      }
      break;

    case 9:
      for (int c = left; c <= right; c++) {
        setPixel(top, c);
        setPixel(bottom, c);
      }
      for (int r = top; r <= bottom; r++) {
        setPixel(r, right);
      }
      for (int r = top; r <= (top + bottom) / 2; r++) {
        setPixel(r, left);
      }
      break;
  }

  matrix.renderBitmap(frame, 8, 12);
}

void showWalk()     { drawWalkMan(); }
void showDontWalk() { drawDontWalkX(); }

// ==========================================================
// Misc helpers
// ==========================================================
const char* phaseName() {
  switch (phase) {
    case RED:      return "RED";
    case PED:      return "PED (Countdown)";
    case GREEN:    return "GREEN";
    case YELLOW:   return "YELLOW";
    case COOLDOWN: return "RED (Cooldown)";
  }
  return "?";
}

// Distance presence with hysteresis + debounce
bool presence(bool got, uint16_t mm) {
  static bool raw = false;
  static unsigned long tEdge = 0;
  bool nextRaw = raw;

  if (got) {
    if (!raw && mm > 0 && mm < TRIGGER_NEAR_MM) {
      nextRaw = true;
      tEdge = millis();
    }
    if (raw && mm >= CLEAR_FAR_MM) {
      nextRaw = false;
      tEdge = millis();
    }
  }

  if (nextRaw != raw && (millis() - tEdge) >= DEBOUNCE_MS) {
    raw = nextRaw;
  }

  return raw;
}

// Ped button (debounced, active-LOW)
bool readPedButtonPressed() {
  bool raw = (digitalRead(PIN_PED_BTN) == LOW);
  static bool debounced = false;

  if (raw != btnLastRaw) {
    btnChangeTime = millis();
    btnLastRaw = raw;
  }

  if (millis() - btnChangeTime >= BTN_DEBOUNCE_MS) {
    debounced = btnLastRaw;
  }

  return debounced;
}

// Phase change
void setPhase(Phase p) {
  phase  = p;
  tPhase = millis();

  switch (p) {
    case RED:
      carsRed();
      showDontWalk();
      break;

    case PED:
      carsRed();
      showWalk();    // numbers will override in loop
      break;

    case GREEN:
      carsGreen();
      showDontWalk();
      carCount++;
      break;

    case YELLOW:
      carsYellow();
      showDontWalk();
      break;

    case COOLDOWN:
      carsRed();
      showDontWalk();
      break;
  }
}

// ==========================================================
// Setup
// ==========================================================
void setup() {
  // Serial optional:
  // Serial.begin(115200); while (!Serial) {}

  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_YELLOW, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  carsRed();

  pinMode(PIN_PED_BTN, INPUT_PULLUP);

  matrix.begin();
  showDontWalk();

  // VL53L4CD init (STM32duino)
  Wire.begin();

  if (tof.begin() != 0) {
    while (true) { carsYellow(); delay(200); carsRed(); delay(200); }
  }

  if (tof.InitSensor() != VL53L4CD_ERROR_NONE) {
    while (true) { carsYellow(); delay(80); carsRed(); delay(80); }
  }

  // 33ms timing, continuous ranging
  tof.VL53L4CD_SetRangeTiming(33, 0);
  tof.VL53L4CD_StartRanging();

  // Wi-Fi AP
  if (WiFi.beginAP(AP_SSID, AP_PASS) != WL_AP_LISTENING) {
    while (true) { carsRed(); delay(250); carsYellow(); delay(250); }
  }
  server.begin();

  setPhase(RED);
}

// ==========================================================
// Loop
// ==========================================================
void loop() {
  unsigned long now = millis();

  // 1) Distance read (non-blocking)
  bool     got   = false;
  uint16_t mm    = 0;
  uint8_t  ready = 0;

  tof.VL53L4CD_CheckForDataReady(&ready);
  if (ready) {
    tof.VL53L4CD_ClearInterrupt();
    if (tof.VL53L4CD_GetResult(&tofResult) == VL53L4CD_ERROR_NONE) {
      if (tofResult.range_status == 0) {
        mm = tofResult.distance_mm;
        got = true;
      }
    }
  }

  bool carHere = presence(got, mm);

  // 2) Ped button: queue request if pressed (unless already in PED)
  if (readPedButtonPressed() && phase != PED) {
    pedRequestPending = true;
  }

  // 3) State machine
  switch (phase) {

    case RED:
      if (pedRequestPending) {
        pedRequestPending = false;
        setPhase(PED);             // start ped phase
      } else if (carHere) {
        setPhase(GREEN);           // give green to car
      }
      break;

    case PED: {
      unsigned long elapsed = now - tPhase;
      if (elapsed >= PED_COUNT_TOTAL) {
        // Countdown finished
        if (carHere) setPhase(GREEN);
        else         setPhase(RED);
      } else {
        int remaining = (int)((PED_COUNT_TOTAL - elapsed + 999) / 1000); // ceil
        if (remaining < 0) remaining = 0;
        if (remaining > 9) remaining = 9;
        drawDigit(remaining);
      }
    } break;

    case GREEN:
      if (now - tPhase >= GREEN_TIME) {
        setPhase(YELLOW);
      }
      break;

    case YELLOW:
      if (now - tPhase >= YELLOW_TIME) {
        setPhase(COOLDOWN);
      }
      break;

    case COOLDOWN:
      if (now - tPhase >= COOLDOWN_MS) {
        setPhase(RED);
      }
      break;
  }

  // 4) HTTP status page on AP: http://192.168.4.1/
  WiFiClient client = server.available();
  if (client) {
    while (client.connected() && !client.available()) {
      delay(1);
    }
    while (client.available()) {
      client.read(); // discard request
    }

    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: text/html; charset=utf-8"));
    client.println(F("Connection: close"));
    client.println();
    client.println(F(
      "<!doctype html><html>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<style>"
      "body{font:16px system-ui;margin:18px}"
      ".tag{display:inline-block;padding:.2em .6em;border-radius:.5em;background:#eee}"
      "h1{margin:0 0 .4em 0;font-size:1.4rem}"
      "small{color:#666}"
      "</style>"
      "<h1>UNO R4 Traffic Light</h1>"
    ));
    client.print(F("<p>Phase: <span class='tag'>")); client.print(phaseName()); client.println(F("</span></p>"));
    client.print(F("<p>Car present: <span class='tag'>")); client.print(carHere ? "YES" : "NO"); client.println(F("</span></p>"));
    client.print(F("<p>Ped request queued: <span class='tag'>"));
    client.print(pedRequestPending ? "YES" : "NO");
    client.println(F("</span></p>"));
    client.print(F("<p>Car count (GREEN entries): <span class='tag'>"));
    client.print(carCount);
    client.println(F("</span></p>"));
    client.println(F(
      "<p><small>"
      "Connect to Wi-Fi AP <b>TrafficLight-UNO</b>, then open <b>http://192.168.4.1</b>."
      "</small></p>"
      "</html>"
    ));
    client.stop();
  }
}
