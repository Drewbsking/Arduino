#include <SPI.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>
#include <Modulino.h>

ModulinoThermo thermo;

// --- TFT pins for Adafruit 2.8" TFT Cap Touch Shield (PID 1947)
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8
Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

// --- Capacitive touch (FT6206 on I2C @ 0x38)
Adafruit_FT6206 ctp;

// --- Colors & layout
const uint16_t BG    = ILI9341_BLACK;
const uint16_t FG    = ILI9341_WHITE;
const uint16_t AXIS  = ILI9341_DARKGREY;
const uint16_t GRID  = ILI9341_NAVY;
const uint16_t COMF  = ILI9341_GREEN;   // comfort line
const uint16_t TEMP  = ILI9341_CYAN;    // temperature line
const uint16_t HUM   = ILI9341_YELLOW;  // humidity line
const uint16_t BAND  = ILI9341_DARKGREY;// bands/frames

// Graph area
const int GRAPH_W = 320, GRAPH_H = 160, GRAPH_X = 0, GRAPH_Y = 60;

// Scales
const float RH_MIN = 0.0f,   RH_MAX = 100.0f;   // left axis (shared with comfort)
const float C_MIN  = 0.0f,   C_MAX  = 100.0f;   // left axis
const float TF_MIN = 60.0f,  TF_MAX = 80.0f;    // right axis typical office range

// Office “good” bands
const float GOOD_TF_MIN = 70.0f, GOOD_TF_MAX = 74.0f;
const float GOOD_RH_MIN = 40.0f, GOOD_RH_MAX = 60.0f;

// Sample cadence
const unsigned long SAMPLE_MS = 10UL * 1000UL;  // 10 s => 1 px = 10 s

// Buffers/state
uint8_t bufComfort[GRAPH_W];
uint8_t bufTempF[GRAPH_W];
uint8_t bufRH[GRAPH_W];
int head = 0;
unsigned long lastSample = 0;
bool paused = false;

// Live readings
float gTempC = NAN, gTempF = NAN, gRH = NAN, gComfort = NAN;

// ===== Helpers =====
static inline float c_to_f(float c) { return c * 9.0f / 5.0f + 32.0f; }

float comfortScoreF(float tempF, float rh) {
  // Simple office-tuned score: 100 if 70–74°F and 40–60% RH, linear penalties outside.
  float tPenalty = 0, hPenalty = 0;
  if (tempF < 70.0f)       tPenalty = (70.0f - tempF) * 2.0f;
  else if (tempF > 74.0f)  tPenalty = (tempF - 74.0f) * 2.0f;

  if (rh < 40.0f)          hPenalty = (40.0f - rh) * 1.0f;
  else if (rh > 60.0f)     hPenalty = (rh - 60.0f) * 1.0f;

  float score = 100.0f - (tPenalty + hPenalty);
  if (score < 0) score = 0;
  if (score > 100) score = 100;
  return score;
}

int mapLinearToY(float v, float vmin, float vmax) {
  // Higher value -> higher on screen: invert to pixel coords
  if (v < vmin) v = vmin;
  if (v > vmax) v = vmax;
  float frac = (v - vmin) / (vmax - vmin);
  int y = GRAPH_Y + (GRAPH_H - 1) - (int)roundf(frac * (GRAPH_H - 1));
  if (y < GRAPH_Y) y = GRAPH_Y;
  if (y > GRAPH_Y + GRAPH_H - 1) y = GRAPH_Y + GRAPH_H - 1;
  return y;
}

void drawLegend() {
  int x = 6, y = 8;
  tft.setTextSize(2);
  tft.setTextColor(FG);
  tft.setCursor(x, y);
  tft.print("Office Comfort Monitor");

  tft.setTextSize(1);
  y += 20;
  tft.setCursor(x, y); tft.print("Tap UL to "); tft.print(paused ? "resume" : "pause");

  // Legend boxes
  int lx = 210, ly = 10, lw = 14, lh = 10, gap = 12;
  tft.fillRect(lx, ly, lw, lh, COMF);  tft.setCursor(lx + lw + 6, ly-1); tft.print("Comfort");
  ly += lh + gap;
  tft.fillRect(lx, ly, lw, lh, TEMP);  tft.setCursor(lx + lw + 6, ly-1); tft.print("Temp (F)");
  ly += lh + gap;
  tft.fillRect(lx, ly, lw, lh, HUM);   tft.setCursor(lx + lw + 6, ly-1); tft.print("RH (%)");
}

void drawAxesAndGrid() {
  // Clear graph area
  tft.fillRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, BG);

  // Comfort/RH horizontal grid (every 10)
  tft.setTextSize(1);
  for (int v = 0; v <= 100; v += 10) {
    int y = mapLinearToY(v, C_MIN, C_MAX);
    tft.drawFastHLine(GRAPH_X, y, GRAPH_W, (v % 20 == 0) ? AXIS : GRID);
    // Left labels every 20
    if (v % 20 == 0) {
      tft.setTextColor(FG);
      tft.setCursor(GRAPH_X + 3, y - 7);
      tft.print(v);
    }
  }

  // Right-side temperature labels every 5°F
  for (int f = (int)TF_MIN; f <= (int)TF_MAX; f += 5) {
    int y = mapLinearToY((float)f, TF_MIN, TF_MAX);
    tft.setTextColor(TEMP);
    tft.setCursor(GRAPH_X + GRAPH_W - 30, y - 7);
    tft.print(f);
  }

  // “Good” bands (outlined to avoid hiding lines)
  // Temp band
  int yTmin = mapLinearToY(GOOD_TF_MIN, TF_MIN, TF_MAX);
  int yTmax = mapLinearToY(GOOD_TF_MAX, TF_MIN, TF_MAX);
  if (yTmax > yTmin) { int tmp = yTmin; yTmin = yTmax; yTmax = tmp; }
  tft.drawRect(GRAPH_X+1, yTmax, GRAPH_W-2, yTmin - yTmax, TEMP);

  // RH band
  int yRHmin = mapLinearToY(GOOD_RH_MIN, RH_MIN, RH_MAX);
  int yRHmax = mapLinearToY(GOOD_RH_MAX, RH_MIN, RH_MAX);
  if (yRHmax > yRHmin) { int tmp = yRHmin; yRHmin = yRHmax; yRHmax = tmp; }
  tft.drawRect(GRAPH_X+1, yRHmax, GRAPH_W-2, yRHmin - yRHmax, HUM);

  // Frame
  tft.drawRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, AXIS);

  // Footer note
  tft.setTextColor(FG);
  tft.setCursor(6, GRAPH_Y + GRAPH_H + 8);
  tft.print("Newest at right; 1 px = 1 sample (10 s)");
}

void drawHeaderReadings() {
  // Wipe line and print current readings
  tft.fillRect(0, 34, 320, 20, BG);
  tft.setCursor(6, 36);
  tft.setTextColor(FG); tft.setTextSize(2);

  tft.print("T: ");
  if (isnan(gTempF)) tft.print("--");
  else { tft.print(gTempF, 1); tft.print("F"); }

  tft.print("  RH: ");
  if (isnan(gRH)) tft.print("--");
  else { tft.print(gRH, 0); tft.print("%"); }

  tft.print("  Comfort: ");
  if (isnan(gComfort)) tft.print("--");
  else tft.print((int)gComfort);
}

void drawSampleColumnAll(int x, uint8_t vComfort, uint8_t vTempF, uint8_t vRH) {
  // Erase this column in graph area
  tft.drawFastVLine(x, GRAPH_Y, GRAPH_H, BG);

  // Plot 3 points (1–3 px thick dots) for each series
  // Comfort (left axis)
  int yC = mapLinearToY((float)vComfort, C_MIN, C_MAX);
  tft.drawPixel(x, yC, COMF);
  if (yC > GRAPH_Y) tft.drawPixel(x, yC-1, COMF); // make it a tad thicker

  // RH (left axis)
  int yH = mapLinearToY((float)vRH, RH_MIN, RH_MAX);
  tft.drawPixel(x, yH, HUM);

  // TempF (right axis scaling)
  int yT = mapLinearToY((float)vTempF, TF_MIN, TF_MAX);
  tft.drawPixel(x, yT, TEMP);
}

void fullRedrawGraph() {
  drawAxesAndGrid();
  for (int i = 0; i < GRAPH_W; i++) {
    int idx = (head + i) % GRAPH_W;
    drawSampleColumnAll(i, bufComfort[idx], bufTempF[idx], bufRH[idx]);
  }
}

void addSample(float tempC, float rh) {
  gTempC = tempC;
  gTempF = c_to_f(tempC);
  gRH = rh;
  gComfort = comfortScoreF(gTempF, gRH);

  uint8_t cVal = (uint8_t)lroundf(fminf(fmaxf(gComfort, 0.0f), 100.0f));
  uint8_t tF   = (uint8_t)lroundf(fminf(fmaxf(gTempF, 0.0f), 255.0f));
  uint8_t rVal = (uint8_t)lroundf(fminf(fmaxf(gRH, 0.0f), 100.0f));

  bufComfort[head] = cVal;
  bufTempF[head]   = tF;
  bufRH[head]      = rVal;
  head = (head + 1) % GRAPH_W;

  // Draw only the newest column for speed
  drawSampleColumnAll(GRAPH_W - 1, cVal, tF, rVal);
  drawHeaderReadings();

  // Shift effect: redraw one column earlier to “scroll”
  // (Cheap trick: redraw whole graph every N samples if you prefer perfectly clean scroll)
  // Here we just rely on drawing the specific x since x==GRAPH_W-1 always used; buffers handle order.
  // If you see smearing after long runs, call fullRedrawGraph() every ~50 samples.
  static uint8_t redrawCtr = 0;
  if (++redrawCtr >= 50) { fullRedrawGraph(); redrawCtr = 0; }

  Serial.print("T(C/F)= ");
  Serial.print(gTempC, 1); Serial.print("/");
  Serial.print(gTempF, 1);
  Serial.print("  RH= "); Serial.print(gRH, 0);
  Serial.print("%  Comfort= "); Serial.println(gComfort, 0);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nOffice Comfort Monitor (Overlay °F, %RH, Comfort)");

  Wire.begin();
  Wire.setClock(100000);

  Modulino.begin();
  bool thermoOK = thermo.begin();
  Serial.print("thermo.begin(): "); Serial.println(thermoOK ? "OK" : "FAIL");

  tft.begin();
  tft.setRotation(1);

  if (!ctp.begin(40)) {
    Serial.println("FT6206 not found (touch disabled).");
  }

  // Seeds
  for (int i = 0; i < GRAPH_W; i++) {
    bufComfort[i] = 50;
    bufTempF[i]   = 72;
    bufRH[i]      = 50;
  }

  tft.fillScreen(BG);
  drawLegend();
  drawAxesAndGrid();
  fullRedrawGraph();
  drawHeaderReadings();

  lastSample = millis() - SAMPLE_MS;
}

void loop() {
  // Tap UL corner to pause/resume sampling
  if (ctp.touched()) {
    TS_Point p = ctp.getPoint();
    if (p.y < 80 && p.x < 80) { // UL corner
      paused = !paused;
      drawLegend();
      delay(250);
    }
  }

  if (!paused && (millis() - lastSample >= SAMPLE_MS)) {
    lastSample += SAMPLE_MS;

    float tC = thermo.getTemperature();
    float rh = thermo.getHumidity();

    if (!isnan(tC) && !isnan(rh)) {
      addSample(tC, rh);
    } else {
      Serial.println("⚠️ NaN from sensor; skipping update");
    }
  }
}
